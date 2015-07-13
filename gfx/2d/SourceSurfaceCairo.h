/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_OP_SOURCESURFACE_CAIRO_H
#define _MOZILLA_GFX_OP_SOURCESURFACE_CAIRO_H

#include "2D.h"

namespace mozilla {
namespace gfx {

class DrawTargetCairo;

class SourceSurfaceCairo : public SourceSurface
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SourceSurfaceCairo)
  // Create a SourceSurfaceCairo. The surface will not be copied, but simply
  // referenced.
  // If aDrawTarget is non-nullptr, it is assumed that this is a snapshot source
  // surface, and we'll call DrawTargetCairo::RemoveSnapshot(this) on it when
  // we're destroyed.
  SourceSurfaceCairo(cairo_surface_t* aSurface, const IntSize& aSize,
                     const SurfaceFormat& aFormat,
                     DrawTargetCairo* aDrawTarget = nullptr);
  virtual ~SourceSurfaceCairo();

  virtual SurfaceType GetType() const { return SurfaceType::CAIRO; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;
  virtual already_AddRefed<DataSourceSurface> GetDataSurface();

  cairo_surface_t* GetSurface() const;

private: // methods
  friend class DrawTargetCairo;
  void DrawTargetWillChange();

private: // data
  IntSize mSize;
  SurfaceFormat mFormat;
  cairo_surface_t* mSurface;
  DrawTargetCairo* mDrawTarget;
};

class DataSourceSurfaceCairo : public DataSourceSurface
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceCairo)
  explicit DataSourceSurfaceCairo(cairo_surface_t* imageSurf);
  virtual ~DataSourceSurfaceCairo();
  virtual unsigned char *GetData();
  virtual int32_t Stride();

  virtual SurfaceType GetType() const { return SurfaceType::CAIRO_IMAGE; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;

  cairo_surface_t* GetSurface() const;

private:
  cairo_surface_t* mImageSurface;
};

} // namespace gfx
} // namespace mozilla

#endif // _MOZILLA_GFX_OP_SOURCESURFACE_CAIRO_H
