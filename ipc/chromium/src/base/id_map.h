/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ID_MAP_H__
#define BASE_ID_MAP_H__

#include "base/logging.h"

#include "nsDataHashtable.h"
#include "nsHashKeys.h"

// This object maintains a list of IDs that can be quickly converted to
// objects.
//
// Items can be inserted into the container with arbitrary ID, but the caller
// must ensure they are unique. Inserting IDs and relying on automatically
// generated ones is not allowed because they can collide.
template <class T>
class IDMap {
 private:
  using HashTable = nsDataHashtable<nsUint32HashKey, T>;

 public:
  IDMap() {}
  IDMap(const IDMap& other) : data_(other.data_) {}

  using const_iterator = typename HashTable::const_iterator;

  const_iterator begin() const { return data_.begin(); }
  const_iterator end() const { return data_.end(); }

  bool Contains(int32_t id) { return data_.Contains(id); }

  void Put(int32_t id, const T& data) { data_.Put(id, data); }

  void Remove(int32_t id) { data_.Remove(id); }

  void Clear() { data_.Clear(); }

  T Get(int32_t id) const { return data_.Get(id); }

 protected:
  HashTable data_;
};

#endif  // BASE_ID_MAP_H__
