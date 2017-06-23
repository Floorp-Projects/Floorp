/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BufferCache.h"
#include "MLGDevice.h"
#include "mozilla/MathAlgorithms.h"

namespace mozilla {
namespace layers {

BufferCache::BufferCache(MLGDevice* aDevice)
 : mDevice(aDevice)
{
}

BufferCache::~BufferCache()
{
}

RefPtr<MLGBuffer>
BufferCache::GetOrCreateBuffer(size_t aBytes)
{
  // Try to take a buffer from the expired frame. If none exists, make a new one.
  RefPtr<MLGBuffer> buffer = mExpired.Take(aBytes);
  if (!buffer) {
    // Round up to the nearest size class, but not over 1024 bytes.
    size_t roundedUp = std::max(std::min(RoundUpPow2(aBytes), size_t(1024)), aBytes);
    buffer = mDevice->CreateBuffer(MLGBufferType::Constant, roundedUp, MLGUsage::Dynamic, nullptr);
    if (!buffer) {
      return nullptr;
    }
  }

  MOZ_ASSERT(buffer->GetSize() >= aBytes);

  // Assign this buffer to the current frame, so it becomes available again once
  // this frame expires.
  mCurrent.Put(buffer);
  return buffer;
}

void
BufferCache::EndFrame()
{
  BufferPool empty;
  mExpired = Move(mPrevious);
  mPrevious = Move(mCurrent);
  mCurrent = Move(empty);
}

RefPtr<MLGBuffer>
BufferPool::Take(size_t aBytes)
{
  MOZ_ASSERT(aBytes >= 16);

  // We need to bump the request up to the nearest size class. For example,
  // a request of 24 bytes must allocate from the 32 byte pool.
  SizeClass sc = GetSizeClassFromHighBit(CeilingLog2(aBytes));
  if (sc == SizeClass::Huge) {
    return TakeHugeBuffer(aBytes);
  }

  if (mClasses[sc].IsEmpty()) {
    return nullptr;
  }

  RefPtr<MLGBuffer> buffer = mClasses[sc].LastElement();
  mClasses[sc].RemoveElementAt(mClasses[sc].Length() - 1);
  return buffer.forget();
}

void
BufferPool::Put(MLGBuffer* aBuffer)
{
  MOZ_ASSERT(aBuffer->GetSize() >= 16);

  // When returning buffers, we bump them into a lower size class. For example
  // a 24 byte buffer cannot be re-used for a 32-byte allocation, so it goes
  // into the 16-byte class.
  SizeClass sc = GetSizeClassFromHighBit(FloorLog2(aBuffer->GetSize()));
  if (sc == SizeClass::Huge) {
    mHugeBuffers.push_back(aBuffer);
  } else {
    mClasses[sc].AppendElement(aBuffer);
  }
}

RefPtr<MLGBuffer>
BufferPool::TakeHugeBuffer(size_t aBytes)
{
  static const size_t kMaxSearches = 3;
  size_t numSearches = std::min(kMaxSearches, mHugeBuffers.size());

  for (size_t i = 0; i < numSearches; i++) {
    RefPtr<MLGBuffer> buffer = mHugeBuffers.front();
    mHugeBuffers.pop_front();

    // Don't pick buffers that are massively overallocated.
    if (buffer->GetSize() >= aBytes && buffer->GetSize() <= aBytes * 2) {
      return buffer.forget();
    }

    // Return the buffer to the list.
    mHugeBuffers.push_back(buffer);
  }

  return nullptr;
}

/* static */ BufferPool::SizeClass
BufferPool::GetSizeClassFromHighBit(size_t aBit)
{
  // If the size is smaller than our smallest size class (which should
  // never happen), or bigger than our largest size class, we dump it
  // in the catch-all "huge" list.
  static const size_t kBitForFirstClass = 4;
  static const size_t kBitForLastClass = kBitForFirstClass + size_t(SizeClass::Huge);
  if (aBit < kBitForFirstClass || aBit >= kBitForLastClass) {
    return SizeClass::Huge;
  }
  return SizeClass(aBit - kBitForFirstClass);
}

} // namespace layers
} // namespace mozilla
