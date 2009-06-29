// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_REF_COUNTED_UTIL_H__
#define CHROME_COMMON_REF_COUNTED_UTIL_H__

#include "base/ref_counted.h"
#include <vector>

// RefCountedVector is just a vector wrapped up with
// RefCountedThreadSafe.
template<class T>
class RefCountedVector :
    public base::RefCountedThreadSafe<RefCountedVector<T> > {
 public:
  RefCountedVector() {}
  RefCountedVector(const std::vector<T>& initializer)
      : data(initializer) {}

  std::vector<T> data;

  DISALLOW_EVIL_CONSTRUCTORS(RefCountedVector<T>);
};

// RefCountedThreadSafeBytes represent a ref-counted blob of bytes.
// Useful for passing data between threads without copying.
typedef RefCountedVector<unsigned char> RefCountedBytes;

#endif  // CHROME_COMMON_REF_COUNTED_UTIL_H__
