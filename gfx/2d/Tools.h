/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TOOLS_H_
#define MOZILLA_GFX_TOOLS_H_

#include "mozilla/CheckedInt.h"
#include "mozilla/TypeTraits.h"
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
  case CompositionOp::OP_IN:
  case CompositionOp::OP_OUT:
  case CompositionOp::OP_DEST_IN:
  case CompositionOp::OP_DEST_ATOP:
  case CompositionOp::OP_SOURCE:
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

static inline void
NudgeToInteger(float *aVal, float aErr)
{
  float r = floorf(*aVal + 0.5f);
  if (FuzzyEqual(r, *aVal, aErr)) {
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
  case SurfaceFormat::A8:
    return 1;
  case SurfaceFormat::R5G6B5:
    return 2;
  default:
    return 4;
  }
}

template<typename T, int alignment = 16>
struct AlignedArray
{
  typedef T value_type;

  AlignedArray()
    : mPtr(nullptr)
    , mStorage(nullptr)
  {
  }

  MOZ_ALWAYS_INLINE AlignedArray(size_t aCount)
    : mStorage(nullptr)
    , mCount(0)
  {
    Realloc(aCount);
  }

  MOZ_ALWAYS_INLINE ~AlignedArray()
  {
    Dealloc();
  }

  void Dealloc()
  {
    // If we fail this assert we'll need to uncomment the loop below to make
    // sure dtors are properly invoked. If we do that, we should check that the
    // comment about compiler dead code elimination is in fact true for all the
    // compilers that we care about.
    static_assert(mozilla::IsPod<T>::value,
                  "Destructors must be invoked for this type");
#if 0
    for (size_t i = 0; i < mCount; ++i) {
      // Since we used the placement |operator new| function to construct the
      // elements of this array we need to invoke their destructors manually.
      // For types where the destructor does nothing the compiler's dead code
      // elimination step should optimize this loop away.
      mPtr[i].~T();
    }
#endif

    delete [] mStorage;
    mStorage = nullptr;
    mPtr = nullptr;
  }

  MOZ_ALWAYS_INLINE void Realloc(size_t aCount)
  {
    delete [] mStorage;
    CheckedInt32 storageByteCount =
      CheckedInt32(sizeof(T)) * aCount + (alignment - 1);
    if (!storageByteCount.isValid()) {
      mStorage = nullptr;
      mPtr = nullptr;
      mCount = 0;
      return;
    }
    // We don't create an array of T here, since we don't want ctors to be
    // invoked at the wrong places if we realign below.
    mStorage = new (std::nothrow) uint8_t[storageByteCount.value()];
    if (uintptr_t(mStorage) % alignment) {
      // Our storage does not start at a <alignment>-byte boundary. Make sure mPtr does!
      mPtr = (T*)(uintptr_t(mStorage) + alignment - (uintptr_t(mStorage) % alignment));
    } else {
      mPtr = (T*)(mStorage);
    }
    // Now that mPtr is pointing to the aligned position we can use placement
    // |operator new| to invoke any ctors at the correct positions. For types
    // that have a no-op default constructor the compiler's dead code
    // elimination step should optimize this away.
    mPtr = new (mPtr) T[aCount];
    mCount = aCount;
  }

  MOZ_ALWAYS_INLINE operator T*()
  {
    return mPtr;
  }

  T *mPtr;

private:
  uint8_t *mStorage;
  size_t mCount;
};

/**
 * Returns aStride increased, if necessary, so that it divides exactly into
 * |alignment|.
 *
 * Note that currently |alignment| must be a power-of-2. If for some reason we
 * want to support NPOT alignment we can revert back to this functions old
 * implementation.
 */
template<int alignment>
int32_t GetAlignedStride(int32_t aStride)
{
  static_assert(alignment > 0 && (alignment & (alignment-1)) == 0,
                "This implementation currently require power-of-two alignment");
  const int32_t mask = alignment - 1;
  return (aStride + mask) & ~mask;
}

}
}

#endif /* MOZILLA_GFX_TOOLS_H_ */
