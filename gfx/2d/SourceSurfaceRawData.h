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
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceRawData, override)

  SourceSurfaceRawData()
    : mRawData(0)
    , mStride(0)
    , mFormat(SurfaceFormat::UNKNOWN)
    , mMapCount(0)
    , mOwnData(false)
    , mDeallocator(nullptr)
    , mClosure(nullptr)
  {
  }

  virtual ~SourceSurfaceRawData()
  {
    if (mDeallocator) {
      mDeallocator(mClosure);
    } else if (mOwnData) {
      // The buffer is created from GuaranteePersistance().
      delete [] mRawData;
    }

    MOZ_ASSERT(mMapCount == 0);
  }

  virtual uint8_t *GetData() override { return mRawData; }
  virtual int32_t Stride() override { return mStride; }

  virtual SurfaceType GetType() const override { return SurfaceType::DATA; }
  virtual IntSize GetSize() const override { return mSize; }
  virtual SurfaceFormat GetFormat() const override { return mFormat; }

  virtual void GuaranteePersistance() override;

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
  friend class Factory;

  // If we have a custom deallocator, the |aData| will be released using the
  // custom deallocator and |aClosure| in dtor.
  void InitWrappingData(unsigned char *aData,
                        const IntSize &aSize,
                        int32_t aStride,
                        SurfaceFormat aFormat,
                        Factory::SourceSurfaceDeallocator aDeallocator,
                        void* aClosure);

  uint8_t *mRawData;
  int32_t mStride;
  SurfaceFormat mFormat;
  IntSize mSize;
  Atomic<int32_t> mMapCount;

  bool mOwnData;
  Factory::SourceSurfaceDeallocator mDeallocator;
  void* mClosure;
};

class SourceSurfaceAlignedRawData : public DataSourceSurface
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceAlignedRawData, override)
  SourceSurfaceAlignedRawData()
    : mStride(0)
    , mFormat(SurfaceFormat::UNKNOWN)
    , mMapCount(0)
  {}
  ~SourceSurfaceAlignedRawData()
  {
    MOZ_ASSERT(mMapCount == 0);
  }

  virtual uint8_t* GetData() override { return mArray; }
  virtual int32_t Stride() override { return mStride; }

  virtual SurfaceType GetType() const override { return SurfaceType::DATA; }
  virtual IntSize GetSize() const override { return mSize; }
  virtual SurfaceFormat GetFormat() const override { return mFormat; }

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
  friend class Factory;

  bool Init(const IntSize &aSize,
            SurfaceFormat aFormat,
            bool aClearMem,
            uint8_t aClearValue,
            int32_t aStride = 0);

  AlignedArray<uint8_t> mArray;
  int32_t mStride;
  SurfaceFormat mFormat;
  IntSize mSize;
  Atomic<int32_t> mMapCount;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_SOURCESURFACERAWDATA_H_ */
