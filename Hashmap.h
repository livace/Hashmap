//
// Created by Alexey Ilyukhov on 2019-02-12.
//

#ifndef COMPETITIVE_HASHMAP_H
#define COMPETITIVE_HASHMAP_H

#include <vector>
#include <iostream>
#include <stdexcept>

template<class KeyType, class ValueType, class Hash = std::hash<KeyType>>
class HashMap {
public:
  constexpr static size_t MIN_SIZE_TO_HALVE = 5;

  typedef std::pair<const KeyType, ValueType> HashMapItem;
  typedef std::vector<HashMapItem> HashMapBatch;
  typedef std::vector<HashMapBatch> HashMapData;

  class iterator {
  public:
    iterator() : data_(nullptr), index_(0), position_(0) {}

    iterator(HashMapData *data, size_t index, size_t position)
            : data_(data), index_(index), position_(position) {}

    bool operator==(const iterator &other) const {
      return data_ == other.data_ && index_ == other.index_ && position_ == other.position_;
    }

    bool operator!=(const iterator &other) const {
      return !((*this) == other);
    }

    iterator& operator++() {
      ++position_;

      if (position_ == (*data_)[index_].size()) {
        position_ = 0;
        ++index_;
        while (index_ < data_->size() && (*data_)[index_].size() == 0) {
          ++index_;
        }
      }
      return *this;
    }

    const iterator operator++(int) {
      const iterator current = (*this);
      ++(*this);
      return current;
    }

    HashMapItem* operator->() {
      return ((*data_)[index_].begin() + position_).operator->();
    }

    HashMapItem& operator*() {
      return ((*data_)[index_].begin() + position_).operator*();
    }

  private:
    HashMapData *data_;
    size_t index_;
    size_t position_;
  };

  class const_iterator {
  public:
    const_iterator() : data_(nullptr), index_(0), position_(0) {}

    const_iterator(const HashMapData *data, size_t index, size_t position)
            : data_(data), index_(index), position_(position) {}

    bool operator==(const const_iterator &other) const {
      return data_ == other.data_ && index_ == other.index_ && position_ == other.position_;
    }

    bool operator!=(const const_iterator &other) const {
      return !((*this) == other);
    }

    const_iterator& operator++() {
      ++position_;
      if (position_ == (*data_)[index_].size()) {
        position_ = 0;
        ++index_;
        while (index_ < data_->size() && (*data_)[index_].size() == 0) {
          ++index_;
        }
      }
      return *this;
    }

    const const_iterator operator++(int) {
      const const_iterator current = (*this);
      ++(*this);
      return current;
    }

    const HashMapItem* operator->() const {
      return ((*data_)[index_].cbegin() + position_).operator->();
    }

    const std::pair<const KeyType, ValueType>& operator*() const {
      return ((*data_)[index_].cbegin() + position_).operator*();
    }

  private:
    const HashMapData *data_;
    size_t index_;
    size_t position_;
  };

  HashMap(const HashMap<KeyType, ValueType, Hash> &other) : HashMap(other.begin(), other.end(), other.hash_) {}

  HashMap& operator=(const HashMap<KeyType, ValueType, Hash> &other) {
    size_ = other.size_;
    hash_ = other.hash_;
    batches_ = other.batches_;

    HashMapData new_data(batches_);

    for (const auto &item : other) {
      auto key = item.first;
      auto value = item.second;
      new_data[getHash(key)].emplace_back(key, value);
    }

    std::swap(data_, new_data);

    return (*this);
  }

  explicit HashMap(Hash hash = Hash()) : hash_(hash), size_(0), batches_(1) {
    data_.resize(batches_);
  }

  template<typename Iterator>
  HashMap(Iterator begin, Iterator end, Hash hash = Hash()) : HashMap(hash) {
    while (begin != end) {
      insert(*begin);
      ++begin;
    }
  }

  explicit HashMap(const std::vector<HashMapItem> &data, Hash hash = Hash())
          : HashMap(data.begin(), data.end(), hash) {}

