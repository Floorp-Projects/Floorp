/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_BufferCache_h
#define mozilla_gfx_layers_mlgpu_BufferCache_h

#include "mozilla/EnumeratedArray.h"
#include "nsTArray.h"
#include <deque>

namespace mozilla {
namespace layers {

class MLGBuffer;
class MLGDevice;

// This file defines a buffer caching mechanism for systems where constant
// buffer offset binding is not allowed. On those systems we must allocate
// new buffers each frame, and this cache allows us to re-use them.

// Track buffers based on their size class, for small buffers.
class BufferPool
{
public:
  // Remove a buffer from the pool holding at least |aBytes|.
  RefPtr<MLGBuffer> Take(size_t aBytes);

  // Put a buffer into the pool holding at least |aBytes|.
  void Put(MLGBuffer* aBuffer);

  BufferPool& operator =(BufferPool&& aOther) {
    mClasses = Move(aOther.mClasses);
    mHugeBuffers = Move(aOther.mHugeBuffers);
    return *this;
  }

private:
  // Try to see if we can quickly re-use any buffer that didn't fit into a
  // pre-existing size class.
  RefPtr<MLGBuffer> TakeHugeBuffer(size_t aBytes);

  enum class SizeClass {
    One,    // 16+ bytes (one constant)
    Two,    // 32+ bytes (two constants)
    Four,   // 64+ bytes (four constants)
    Eight,  // 128+ bytes (eight constants)
    Medium,  // 256+ bytes (16 constants)
    Large,   // 512+ bytes (32 constants)
    Huge     // 1024+ bytes (64+ constants)
  };
  static SizeClass GetSizeClassFromHighBit(size_t bit);

private:
  typedef nsTArray<RefPtr<MLGBuffer>> BufferList;
  EnumeratedArray<SizeClass, SizeClass::Huge, BufferList> mClasses;
  std::deque<RefPtr<MLGBuffer>> mHugeBuffers;
};

// Cache buffer pools based on how long ago they were last used.
class BufferCache
{
public:
  explicit BufferCache(MLGDevice* aDevice);
  ~BufferCache();

  // Get a buffer that has at least |aBytes|, or create a new one
  // if none can be re-used.
  RefPtr<MLGBuffer> GetOrCreateBuffer(size_t aBytes);

  // Rotate buffers after a frame has been completed.
  void EndFrame();

private:
  // Not RefPtr since this would create a cycle.
  MLGDevice* mDevice;

  // We keep three active buffer pools:
  //   The "expired" pool, which was used two frames ago.
  //   The "previous" pool, which is being used by the previous frame.
  //   The "current" pool, which is being used for the current frame.
  //
  // We always allocate from the expired pool into the current pool.
  // After a frame is completed, the current is moved into the previous,
  // and the previous is moved into the expired. The expired buffers
  // are deleted if still alive.
  //
  // Since Layers does not allow us to composite more than one frame
  // ahead, this system ensures the expired buffers are always free.
  BufferPool mExpired;
  BufferPool mPrevious;
  BufferPool mCurrent;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_BufferCache_h
