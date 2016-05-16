/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <math.h>

#include <limits>

#include "base/basictypes.h"
#include "base/logging.h"

namespace base {

int RandInt(int min, int max) {
  DCHECK(min <= max);

  uint64_t range = static_cast<int64_t>(max) - min + 1;
  uint64_t number = base::RandUint64();
  int result = min + static_cast<int>(number % range);
  DCHECK(result >= min && result <= max);
  return result;
}

double RandDouble() {
  // We try to get maximum precision by masking out as many bits as will fit
  // in the target type's mantissa, and raising it to an appropriate power to
  // produce output in the range [0, 1).  For IEEE 754 doubles, the mantissa
  // is expected to accommodate 53 bits.

  COMPILE_ASSERT(std::numeric_limits<double>::radix == 2, otherwise_use_scalbn);
  static const int kBits = std::numeric_limits<double>::digits;
  uint64_t random_bits = base::RandUint64() & ((GG_UINT64_C(1) << kBits) - 1);
  double result = ldexp(static_cast<double>(random_bits), -1 * kBits);
  DCHECK(result >= 0.0 && result < 1.0);
  return result;
}

}  // namespace base