  HashMap(std::initializer_list<HashMapItem> list, Hash hash = Hash())
          : HashMap(HashMapBatch(list), hash) {
  }

  size_t size() const {
    return size_;
  }

  bool empty() const {
    return size() == 0;
  }

  Hash hash_function() const {
    return hash_;
  }

  iterator insert(HashMapItem item) {
    auto key = item.first;
    iterator found_element = find(key);

    if (found_element == end()) {
      double_batches_if_needed();
      size_t hash = getHash(key);
      ++size_;
      data_[hash].push_back(item);
      return iterator(&data_, hash, data_[hash].size() - 1);
    } else {
      return found_element;
    }
  }

  void erase(KeyType key) {
    halve_batches_if_needed();

    size_t hash = getHash(key);

    HashMapBatch &batch = data_[hash];

    size_t found_position;
    bool found = false;

    for (size_t i = 0; i < batch.size(); i++) {
      if (batch[i].first == key) {
        found = true;
        found_position = i;
        break;
      }
    }

    if (found) {
      HashMapBatch new_data;
      new_data.reserve(batch.size() - 1);
      for (size_t j = 0; j < batch.size(); j++) {
        if (j != found_position) {
          new_data.push_back(batch[j]);
        }
      }
      swap(batch, new_data);
      --size_;
    }
  }

  iterator begin() {
    for (size_t i = 0; i < batches_; i++) {
      if (!data_[i].empty()) {
        return iterator(&data_, i, 0);
      }
    }
    return end();
  }

  const_iterator begin() const {
    for (size_t i = 0; i < batches_; i++) {
      if (!data_[i].empty()) {
        return const_iterator(&data_, i, 0);
      }
    }
    return end();
  }

  iterator end() {
    return iterator(&data_, batches_, 0);
  }

  const_iterator end() const {
    return const_iterator(&data_, batches_, 0);
  }

  ValueType& operator[](const KeyType &key) {
    return insert({key, ValueType()})->second;
  }

  const ValueType &at(const KeyType &key) const {
    auto result = find(key);
    if (result == end()) {
      throw std::out_of_range("Key out of range");
    } else {
      return result->second;
    }
  }

  void clear() {
    HashMapData new_data(1);
    size_ = 0;
    batches_ = 1;
    std::swap(data_, new_data);
  }

  const_iterator find(const KeyType &key) const {
    size_t hash = getHash(key);

    const HashMapBatch &batch = data_[hash];

    for (size_t position = 0; position < batch.size(); position++) {
      if (batch[position].first == key) {
        return const_iterator(&data_, hash, position);
      }
    }

    return end();
  }

  iterator find(const KeyType &key) {
    size_t hash = getHash(key);

    const HashMapBatch &batch = data_[hash];

    for (size_t position = 0; position < batch.size(); position++) {
      if (batch[position].first == key) {
        return iterator(&data_, hash, position);
      }
    }

    return end();
  }

private:
  HashMapData data_;

  size_t size_;
  size_t batches_;
  Hash hash_;

  size_t getHash(KeyType key, size_t batches = 0) const {
    if (batches == 0) {
      batches = batches_;
    }
    return hash_(key) % batches;
  }

  void rehash(size_t new_batches) {
    HashMapData new_data(new_batches);

    for (const auto &batch : data_) {
      for (const auto &item : batch) {
        auto key = item.first;
        auto value = item.second;
        new_data[getHash(key, new_batches)].emplace_back(key, value);
      }
    }

    batches_ = new_batches;
    std::swap(data_, new_data);
  }

  void double_batches_if_needed() {
    if (batches_ < size_) {
      rehash(batches_ * 2);
    }
  }

  void halve_batches_if_needed() {
    if (batches_ > MIN_SIZE_TO_HALVE && batches_ > 2 * size_) {
      rehash(batches_ / 2);
    }
  }
};


#endif //COMPETITIVE_HASHMAP_H
