#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept
        : buffer_(other.buffer_)
        , capacity_(other.capacity_)
    {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory& operator=(RawMemory&& rhs) noexcept { 
        if (this != &rhs) {
            Deallocate(buffer_);
            buffer_ = rhs.buffer_;
            capacity_ = rhs.capacity_;

            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return cbegin();
    }

    const_iterator end() const noexcept {
        return cend();
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept 
        : data_(std::move(other.data_))
        , size_(other.size_) {
        other.size_ = 0;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                if (rhs.size_ < size_) {
                    for (size_t i = 0; i < rhs.size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                } else {
                    for (size_t i = 0; i < size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            size_ = rhs.size_;
            rhs.size_ = 0;
        }
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        UninitializedMoveOrCopyN(data_.GetAddress(), size_, new_data.GetAddress());
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        /* возвращенное значение не нужно*/ EmplaceBackImpl(value);
    }

    void PushBack(T&& value) {
        /* возвращенное значение не нужно*/ EmplaceBackImpl(std::move(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return EmplaceBackImpl(std::forward<Args>(args)...);
    }

    void PopBack() noexcept {
        assert(size_ != 0);
        std::destroy_at(end() - 1);
        --size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= begin() && pos <= end());
        T* pos_it = begin() + (pos - begin());
        if (size_ < data_.Capacity()) {
            if (pos == end()) {
                new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
            } else {
                T copy{std::forward<Args>(args)...};
                new (begin() + size_) T(std::move(data_[size_ - 1]));
                try {
                    std::move_backward(pos_it, end() - 1, end());
                } catch (...) {
                    std::destroy_at(begin() + size_);
                    throw;
                }
                *pos_it = std::move(copy);
            }
        } else {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            T* new_pos_it = new (new_data.GetAddress() + (pos - begin())) T(std::forward<Args>(args)...);
            try {
                UninitializedMoveOrCopy(begin(), pos_it, new_data.GetAddress());
            } catch (...) {
                std::destroy_at(new_pos_it);
                throw;
            }
            try {
                UninitializedMoveOrCopy(pos_it, end(), new_data.GetAddress() + (pos_it - begin()) + 1);
            } catch (...) {
                std::destroy(new_data.GetAddress(), new_pos_it + 1);
                throw;
            }
            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
            pos_it = new_pos_it;
        }
        ++size_;
        return pos_it;
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        assert(pos >= begin() && pos < end());
        size_t diff = pos - begin();
        T* pos_it = begin() + diff;
        if (pos != end() - 1) {
            std::move(pos_it + 1, end(), pos_it);
        }
        PopBack();
        return begin() + diff;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    template <typename InputIt, typename OutputIt>
    void UninitializedMoveOrCopy(InputIt first, InputIt last, OutputIt d_first) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move(first, last, d_first);
        } else {
            std::uninitialized_copy(first, last, d_first);
        }
    }

    template <typename InputIt, typename OutputIt>
    void UninitializedMoveOrCopyN(InputIt first, size_t size, OutputIt d_first) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(first, size, d_first);
        } else {
            std::uninitialized_copy_n(first, size, d_first);
        }
    }

    template <typename... Args>
    T& EmplaceBackImpl(Args&&... args) {
        if (size_ < data_.Capacity()) {
            new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        } else {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);
            try {
                UninitializedMoveOrCopyN(data_.GetAddress(), size_, new_data.GetAddress());
            } catch (...) {
                std::destroy_at(new_data.GetAddress() + size_);
                throw;
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
        return data_[size_ - 1];
    }
};