/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ID_MAP_H__
#define BASE_ID_MAP_H__

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"

// This object maintains a list of IDs that can be quickly converted to
// pointers to objects. It is implemented as a hash table, optimized for
// relatively small data sets (in the common case, there will be exactly one
// item in the list).
//
// Items can be inserted into the container with arbitrary ID, but the caller
// must ensure they are unique. Inserting IDs and relying on automatically
// generated ones is not allowed because they can collide.
template<class T>
class IDMap {
 private:
  typedef base::hash_map<int32_t, T*> HashTable;
  typedef typename HashTable::iterator iterator;

 public:
  // support const iterators over the items
  // Note, use iterator->first to get the ID, iterator->second to get the T*
  typedef typename HashTable::const_iterator const_iterator;

  IDMap() : next_id_(1) {
  }
  IDMap(const IDMap& other) : next_id_(other.next_id_),
                                        data_(other.data_) {
  }

  const_iterator begin() const {
    return data_.begin();
  }
  const_iterator end() const {
    return data_.end();
  }

  // Adds a view with an automatically generated unique ID. See AddWithID.
  int32_t Add(T* data) {
    int32_t this_id = next_id_;
    DCHECK(data_.find(this_id) == data_.end()) << "Inserting duplicate item";
    data_[this_id] = data;
    next_id_++;
    return this_id;
  }

  // Adds a new data member with the specified ID. The ID must not be in
  // the list. The caller either must generate all unique IDs itself and use
  // this function, or allow this object to generate IDs and call Add. These
  // two methods may not be mixed, or duplicate IDs may be generated
  void AddWithID(T* data, int32_t id) {
    DCHECK(data_.find(id) == data_.end()) << "Inserting duplicate item";
    data_[id] = data;
  }

  void Remove(int32_t id) {
    iterator i = data_.find(id);
    if (i == data_.end()) {
      NOTREACHED() << "Attempting to remove an item not in the list";
      return;
    }
    data_.erase(i);
  }

  bool IsEmpty() const {
    return data_.empty();
  }

  void Clear() {
    data_.clear();
  }

  bool HasData(const T* data) const {
    // XXX would like to use <algorithm> here ...
    for (const_iterator it = begin(); it != end(); ++it)
      if (data == it->second)
        return true;
    return false;
  }

  T* Lookup(int32_t id) const {
    const_iterator i = data_.find(id);
    if (i == data_.end())
      return NULL;
    return i->second;
  }

  size_t size() const {
    return data_.size();
  }

 protected:
  // The next ID that we will return from Add()
  int32_t next_id_;

  HashTable data_;
};

#endif  // BASE_ID_MAP_H__
