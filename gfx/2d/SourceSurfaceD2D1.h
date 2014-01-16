/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACED2D1_H_
#define MOZILLA_GFX_SOURCESURFACED2D1_H_

#include "2D.h"
#include "HelpersD2D.h"
#include <vector>
#include <d3d11.h>
#include <d2d1_1.h>

namespace mozilla {
namespace gfx {

class DrawTargetD2D1;

class SourceSurfaceD2D1 : public SourceSurface
{
public:
  SourceSurfaceD2D1(ID2D1Image* aImage, ID2D1DeviceContext *aDC,
                    SurfaceFormat aFormat, const IntSize &aSize,
                    DrawTargetD2D1 *aDT = nullptr);
  ~SourceSurfaceD2D1();

  virtual SurfaceType GetType() const { return SurfaceType::D2D1_1_IMAGE; }
  virtual IntSize GetSize() const { return mSize; }
  virtual SurfaceFormat GetFormat() const { return mFormat; }
  virtual TemporaryRef<DataSourceSurface> GetDataSurface();

  ID2D1Image *GetImage() { return mImage; }

  void EnsureIndependent() { if (!mDrawTarget) return; DrawTargetWillChange(); }

private:
  friend class DrawTargetD2D1;

  void EnsureRealizedBitmap();

  // This function is called by the draw target this texture belongs to when
  // it is about to be changed. The texture will be required to make a copy
  // of itself when this happens.
  void DrawTargetWillChange();

  // This will mark the surface as no longer depending on its drawtarget,
  // this may happen on destruction or copying.
  void MarkIndependent();

  RefPtr<ID2D1Image> mImage;
  // This may be null if we were created for a non-bitmap image and have not
  // had a reason yet to realize ourselves.
  RefPtr<ID2D1Bitmap1> mRealizedBitmap;
  RefPtr<ID2D1DeviceContext> mDC;

  SurfaceFormat mFormat;
  IntSize mSize;
  RefPtr<DrawTargetD2D1> mDrawTarget;
};

class DataSourceSurfaceD2D1 : public DataSourceSurface
{
public:
  DataSourceSurfaceD2D1(ID2D1Bitmap1 *aMappableBitmap, SurfaceFormat aFormat);
  ~DataSourceSurfaceD2D1();

  virtual SurfaceType GetType() const { return SurfaceType::DATA; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const { return mFormat; }
  virtual uint8_t *GetData();
  virtual int32_t Stride();

private:
  friend class SourceSurfaceD2DTarget;
  void EnsureMapped();

  mutable RefPtr<ID2D1Bitmap1> mBitmap;
  SurfaceFormat mFormat;
  D2D1_MAPPED_RECT mMap;
  bool mMapped;
};

}
}

#endif /* MOZILLA_GFX_SOURCESURFACED2D2TARGET_H_ */
