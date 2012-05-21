/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACESKIA_H_
#define MOZILLA_GFX_SOURCESURFACESKIA_H_

#include "2D.h"
#include <vector>
#include "skia/SkCanvas.h"
#include "skia/SkBitmap.h"

namespace mozilla {
namespace gfx {

class DrawTargetSkia;

class SourceSurfaceSkia : public DataSourceSurface
{
public:
  SourceSurfaceSkia();
  ~SourceSurfaceSkia();

  virtual SurfaceType GetType() const { return SURFACE_SKIA; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;

  SkBitmap& GetBitmap() { return mBitmap; }

  bool InitFromData(unsigned char* aData,
                    const IntSize &aSize,
                    int32_t aStride,
                    SurfaceFormat aFormat);

  /**
   * If aOwner is NULL, we make a copy of the pixel data in the bitmap, 
   * otherwise we just reference this data until DrawTargetWillChange is called.
   */
  bool InitWithBitmap(const SkBitmap& aBitmap,
                      SurfaceFormat aFormat,
                      DrawTargetSkia* aOwner);


  virtual unsigned char *GetData();

  virtual int32_t Stride() { return mStride; }

private:
  friend class DrawTargetSkia;

  void DrawTargetWillChange();
  void DrawTargetDestroyed();
  void MarkIndependent();

  SkBitmap mBitmap;
  SurfaceFormat mFormat;
  IntSize mSize;
  int32_t mStride;
  DrawTargetSkia* mDrawTarget;
};

}
}

#endif /* MOZILLA_GFX_SOURCESURFACESKIA_H_ */
