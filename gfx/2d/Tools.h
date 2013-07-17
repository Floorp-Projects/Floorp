/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TOOLS_H_
#define MOZILLA_GFX_TOOLS_H_

#include "Types.h"
#include "Point.h"
#include <math.h>
#if defined(_MSC_VER) && (_MSC_VER < 1600)
#define hypotf _hypotf
#endif

namespace mozilla {
namespace gfx {

static inline bool
IsOperatorBoundByMask(CompositionOp aOp) {
  switch (aOp) {
  case OP_IN:
  case OP_OUT:
  case OP_DEST_IN:
  case OP_DEST_ATOP:
  case OP_SOURCE:
    return false;
  default:
    return true;
  }
}

template <class T>
struct ClassStorage
{
  char bytes[sizeof(T)];

  const T *addr() const { return (const T *)bytes; }
  T *addr() { return (T *)(void *)bytes; }
};

static inline bool
FuzzyEqual(Float aA, Float aB, Float aErr)
{
  if ((aA + aErr >= aB) && (aA - aErr <= aB)) {
    return true;
  }
  return false;
}

static inline void
NudgeToInteger(float *aVal)
{
  float r = floorf(*aVal + 0.5f);
  // The error threshold should be proportional to the rounded value. This
  // bounds the relative error introduced by the nudge operation. However,
  // when the rounded value is 0, the error threshold can't be proportional
  // to the rounded value (we'd never round), so we just choose the same
  // threshold as for a rounded value of 1.
  if (FuzzyEqual(r, *aVal, r == 0.0f ? 1e-6f : fabs(r*1e-6f))) {
    *aVal = r;
  }
}

static inline Float
Distance(Point aA, Point aB)
{
  return hypotf(aB.x - aA.x, aB.y - aA.y);
}

static inline int
BytesPerPixel(SurfaceFormat aFormat)
{
  switch (aFormat) {
  case FORMAT_A8:
    return 1;
  case FORMAT_R5G6B5:
    return 2;
  default:
    return 4;
  }
}

template<typename T, int alignment = 16>
struct AlignedArray
{
  AlignedArray()
    : mStorage(nullptr)
    , mPtr(nullptr)
  {
  }

  MOZ_ALWAYS_INLINE AlignedArray(size_t aSize)
    : mStorage(nullptr)
  {
    Realloc(aSize);
  }

  MOZ_ALWAYS_INLINE ~AlignedArray()
  {
    delete [] mStorage;
  }

  void Dealloc()
  {
    delete [] mStorage;
    mStorage = mPtr = nullptr;
  }

  MOZ_ALWAYS_INLINE void Realloc(size_t aSize)
  {
    delete [] mStorage;
    mStorage = new (std::nothrow) T[aSize + (alignment - 1)];
    if (uintptr_t(mStorage) % alignment) {
      // Our storage does not start at a <alignment>-byte boundary. Make sure mData does!
      mPtr = (T*)(uintptr_t(mStorage) +
        (alignment - (uintptr_t(mStorage) % alignment)));
    } else {
      mPtr = mStorage;
    }
  }

  MOZ_ALWAYS_INLINE operator T*()
  {
    return mPtr;
  }

  T *mStorage;
  T *mPtr;
};

template<int alignment>
int32_t GetAlignedStride(int32_t aStride)
{
  if (aStride % alignment) {
    return aStride + (alignment - (aStride % alignment));
  }

  return aStride;
}

}
}

#endif /* MOZILLA_GFX_TOOLS_H_ */
