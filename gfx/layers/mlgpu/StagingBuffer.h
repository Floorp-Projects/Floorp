/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_StagingBuffer_h
#define mozilla_gfx_layers_mlgpu_StagingBuffer_h

#include "mozilla/MathAlgorithms.h"
#include "mozilla/UniquePtr.h"
#include "UtilityMLGPU.h"
#include <algorithm>
#include <utility>
#include <limits.h>

namespace mozilla {
namespace layers {

class MLGDevice;

// A StagingBuffer is a writable memory buffer for arbitrary contents.
template <size_t Alignment = 0>
class StagingBuffer
{
public:
  StagingBuffer()
   : StagingBuffer(0)
  {}

  // By default, staging buffers operate in "forward" mode: items are added to
  // the end of the buffer. In "reverse" mode the cursor is at the end of the
  // buffer, and items are added to the beginning.
  //
  // This must be called before the buffer is written.
  void SetReversed() {
    MOZ_ASSERT(IsEmpty());
    mReversed = true;
  }

  // Write a series of components as a single item. When this is first used, the
  // buffer records the initial item size and requires that all future items be
  // the exact same size.
  //
  // This directs to either AppendItem or PrependItem depending on the buffer
  // state.
  template <typename T>
  bool AddItem(const T& aItem) {
    if (mReversed) {
      return PrependItem(aItem);
    }
    return AppendItem(aItem);
  }

  // Helper for adding a single item as two components.
  template <typename T1, typename T2>
  bool AddItem(const T1& aItem1, const T2& aItem2) {
    if (mReversed) {
      return PrependItem(aItem1, aItem2);
    }
    return AppendItem(aItem1, aItem2);
  }

  // This may only be called on forward buffers.
  template <typename T>
  bool AppendItem(const T& aItem) {
    MOZ_ASSERT(!mReversed);

    size_t alignedBytes = AlignUp<Alignment>::calc(sizeof(aItem));
    if (!mUniformSize) {
      mUniformSize = alignedBytes;
    }

    if (!EnsureForwardRoomFor(alignedBytes)) {
      return false;
    }
    if (mUniformSize != alignedBytes) {
      MOZ_ASSERT_UNREACHABLE("item of incorrect size added!");
      return false;
    }

    *reinterpret_cast<T*>(mPos) = aItem;
    mPos += alignedBytes;
    MOZ_ASSERT(mPos <= mEnd);

    mNumItems++;
    return true;
  }

  // Append an item in two stages.
  template <typename T1, typename T2>
  bool AppendItem(const T1& aFirst, const T2& aSecond) {
    struct Combined {
      T1 first;
      T2 second;
    } value = { aFirst, aSecond };

    // The combined value must be packed.
    static_assert(sizeof(value) == sizeof(aFirst) + sizeof(aSecond),
                  "Items must be packed within struct");
    return AppendItem(value);
  }

  // This may only be called on reversed buffers.
  template <typename T>
  bool PrependItem(const T& aItem) {
    MOZ_ASSERT(mReversed);

    size_t alignedBytes = AlignUp<Alignment>::calc(sizeof(aItem));
    if (!mUniformSize) {
      mUniformSize = alignedBytes;
    }

    if (!EnsureBackwardRoomFor(alignedBytes)) {
      return false;
    }
    if (mUniformSize != alignedBytes) {
      MOZ_ASSERT_UNREACHABLE("item of incorrect size added!");
      return false;
    }

    mPos -= alignedBytes;
    *reinterpret_cast<T*>(mPos) = aItem;
    MOZ_ASSERT(mPos >= mBuffer.get());

    mNumItems++;
    return true;
  }

  // Prepend an item in two stages.
  template <typename T1, typename T2>
  bool PrependItem(const T1& aFirst, const T2& aSecond) {
    struct Combined {
      T1 first;
      T2 second;
    } value = { aFirst, aSecond };

    // The combined value must be packed.
    static_assert(sizeof(value) == sizeof(aFirst) + sizeof(aSecond),
                  "Items must be packed within struct");
    return PrependItem(value);
  }

  size_t NumBytes() const {
    return mReversed
           ? mEnd - mPos
           : mPos - mBuffer.get();
  }
  uint8_t* GetBufferStart() const {
    return mReversed
           ? mPos
           : mBuffer.get();
  }
  size_t SizeOfItem() const {
    return mUniformSize;
  }
  size_t NumItems() const {
    return mNumItems;
  }

