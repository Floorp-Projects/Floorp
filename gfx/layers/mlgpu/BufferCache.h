/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_BufferCache_h
#define mozilla_gfx_layers_mlgpu_BufferCache_h

#include "mozilla/EnumeratedArray.h"
#include <deque>
#include <vector>

namespace mozilla {
namespace layers {

class MLGBuffer;
class MLGDevice;

// Cache MLGBuffers based on how long ago they were last used.
class BufferCache
{
public:
  explicit BufferCache(MLGDevice* aDevice);
  ~BufferCache();

  // Get a buffer that has at least |aBytes|, or create a new one
  // if none can be re-used.
  RefPtr<MLGBuffer> GetOrCreateBuffer(size_t aBytes);

  // Age out old buffers after a frame has been completed.
  void EndFrame();

private:
  // Not RefPtr since this would create a cycle.
  MLGDevice* mDevice;

  // The first size class is Log2(N), where 16 is the minimum size of a
  // constant buffer (currently 16 bytes).
  size_t mFirstSizeClass;

  // Each size class is a power of 2. Each pool of buffers is represented as a
  // deque, with the least-recently-used (i.e., oldest) buffers at the front,
  // and most-recently-used (i.e., newest) buffers at the back. To re-use a
  // buffer it is popped off the front and re-added to the back.
  //
  // This is not always efficient use of storage: if a single frame allocates
  // 300 buffers of the same size, we may keep recycling through all those
  // buffers for a long time, as long as at least one gets used per frame.
  // But since buffers use tiny amounts of memory, and they are only mapped
  // while drawing, it shouldn't be a big deal.
  struct CacheEntry {
    CacheEntry() : mLastUsedFrame(0)
    {}
    CacheEntry(const CacheEntry& aEntry)
     : mLastUsedFrame(aEntry.mLastUsedFrame),
       mBuffer(aEntry.mBuffer)
    {}
    CacheEntry(CacheEntry&& aEntry)
     : mLastUsedFrame(aEntry.mLastUsedFrame),
       mBuffer(Move(aEntry.mBuffer))
    {}
    CacheEntry(size_t aLastUsedFrame, MLGBuffer* aBuffer)
     : mLastUsedFrame(aLastUsedFrame),
       mBuffer(aBuffer)
    {}

    uint64_t mLastUsedFrame;
    RefPtr<MLGBuffer> mBuffer;
  };
  typedef std::deque<CacheEntry> CachePool;

  // We track how many frames have occurred to determine the age of cache entries.
  uint64_t mFrameNumber;

  // To avoid doing too much work in one frame, we only shrink one size class
  // per frame.
  uint64_t mNextSizeClassToShrink;

  // There is one pool of buffers for each power of 2 allocation size. The
  // maximum buffer size is at most 64KB on Direct3D 11.
  std::vector<CachePool> mCaches;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_BufferCache_h
