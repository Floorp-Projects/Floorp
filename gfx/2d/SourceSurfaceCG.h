/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <ApplicationServices/ApplicationServices.h>

#include "2D.h"

class MacIOSurface;

namespace mozilla {
namespace gfx {

class DrawTargetCG;

class SourceSurfaceCG : public SourceSurface
{
public:
  SourceSurfaceCG() {}
  SourceSurfaceCG(CGImageRef aImage) : mImage(aImage) {}
  ~SourceSurfaceCG();

  virtual SurfaceType GetType() const { return SURFACE_COREGRAPHICS_IMAGE; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;
  virtual TemporaryRef<DataSourceSurface> GetDataSurface();

  CGImageRef GetImage() { return mImage; }

  bool InitFromData(unsigned char *aData,
                    const IntSize &aSize,
                    int32_t aStride,
                    SurfaceFormat aFormat);

private:
  CGImageRef mImage;

  /* It might be better to just use the bitmap info from the CGImageRef to
   * deduce the format to save space in SourceSurfaceCG,
   * for now we just store it in mFormat */
  SurfaceFormat mFormat;
};

class DataSourceSurfaceCG : public DataSourceSurface
{
public:
  DataSourceSurfaceCG() {}
  DataSourceSurfaceCG(CGImageRef aImage);
  ~DataSourceSurfaceCG();

  virtual SurfaceType GetType() const { return SURFACE_DATA; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const { return FORMAT_B8G8R8A8; }

  CGImageRef GetImage() { return mImage; }

  bool InitFromData(unsigned char *aData,
                    const IntSize &aSize,
                    int32_t aStride,
                    SurfaceFormat aFormat);

  virtual unsigned char *GetData();

  virtual int32_t Stride() { return CGImageGetBytesPerRow(mImage); }


private:
  CGContextRef mCg;
  CGImageRef mImage;
  //XXX: we don't need to store mData we can just get it from the CGContext
  void *mData;
  /* It might be better to just use the bitmap info from the CGImageRef to
   * deduce the format to save space in SourceSurfaceCG,
   * for now we just store it in mFormat */
};

class SourceSurfaceCGContext : public DataSourceSurface
{
public:
  virtual void DrawTargetWillChange() = 0;
  virtual CGImageRef GetImage() = 0;
};

class SourceSurfaceCGBitmapContext : public SourceSurfaceCGContext
{
public:
  SourceSurfaceCGBitmapContext(DrawTargetCG *);
  ~SourceSurfaceCGBitmapContext();

  virtual SurfaceType GetType() const { return SURFACE_COREGRAPHICS_CGCONTEXT; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const { return FORMAT_B8G8R8A8; }

  CGImageRef GetImage() { EnsureImage(); return mImage; }

  virtual unsigned char *GetData() { return static_cast<unsigned char*>(mData); }

  virtual int32_t Stride() { return mStride; }

private:
  //XXX: do the other backends friend their DrawTarget?
  friend class DrawTargetCG;
  virtual void DrawTargetWillChange();
  void EnsureImage() const;

  // We hold a weak reference to these two objects.
  // The cycle is broken by DrawTargetWillChange
  DrawTargetCG *mDrawTarget;
  CGContextRef mCg;

  mutable CGImageRef mImage;

  // mData can be owned by three different things:
  // mImage, mCg or SourceSurfaceCGBitmapContext
  void *mData;

  int32_t mStride;
  IntSize mSize;
};

class SourceSurfaceCGIOSurfaceContext : public SourceSurfaceCGContext
{
public:
  SourceSurfaceCGIOSurfaceContext(DrawTargetCG *);
  ~SourceSurfaceCGIOSurfaceContext();

  virtual SurfaceType GetType() const { return SURFACE_COREGRAPHICS_CGCONTEXT; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const { return FORMAT_B8G8R8A8; }

  CGImageRef GetImage() { EnsureImage(); return mImage; }

  virtual unsigned char *GetData();

  virtual int32_t Stride() { return mStride; }

private:
  //XXX: do the other backends friend their DrawTarget?
  friend class DrawTargetCG;
  virtual void DrawTargetWillChange();
  void EnsureImage() const;

  mutable CGImageRef mImage;
  MacIOSurface* mIOSurface;

  void *mData;
  int32_t mStride;

  IntSize mSize;
};


}
}
