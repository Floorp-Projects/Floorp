// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RAND_UTIL_H_
#define BASE_RAND_UTIL_H_

#include "base/basictypes.h"

namespace base {

// Returns a random number in range [0, kuint64max]. Thread-safe.
uint64_t RandUint64();

// Returns a random number between min and max (inclusive). Thread-safe.
int RandInt(int min, int max);

// Returns a random double in range [0, 1). Thread-safe.
double RandDouble();

}  // namespace base

#endif // BASE_RAND_UTIL_H_
