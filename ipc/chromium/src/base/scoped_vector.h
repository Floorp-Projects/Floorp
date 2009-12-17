// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_VECTOR_H_
#define BASE_SCOPED_VECTOR_H_

#include <vector>

#include "base/logging.h"
#include "base/stl_util-inl.h"

// ScopedVector wraps a vector deleting the elements from its
// destructor.
template <class T>
class ScopedVector {
 public:
  typedef typename std::vector<T*>::iterator iterator;
  typedef typename std::vector<T*>::const_iterator const_iterator;

  ScopedVector() {}
  ~ScopedVector() { reset(); }

  std::vector<T*>* operator->() { return &v; }
  const std::vector<T*>* operator->() const { return &v; }
  T* operator[](size_t i) { return v[i]; }
  const T* operator[](size_t i) const { return v[i]; }

  bool empty() const { return v.empty(); }
  size_t size() const { return v.size(); }

  iterator begin() { return v.begin(); }
  const_iterator begin() const { return v.begin(); }
  iterator end() { return v.end(); }
  const_iterator end() const { return v.end(); }

  void push_back(T* elem) { v.push_back(elem); }

  std::vector<T*>& get() { return v; }
  const std::vector<T*>& get() const { return v; }
  void swap(ScopedVector<T>& other) { v.swap(other.v); }
  void release(std::vector<T*>* out) { out->swap(v); v.clear(); }

  void reset() { STLDeleteElements(&v); }

 private:
  std::vector<T*> v;

  DISALLOW_COPY_AND_ASSIGN(ScopedVector);
};

#endif // BASE_SCOPED_VECTOR_H_
