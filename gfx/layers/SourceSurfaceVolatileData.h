/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACEVOLATILEDATA_H_
#define MOZILLA_GFX_SOURCESURFACEVOLATILEDATA_H_

#include "mozilla/gfx/2D.h"
#include "mozilla/Mutex.h"
#include "mozilla/VolatileBuffer.h"

namespace mozilla {
namespace gfx {

/**
 * This class is used to wrap volatile data buffers used for source surfaces.
 * The Map and Unmap semantics are used to guarantee that the volatile data
 * buffer is not freed by the operating system while the surface is in active
 * use. If GetData is expected to return a non-null value without a
 * corresponding Map call (and verification of the result), the surface data
 * should be wrapped in a temporary SourceSurfaceRawData with a ScopedMap
 * closure.
 */
class SourceSurfaceVolatileData : public DataSourceSurface
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SourceSurfaceVolatileData, override)

  SourceSurfaceVolatileData()
    : mMutex("SourceSurfaceVolatileData")
    , mStride(0)
    , mMapCount(0)
    , mFormat(SurfaceFormat::UNKNOWN)
  {
  }

  bool Init(const IntSize &aSize,
            int32_t aStride,
            SurfaceFormat aFormat);

  uint8_t *GetData() override { return mVBufPtr; }
  int32_t Stride() override { return mStride; }

  SurfaceType GetType() const override { return SurfaceType::DATA; }
  IntSize GetSize() const override { return mSize; }
  SurfaceFormat GetFormat() const override { return mFormat; }

  void GuaranteePersistance() override;

  void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                              size_t& aHeapSizeOut,
                              size_t& aNonHeapSizeOut) const override;

  bool OnHeap() const override
  {
    return mVBuf->OnHeap();
  }

  // Althought Map (and Moz2D in general) isn't normally threadsafe,
  // we want to allow it for SourceSurfaceVolatileData since it should
  // always be fine (for reading at least).
  //
  // This is the same as the base class implementation except using
  // mMapCount instead of mIsMapped since that breaks for multithread.
  bool Map(MapType, MappedSurface *aMappedSurface) override
  {
    MutexAutoLock lock(mMutex);
    if (mMapCount == 0) {
      mVBufPtr = mVBuf;
    }
    if (mVBufPtr.WasBufferPurged()) {
      return false;
    }
    aMappedSurface->mData = mVBufPtr;
    aMappedSurface->mStride = mStride;
    ++mMapCount;
    return true;
  }

  void Unmap() override
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mMapCount > 0);
    if (--mMapCount == 0) {
      mVBufPtr = nullptr;
    }
  }

private:
  ~SourceSurfaceVolatileData() override
  {
    MOZ_ASSERT(mMapCount == 0);
  }

  Mutex mMutex;
  int32_t mStride;
  int32_t mMapCount;
  IntSize mSize;
  RefPtr<VolatileBuffer> mVBuf;
  VolatileBufferPtr<uint8_t> mVBufPtr;
  SurfaceFormat mFormat;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_SOURCESURFACEVOLATILEDATA_H_ */
