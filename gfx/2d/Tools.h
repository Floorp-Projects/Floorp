/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TOOLS_H_
#define MOZILLA_GFX_TOOLS_H_

#include <math.h>

#include <utility>

#include "Point.h"
#include "Types.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/MemoryReporting.h"  // for MallocSizeOf
#include "mozilla/TypeTraits.h"

namespace mozilla {
namespace gfx {

static inline bool IsOperatorBoundByMask(CompositionOp aOp) {
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
struct ClassStorage {
  char bytes[sizeof(T)];

  const T* addr() const { return (const T*)bytes; }
  T* addr() { return (T*)(void*)bytes; }
};

static inline bool FuzzyEqual(Float aA, Float aB, Float aErr) {
  if ((aA + aErr >= aB) && (aA - aErr <= aB)) {
    return true;
  }
  return false;
}

static inline void NudgeToInteger(float* aVal) {
  float r = floorf(*aVal + 0.5f);
  // The error threshold should be proportional to the rounded value. This
  // bounds the relative error introduced by the nudge operation. However,
  // when the rounded value is 0, the error threshold can't be proportional
  // to the rounded value (we'd never round), so we just choose the same
  // threshold as for a rounded value of 1.
  if (FuzzyEqual(r, *aVal, r == 0.0f ? 1e-6f : fabs(r * 1e-6f))) {
    *aVal = r;
  }
}

static inline void NudgeToInteger(float* aVal, float aErr) {
  float r = floorf(*aVal + 0.5f);
  if (FuzzyEqual(r, *aVal, aErr)) {
    *aVal = r;
  }
}

static inline void NudgeToInteger(double* aVal) {
  float f = float(*aVal);
  NudgeToInteger(&f);
  *aVal = f;
}

static inline Float Distance(Point aA, Point aB) {
  return hypotf(aB.x - aA.x, aB.y - aA.y);
}

template <typename T, int alignment = 16>
struct AlignedArray final {
  typedef T value_type;

  AlignedArray() : mPtr(nullptr), mStorage(nullptr), mCount(0) {}

  explicit MOZ_ALWAYS_INLINE AlignedArray(size_t aCount, bool aZero = false)
      : mPtr(nullptr), mStorage(nullptr), mCount(0) {
    Realloc(aCount, aZero);
  }

  MOZ_ALWAYS_INLINE ~AlignedArray() { Dealloc(); }

  void Dealloc() {
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

    free(mStorage);
    mStorage = nullptr;
    mPtr = nullptr;
  }

  MOZ_ALWAYS_INLINE void Realloc(size_t aCount, bool aZero = false) {
    free(mStorage);
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
    if (aZero) {
      // calloc can be more efficient than new[] for large chunks,
      // so we use calloc/malloc/free for everything.
      mStorage = static_cast<uint8_t*>(calloc(1u, storageByteCount.value()));
    } else {
      mStorage = static_cast<uint8_t*>(malloc(storageByteCount.value()));
    }
    if (!mStorage) {
      mStorage = nullptr;
      mPtr = nullptr;
      mCount = 0;
      return;
    }
    if (uintptr_t(mStorage) % alignment) {
      // Our storage does not start at a <alignment>-byte boundary. Make sure
      // mPtr does!
      mPtr = (T*)(uintptr_t(mStorage) + alignment -
                  (uintptr_t(mStorage) % alignment));
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

  void Swap(AlignedArray<T, alignment>& aOther) {
    std::swap(mPtr, aOther.mPtr);
    std::swap(mStorage, aOther.mStorage);
    std::swap(mCount, aOther.mCount);
  }

  size_t HeapSizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(mStorage);
  }

  MOZ_ALWAYS_INLINE operator T*() { return mPtr; }

  T* mPtr;

 private:
  uint8_t* mStorage;
  size_t mCount;
};

/**
 * Returns aWidth * aBytesPerPixel increased, if necessary, so that it divides
 * exactly into |alignment|.
 *
 * Note that currently |alignment| must be a power-of-2. If for some reason we
 * want to support NPOT alignment we can revert back to this functions old
 * implementation.
 */
template <int alignment>
int32_t GetAlignedStride(int32_t aWidth, int32_t aBytesPerPixel) {
  static_assert(alignment > 0 && (alignment & (alignment - 1)) == 0,
                "This implementation currently require power-of-two alignment");
  const int32_t mask = alignment - 1;
  CheckedInt32 stride =
      CheckedInt32(aWidth) * CheckedInt32(aBytesPerPixel) + CheckedInt32(mask);
  if (stride.isValid()) {
    return stride.value() & ~mask;
  }
  return 0;
}

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_TOOLS_H_ */
