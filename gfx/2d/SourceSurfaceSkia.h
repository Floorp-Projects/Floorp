/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACESKIA_H_
#define MOZILLA_GFX_SOURCESURFACESKIA_H_

#include "2D.h"
#include <vector>
#include "skia/include/core/SkCanvas.h"
#include "skia/include/core/SkImage.h"

namespace mozilla {

namespace gfx {

class DrawTargetSkia;

class SourceSurfaceSkia : public DataSourceSurface
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceSkia)
  SourceSurfaceSkia();
  ~SourceSurfaceSkia();

  virtual SurfaceType GetType() const { return SurfaceType::SKIA; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;

  sk_sp<SkImage>& GetImage() { return mImage; }

  bool InitFromData(unsigned char* aData,
                    const IntSize &aSize,
                    int32_t aStride,
                    SurfaceFormat aFormat);

  bool InitFromImage(const sk_sp<SkImage>& aImage,
                     SurfaceFormat aFormat = SurfaceFormat::UNKNOWN,
                     DrawTargetSkia* aOwner = nullptr);

  virtual uint8_t* GetData();

  virtual int32_t Stride() { return mStride; }

private:
  friend class DrawTargetSkia;

  void DrawTargetWillChange();

  sk_sp<SkImage> mImage;
  SurfaceFormat mFormat;
  IntSize mSize;
  int32_t mStride;
  RefPtr<DrawTargetSkia> mDrawTarget;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_SOURCESURFACESKIA_H_ */
