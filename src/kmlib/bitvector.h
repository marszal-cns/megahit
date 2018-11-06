//
// Created by vout on 28/2/2018.
//

#ifndef KMLIB_ATOMIC_BIT_VECTOR_H
#define KMLIB_ATOMIC_BIT_VECTOR_H

#include <cstdlib>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <exception>
#include <vector>
#include <atomic>

namespace kmlib {
/*!
 * @brief Atomic bit vector: a class that represent a vector of "bits".
 * @details Update of each bit is threads safe via set and get.
 * It can also be used as a vector of bit locks via try_lock, lock and unlock
 */
template<typename WordType = uint64_t>
class AtomicBitVector {
 public:
  /*!
   * @brief Constructor
   * @param size the size (number of bits) of the bit vector
   */
  explicit AtomicBitVector(size_t size = 0, WordType const *src = nullptr)
      : size_(size),
        data_array_((size + kBitsPerWord - 1) / kBitsPerWord, 0) {
    if (src != nullptr) {
      std::copy(src, src + data_array_.size(), data_array_.begin());
    }
  }

  AtomicBitVector(AtomicBitVector &&rhs)
      : size_(rhs.size_), data_array_(std::move(rhs.data_array_)) {}

  AtomicBitVector &operator=(AtomicBitVector &&rhs) {
    size_ = rhs.size_;
    data_array_ = std::move(rhs.data_array_);
    return *this;
  }

  AtomicBitVector& FromPtr(size_t size, WordType const *src) {
    size_ = size;
    data_array_.resize((size + kBitsPerWord - 1) / kBitsPerWord);
    std::copy(src, src + data_array_.size(), data_array_.begin());
  }
  ~AtomicBitVector() = default;

  /*!
   * @return the size of the bit vector
   */
  size_t size() {
    return size_;
  }

  /*!
   * @brief set the i-th bit to 1
   * @param i the index of the bit to be set to 1
   */
  void set(size_t i) {
    WordType mask = WordType(1) << (i % kBitsPerWord);
    data_array_[i / kBitsPerWord].v.fetch_or(mask, std::memory_order_release);
  }

  /*!
   * @brief set the i-th bit to 0
   * @param i the index of the bit to be set to 0
   */
  void unset(size_t i) {
    WordType mask = ~(WordType(1) << (i % kBitsPerWord));
    data_array_[i / kBitsPerWord].v.fetch_and(mask, std::memory_order_release);
  }

  /*!
   * @param i the index of the bit
   * @return value of the i-th bit
   */
  bool get(size_t i) const {
    return (data_array_[i / kBitsPerWord].v.load(std::memory_order_acquire)
        >> i % kBitsPerWord) & 1;
  }

  /*!
   * @param i the index of the bit
   * @return whether the i-th bit has been locked successfully
   */
  bool try_lock(size_t i) {
    auto p = data_array_.begin() + i / kBitsPerWord;
    WordType old_value = p->v.load(std::memory_order_acquire);
    while (!((old_value >> i % kBitsPerWord) & 1)) {
      WordType new_value = old_value | (WordType(1) << (i % kBitsPerWord));
      if (p->v.compare_exchange_weak(old_value,
                                     new_value,
                                     std::memory_order_release)) {
        return true;
      }
    }
    return false;
  }

  /*!
   * @brief lock the i-th bit
   * @param i the bit to lock
   */
  void lock(size_t i) {
    while (!try_lock(i)) {
      continue;
    }
  }

  /*!
   * @brief unlock the i-th bit
   * @param i the index of the bits
   */
  void unlock(size_t i) {
    unset(i);
  }

  /*!
   * @brief reset the size of the bit vector and clear all bits
   * @param size the new size of the bit vector
   */
  void reset(size_t size) {
    data_array_ = std::move(ArrayType(0)); // clear memory
    size_ = size;
    data_array_ = ArrayType((size + kBitsPerWord - 1) / kBitsPerWord, 0);
  }

  /*!
   * @brief swap with another bit vector
   * @param rhs the target to swap
   */
  void swap(AtomicBitVector &rhs) {
    std::swap(size_, rhs.size_);
    std::swap(data_array_, rhs.data_array_);
  }

 private:
  /*!
   * @brief a wrapper for std::Atomic. std::Atomic do not support copy and move
   * constructor, so this wrapper is used to make suitable to std::vector
   * @tparam T the underlying type of the atomic struct
   */
  template<typename T>
  struct AtomicWrapper {
    std::atomic<T> v;
    AtomicWrapper(T a = T()) : v(a) {}
    AtomicWrapper(const AtomicWrapper &rhs) : v(rhs.v.load()) {}
    AtomicWrapper &operator=(const AtomicWrapper &rhs) {
      v.store(rhs.v.load());
      return *this;
    }
  };

  using ArrayType = std::vector<AtomicWrapper<WordType>>;
  static const unsigned kBitsPerByte = 8;
  static const unsigned kBitsPerWord = sizeof(WordType) * kBitsPerByte;
  size_t size_;
  ArrayType data_array_;

  static_assert(sizeof(AtomicWrapper<WordType>) == sizeof(WordType), "");
};

} // namespace kmlib

using AtomicBitVector = kmlib::AtomicBitVector<uint64_t>;

#endif //KMLIB_ATOMIC_BIT_VECTOR_H