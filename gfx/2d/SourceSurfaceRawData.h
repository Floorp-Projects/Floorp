/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACERAWDATA_H_
#define MOZILLA_GFX_SOURCESURFACERAWDATA_H_

#include "2D.h"
#include "Tools.h"
#include "mozilla/Atomics.h"

namespace mozilla {
namespace gfx {

class SourceSurfaceMappedData final : public DataSourceSurface {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SourceSurfaceMappedData, final)

  SourceSurfaceMappedData(ScopedMap&& aMap, const IntSize& aSize,
                          SurfaceFormat aFormat)
      : mMap(std::move(aMap)), mSize(aSize), mFormat(aFormat) {}

  ~SourceSurfaceMappedData() final = default;

  uint8_t* GetData() final { return mMap.GetData(); }
  int32_t Stride() final { return mMap.GetStride(); }

  SurfaceType GetType() const final { return SurfaceType::DATA_MAPPED; }
  IntSize GetSize() const final { return mSize; }
  SurfaceFormat GetFormat() const final { return mFormat; }

  void SizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                           SizeOfInfo& aInfo) const override {
    aInfo.AddType(SurfaceType::DATA_MAPPED);
    mMap.GetSurface()->SizeOfExcludingThis(aMallocSizeOf, aInfo);
  }

  const DataSourceSurface* GetScopedSurface() const {
    return mMap.GetSurface();
  }

 private:
  ScopedMap mMap;
  IntSize mSize;
  SurfaceFormat mFormat;
};

class SourceSurfaceRawData : public DataSourceSurface {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceRawData, override)

  SourceSurfaceRawData()
      : mRawData(0),
        mStride(0),
        mFormat(SurfaceFormat::UNKNOWN),
        mDeallocator(nullptr),
        mClosure(nullptr) {}

  virtual ~SourceSurfaceRawData() {
    if (mDeallocator) {
      mDeallocator(mClosure);
    }
  }

  virtual uint8_t* GetData() override { return mRawData; }
  virtual int32_t Stride() override { return mStride; }

  virtual SurfaceType GetType() const override { return SurfaceType::DATA; }
  virtual IntSize GetSize() const override { return mSize; }
  virtual SurfaceFormat GetFormat() const override { return mFormat; }

  void SizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                           SizeOfInfo& aInfo) const override;

 private:
  friend class Factory;

  // If we have a custom deallocator, the |aData| will be released using the
  // custom deallocator and |aClosure| in dtor.  The assumption is that the
  // caller will check for valid size and stride before making this call.
  void InitWrappingData(unsigned char* aData, const IntSize& aSize,
                        int32_t aStride, SurfaceFormat aFormat,
                        Factory::SourceSurfaceDeallocator aDeallocator,
                        void* aClosure);

  uint8_t* mRawData;
  int32_t mStride;
  SurfaceFormat mFormat;
  IntSize mSize;

  Factory::SourceSurfaceDeallocator mDeallocator;
  void* mClosure;
};

class SourceSurfaceAlignedRawData : public DataSourceSurface {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceAlignedRawData,
                                          override)
  SourceSurfaceAlignedRawData() : mStride(0), mFormat(SurfaceFormat::UNKNOWN) {}
  ~SourceSurfaceAlignedRawData() override = default;

  bool Init(const IntSize& aSize, SurfaceFormat aFormat, bool aClearMem,
            uint8_t aClearValue, int32_t aStride = 0);

  uint8_t* GetData() override { return mArray; }
  int32_t Stride() override { return mStride; }

  SurfaceType GetType() const override { return SurfaceType::DATA_ALIGNED; }
  IntSize GetSize() const override { return mSize; }
  SurfaceFormat GetFormat() const override { return mFormat; }

  void SizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                           SizeOfInfo& aInfo) const override;

 private:
  friend class Factory;

  AlignedArray<uint8_t> mArray;
  int32_t mStride;
  SurfaceFormat mFormat;
  IntSize mSize;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_SOURCESURFACERAWDATA_H_ */