  void Reset() {
    mPos = mReversed ? mEnd : mBuffer.get();
    mUniformSize = 0;
    mNumItems = 0;
  }

  // RestorePosition must only be called with a previous call to
  // GetPosition.
  typedef std::pair<size_t,size_t> Position;
  Position GetPosition() const {
    return std::make_pair(NumBytes(), mNumItems);
  }
  void RestorePosition(const Position& aPosition) {
    mPos = mBuffer.get() + aPosition.first;
    mNumItems = aPosition.second;
    if (mNumItems == 0) {
      mUniformSize = 0;
    }

    // Make sure the buffer is still coherent.
    MOZ_ASSERT(mPos >= mBuffer.get() && mPos <= mEnd);
    MOZ_ASSERT(mNumItems * mUniformSize == NumBytes());
  }

  bool IsEmpty() const {
    return mNumItems == 0;
  }

protected:
  explicit StagingBuffer(size_t aMaxSize)
   : mPos(nullptr),
     mEnd(nullptr),
     mUniformSize(0),
     mNumItems(0),
     mMaxSize(aMaxSize),
     mReversed(false)
  {}

  static const size_t kDefaultSize = 8;

  bool EnsureForwardRoomFor(size_t aAlignedBytes) {
    if (size_t(mEnd - mPos) < aAlignedBytes) {
      return GrowBuffer(aAlignedBytes);
    }
    return true;
  }

  bool EnsureBackwardRoomFor(size_t aAlignedBytes) {
    if (size_t(mPos - mBuffer.get()) < aAlignedBytes) {
      return GrowBuffer(aAlignedBytes);
    }
    return true;
  }

  bool GrowBuffer(size_t aAlignedBytes) {
    // We should not be writing items that are potentially bigger than the
    // maximum constant buffer size, that's crazy. An assert should be good
    // enough since the size of writes is static - and shader compilers
    // would explode anyway.
    MOZ_ASSERT_IF(mMaxSize, aAlignedBytes < mMaxSize);
    MOZ_ASSERT_IF(mMaxSize, kDefaultSize * Alignment < mMaxSize);

    if (!mBuffer) {
      size_t newSize = std::max(kDefaultSize * Alignment, aAlignedBytes);
      MOZ_ASSERT_IF(mMaxSize, newSize < mMaxSize);

      mBuffer = MakeUnique<uint8_t[]>(newSize);
      mEnd = mBuffer.get() + newSize;
      mPos = mReversed ? mEnd : mBuffer.get();
      return true;
    }

    // Take the bigger of exact-fit or 1.5x the previous size, and make sure
    // the new size doesn't overflow size_t. If needed, clamp to the max
    // size.
    size_t oldSize = mEnd - mBuffer.get();
    size_t trySize = std::max(oldSize + aAlignedBytes, oldSize + oldSize / 2);
    size_t newSize = mMaxSize ? std::min(trySize, mMaxSize) : trySize;
    if (newSize < oldSize || newSize - oldSize < aAlignedBytes) {
      return false;
    }

    UniquePtr<uint8_t[]> newBuffer = MakeUnique<uint8_t[]>(newSize);
    if (!newBuffer) {
      return false;
    }

    // When the buffer is in reverse mode, we have to copy from the end of the
    // buffer, not the beginning.
    if (mReversed) {
      size_t usedBytes = mEnd - mPos;
      size_t newPos = newSize - usedBytes;
      MOZ_RELEASE_ASSERT(newPos + usedBytes <= newSize);

      memcpy(newBuffer.get() + newPos, mPos, usedBytes);
      mPos = newBuffer.get() + newPos;
    } else {
      size_t usedBytes = mPos - mBuffer.get();
      MOZ_RELEASE_ASSERT(usedBytes <= newSize);

      memcpy(newBuffer.get(), mBuffer.get(), usedBytes);
      mPos = newBuffer.get() + usedBytes;
    }
    mEnd = newBuffer.get() + newSize;
    mBuffer = std::move(newBuffer);

    MOZ_RELEASE_ASSERT(mPos >= mBuffer.get() && mPos <= mEnd);
    return true;
  }

protected:
  UniquePtr<uint8_t[]> mBuffer;
  uint8_t* mPos;
  uint8_t* mEnd;
  size_t mUniformSize;
  size_t mNumItems;
  size_t mMaxSize;
  bool mReversed;
};

class ConstantStagingBuffer : public StagingBuffer<16>
{
 public:
  explicit ConstantStagingBuffer(MLGDevice* aDevice);
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_StagingBuffer_h
