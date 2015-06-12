/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_SOURCESURFACECG_H
#define _MOZILLA_GFX_SOURCESURFACECG_H

#ifdef MOZ_WIDGET_COCOA
#include <ApplicationServices/ApplicationServices.h>
#else
#include <CoreGraphics/CoreGraphics.h>
#endif

#include "2D.h"

#ifdef MOZ_WIDGET_COCOA
class MacIOSurface;
#endif

namespace mozilla {
namespace gfx {

CGImageRef
CreateCGImage(CGDataProviderReleaseDataCallback aCallback,
              void *aInfo,
              const void *aData,
              const IntSize &aSize,
              int32_t aStride,
              SurfaceFormat aFormat);

CGImageRef
CreateCGImage(void *aInfo,
              const void *aData,
              const IntSize &aSize,
              int32_t aStride,
              SurfaceFormat aFormat);

class DrawTargetCG;

class SourceSurfaceCG : public SourceSurface
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SourceSurfaceCG)
  SourceSurfaceCG() {}
  explicit SourceSurfaceCG(CGImageRef aImage) : mImage(aImage) {}
  ~SourceSurfaceCG();

  virtual SurfaceType GetType() const { return SurfaceType::COREGRAPHICS_IMAGE; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;
  virtual already_AddRefed<DataSourceSurface> GetDataSurface();

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
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceCG)
  DataSourceSurfaceCG() {}
  explicit DataSourceSurfaceCG(CGImageRef aImage);
  ~DataSourceSurfaceCG();

  virtual SurfaceType GetType() const { return SurfaceType::DATA; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const { return mFormat; }

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
  SurfaceFormat mFormat;
  //XXX: we don't need to store mData we can just get it from the CGContext
  void *mData;
  /* It might be better to just use the bitmap info from the CGImageRef to
   * deduce the format to save space in SourceSurfaceCG,
   * for now we just store it in mFormat */
};

class SourceSurfaceCGContext : public DataSourceSurface
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceCGContext)
  virtual void DrawTargetWillChange() = 0;
  virtual void DrawTargetWillGoAway() = 0;
  virtual CGImageRef GetImage() = 0;
};

class SourceSurfaceCGBitmapContext : public SourceSurfaceCGContext
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceCGBitmapContext)
  explicit SourceSurfaceCGBitmapContext(DrawTargetCG *);
  ~SourceSurfaceCGBitmapContext();

  virtual SurfaceType GetType() const { return SurfaceType::COREGRAPHICS_CGCONTEXT; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const { return mFormat; }
  virtual already_AddRefed<DataSourceSurface> GetDataSurface()
  {
    // This call to DrawTargetWillChange() is needed to make a local copy of
    // the data from mDrawTarget.  If we don't do that, the data can end up
    // getting deleted before the CGImageRef it belongs to.
    //
    // Another reason we need a local copy of the data is that the data in
    // mDrawTarget could change when someone touches the original DrawTargetCG
    // object.  But a SourceSurface object should be immutable.
    //
    // For more information see bug 925448.
    DrawTargetWillChange();
    RefPtr<DataSourceSurface> copy(this);
    return copy.forget();
  }

  CGImageRef GetImage() { EnsureImage(); return mImage; }

  virtual unsigned char *GetData() { return static_cast<unsigned char*>(mData); }

  virtual int32_t Stride() { return mStride; }

private:
  //XXX: do the other backends friend their DrawTarget?
  friend class DrawTargetCG;
  virtual void DrawTargetWillChange();
  virtual void DrawTargetWillGoAway();
  void EnsureImage() const;

  // We hold a weak reference to these two objects.
  // The cycle is broken by DrawTargetWillChange
  DrawTargetCG *mDrawTarget;
  CGContextRef mCg;
  SurfaceFormat mFormat;

  mutable CGImageRef mImage;

  // mData can be owned by three different things:
  // mImage, mCg or SourceSurfaceCGBitmapContext
  void *mData;

  // The image buffer, if the buffer is owned by this class.
  AlignedArray<uint8_t> mDataHolder;

  int32_t mStride;
  IntSize mSize;
};

#ifdef MOZ_WIDGET_COCOA
class SourceSurfaceCGIOSurfaceContext : public SourceSurfaceCGContext
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceCGIOSurfaceContext)
  explicit SourceSurfaceCGIOSurfaceContext(DrawTargetCG *);
  ~SourceSurfaceCGIOSurfaceContext();

  virtual SurfaceType GetType() const { return SurfaceType::COREGRAPHICS_CGCONTEXT; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const { return mFormat; }

  CGImageRef GetImage() { EnsureImage(); return mImage; }

  virtual unsigned char *GetData();

  virtual int32_t Stride() { return mStride; }

private:
  //XXX: do the other backends friend their DrawTarget?
  friend class DrawTargetCG;
  virtual void DrawTargetWillChange();
  virtual void DrawTargetWillGoAway() { DrawTargetWillChange(); }
  void EnsureImage() const;

  SurfaceFormat mFormat;
  mutable CGImageRef mImage;
  MacIOSurface* mIOSurface;

  void *mData;
  int32_t mStride;

  IntSize mSize;
};
#endif


} // namespace gfx
} // namespace mozilla

#endif // _MOZILLA_GFX_SOURCESURFACECG_H
