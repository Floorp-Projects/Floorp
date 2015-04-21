/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_NUMERICTOOLS_H_
#define MOZILLA_GFX_NUMERICTOOLS_H_

namespace mozilla {

// XXX - Move these into mfbt/MathAlgorithms.h?

// Returns the largest multiple of aMultiplied that's <= x.
// Same as int32_t(floor(double(x) / aMultiplier)) * aMultiplier,
// but faster.
inline int32_t
RoundDownToMultiple(int32_t x, int32_t aMultiplier)
{
  // We don't use float division + floor because that's hard for the compiler
  // to optimize.
  int mod = x % aMultiplier;
  if (x > 0) {
    return x - mod;
  }
  return mod ? x - aMultiplier - mod : x;
}

// Returns the smallest multiple of aMultiplied that's >= x.
// Same as int32_t(ceil(double(x) / aMultiplier)) * aMultiplier,
// but faster.
inline int32_t
RoundUpToMultiple(int32_t x, int32_t aMultiplier)
{
  int mod = x % aMultiplier;
  if (x > 0) {
    return mod ? x + aMultiplier - mod : x;
  }
  return x - mod;
}

} // namespace mozilla

#endif /* MOZILLA_GFX_NUMERICTOOLS_H_ */
