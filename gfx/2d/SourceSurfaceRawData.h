/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACERAWDATA_H_
#define MOZILLA_GFX_SOURCESURFACERAWDATA_H_

#include "2D.h"
#include "Tools.h"
#include "mozilla/Atomics.h"

namespace mozilla {
namespace gfx {

class SourceSurfaceRawData : public DataSourceSurface
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceRawData)
  SourceSurfaceRawData()
    : mMapCount(0)
  {}
  ~SourceSurfaceRawData()
  {
    if(mOwnData) delete [] mRawData;
    MOZ_ASSERT(mMapCount == 0);
  }

  virtual uint8_t *GetData() { return mRawData; }
  virtual int32_t Stride() { return mStride; }

  virtual SurfaceType GetType() const { return SurfaceType::DATA; }
  virtual IntSize GetSize() const { return mSize; }
  virtual SurfaceFormat GetFormat() const { return mFormat; }

  bool InitWrappingData(unsigned char *aData,
                        const IntSize &aSize,
                        int32_t aStride,
                        SurfaceFormat aFormat,
                        bool aOwnData);

  virtual void GuaranteePersistance();

  // Althought Map (and Moz2D in general) isn't normally threadsafe,
  // we want to allow it for SourceSurfaceRawData since it should
  // always be fine (for reading at least).
  //
  // This is the same as the base class implementation except using
  // mMapCount instead of mIsMapped since that breaks for multithread.
  //
  // Once mfbt supports Monitors we should implement proper read/write
  // locking to prevent write races.
  virtual bool Map(MapType, MappedSurface *aMappedSurface) override
  {
    aMappedSurface->mData = GetData();
    aMappedSurface->mStride = Stride();
    bool success = !!aMappedSurface->mData;
    if (success) {
      mMapCount++;
    }
    return success;
  }

  virtual void Unmap() override
  {
    mMapCount--;
    MOZ_ASSERT(mMapCount >= 0);
  }

private:
  uint8_t *mRawData;
  int32_t mStride;
  SurfaceFormat mFormat;
  IntSize mSize;
  Atomic<int32_t> mMapCount;
  bool mOwnData;
};

class SourceSurfaceAlignedRawData : public DataSourceSurface
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceAlignedRawData)
  SourceSurfaceAlignedRawData()
    : mMapCount(0)
  {}
  ~SourceSurfaceAlignedRawData()
  {
    MOZ_ASSERT(mMapCount == 0);
  }

  virtual uint8_t *GetData() { return mArray; }
  virtual int32_t Stride() { return mStride; }

  virtual SurfaceType GetType() const { return SurfaceType::DATA; }
  virtual IntSize GetSize() const { return mSize; }
  virtual SurfaceFormat GetFormat() const { return mFormat; }

  bool Init(const IntSize &aSize,
            SurfaceFormat aFormat,
            bool aZero);
  bool InitWithStride(const IntSize &aSize,
                      SurfaceFormat aFormat,
                      int32_t aStride,
                      bool aZero);

  virtual bool Map(MapType, MappedSurface *aMappedSurface) override
  {
    aMappedSurface->mData = GetData();
    aMappedSurface->mStride = Stride();
    bool success = !!aMappedSurface->mData;
    if (success) {
      mMapCount++;
    }
    return success;
  }

  virtual void Unmap() override
  {
    mMapCount--;
    MOZ_ASSERT(mMapCount >= 0);
  }

private:
  AlignedArray<uint8_t> mArray;
  int32_t mStride;
  SurfaceFormat mFormat;
  IntSize mSize;
  Atomic<int32_t> mMapCount;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_SOURCESURFACERAWDATA_H_ */
