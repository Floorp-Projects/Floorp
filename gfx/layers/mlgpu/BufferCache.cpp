/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BufferCache.h"
#include "MLGDevice.h"
#include "ShaderDefinitionsMLGPU.h"
#include "mozilla/MathAlgorithms.h"

namespace mozilla {
namespace layers {

using namespace mlg;

BufferCache::BufferCache(MLGDevice* aDevice)
 : mDevice(aDevice),
   mFirstSizeClass(CeilingLog2(kConstantBufferElementSize)),
   mFrameNumber(0),
   mNextSizeClassToShrink(0)
{
  // Create a cache of buffers for each size class, where each size class is a
  // power of 2 between the minimum and maximum size of a constant buffer.
  size_t maxBindSize = mDevice->GetMaxConstantBufferBindSize();
  MOZ_ASSERT(IsPowerOfTwo(maxBindSize));

  size_t lastSizeClass = CeilingLog2(maxBindSize);
  MOZ_ASSERT(lastSizeClass >= mFirstSizeClass);

  mCaches.resize(lastSizeClass - mFirstSizeClass + 1);
}

BufferCache::~BufferCache()
{
}

RefPtr<MLGBuffer>
BufferCache::GetOrCreateBuffer(size_t aBytes)
{
  size_t sizeClass = CeilingLog2(aBytes);
  size_t sizeClassIndex = sizeClass - mFirstSizeClass;
  if (sizeClassIndex >= mCaches.size()) {
    return mDevice->CreateBuffer(MLGBufferType::Constant, aBytes, MLGUsage::Dynamic, nullptr);
  }

  CachePool& pool = mCaches[sizeClassIndex];

  // See if we've cached a buffer that wasn't used in the past 2 frames. A buffer
  // used this frame could have already been mapped and written to, and a buffer
  // used the previous frame might still be in-use by the GPU. While the latter
  // case is okay, it causes aliasing in the driver. Since content is double
  // buffered we do not let the compositor get more than 1 frames ahead, and a
  // count of 2 frames should ensure the buffer is unused.
  if (!pool.empty() && mFrameNumber >= pool.front().mLastUsedFrame + 2) {
    RefPtr<MLGBuffer> buffer = pool.front().mBuffer;
    pool.pop_front();
    pool.push_back(CacheEntry(mFrameNumber, buffer));
    MOZ_RELEASE_ASSERT(buffer->GetSize() >= aBytes);
    return buffer;
  }

  // Allocate a new buffer and cache it.
  size_t bytes = (size_t(1) << sizeClass);
  MOZ_ASSERT(bytes >= aBytes);

  RefPtr<MLGBuffer> buffer =
    mDevice->CreateBuffer(MLGBufferType::Constant, bytes, MLGUsage::Dynamic, nullptr);
  if (!buffer) {
    return nullptr;
  }

  pool.push_back(CacheEntry(mFrameNumber, buffer));
  return buffer;
}

void
BufferCache::EndFrame()
{
  // Consider a buffer dead after ~5 seconds assuming 60 fps.
  static size_t kMaxUnusedFrameCount = 60 * 5;

  // At the end of each frame we pick one size class and see if it has any
  // buffers that haven't been used for many frames. If so we clear them.
  // The next frame we'll search the next size class. (This is just to spread
  // work over more than one frame.)
  CachePool& pool = mCaches[mNextSizeClassToShrink];
  while (!pool.empty()) {
    // Since the deque is sorted oldest-to-newest, front-to-back, we can stop
    // searching as soon as a buffer is active.
    if (mFrameNumber - pool.front().mLastUsedFrame < kMaxUnusedFrameCount) {
      break;
    }
    pool.pop_front();
  }
  mNextSizeClassToShrink = (mNextSizeClassToShrink + 1) % mCaches.size();

  mFrameNumber++;
}

} // namespace layers
} // namespace mozilla
