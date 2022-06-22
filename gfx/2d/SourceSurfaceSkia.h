/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACESKIA_H_
#define MOZILLA_GFX_SOURCESURFACESKIA_H_

#include "2D.h"
#include "mozilla/Mutex.h"
#include "skia/include/core/SkRefCnt.h"

class SkImage;
class SkSurface;

namespace mozilla {

namespace gfx {

class DrawTargetSkia;
class SnapshotLock;

class SourceSurfaceSkia : public DataSourceSurface {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceSkia, override)

  SourceSurfaceSkia();
  virtual ~SourceSurfaceSkia();

  SurfaceType GetType() const override { return SurfaceType::SKIA; }
  IntSize GetSize() const override;
  SurfaceFormat GetFormat() const override;

  void GiveSurface(SkSurface* aSurface);

  sk_sp<SkImage> GetImage(Maybe<MutexAutoLock>* aLock);

  bool InitFromData(unsigned char* aData, const IntSize& aSize, int32_t aStride,
                    SurfaceFormat aFormat);

  bool InitFromImage(const sk_sp<SkImage>& aImage,
                     SurfaceFormat aFormat = SurfaceFormat::UNKNOWN,
                     DrawTargetSkia* aOwner = nullptr);

  uint8_t* GetData() override;

  /**
   * The caller is responsible for ensuring aMappedSurface is not null.
   */
  bool Map(MapType, MappedSurface* aMappedSurface) override;

  void Unmap() override;

  int32_t Stride() override { return mStride; }

 private:
  friend class DrawTargetSkia;

  void DrawTargetWillChange();

  sk_sp<SkImage> mImage;
  // This keeps a surface alive if needed because its DrawTarget has gone away.
  sk_sp<SkSurface> mSurface;
  SurfaceFormat mFormat;
  IntSize mSize;
  int32_t mStride;
  Atomic<DrawTargetSkia*> mDrawTarget;
  Mutex mChangeMutex MOZ_UNANNOTATED;
  bool mIsMapped;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_SOURCESURFACESKIA_H_ */
