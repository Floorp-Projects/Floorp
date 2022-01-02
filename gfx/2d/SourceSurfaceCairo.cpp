/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceCairo.h"
#include "DrawTargetCairo.h"
#include "HelpersCairo.h"
#include "DataSourceSurfaceWrapper.h"

#include "cairo.h"

namespace mozilla {
namespace gfx {

static SurfaceFormat CairoFormatToSurfaceFormat(cairo_format_t format) {
  switch (format) {
    case CAIRO_FORMAT_ARGB32:
      return SurfaceFormat::B8G8R8A8;
    case CAIRO_FORMAT_RGB24:
      return SurfaceFormat::B8G8R8X8;
    case CAIRO_FORMAT_RGB16_565:
      return SurfaceFormat::R5G6B5_UINT16;
    case CAIRO_FORMAT_A8:
      return SurfaceFormat::A8;
    default:
      return SurfaceFormat::B8G8R8A8;
  }
}

SourceSurfaceCairo::SourceSurfaceCairo(
    cairo_surface_t* aSurface, const IntSize& aSize,
    const SurfaceFormat& aFormat, DrawTargetCairo* aDrawTarget /* = nullptr */)
    : mSize(aSize),
      mFormat(aFormat),
      mSurface(aSurface),
      mDrawTarget(aDrawTarget) {
  cairo_surface_reference(mSurface);
}

SourceSurfaceCairo::~SourceSurfaceCairo() { cairo_surface_destroy(mSurface); }

IntSize SourceSurfaceCairo::GetSize() const { return mSize; }

SurfaceFormat SourceSurfaceCairo::GetFormat() const { return mFormat; }

already_AddRefed<DataSourceSurface> SourceSurfaceCairo::GetDataSurface() {
  RefPtr<DataSourceSurface> dataSurf;

  if (cairo_surface_get_type(mSurface) == CAIRO_SURFACE_TYPE_IMAGE) {
    dataSurf = new DataSourceSurfaceCairo(mSurface);
  } else {
    cairo_surface_t* imageSurf = cairo_image_surface_create(
        GfxFormatToCairoFormat(mFormat), mSize.width, mSize.height);

    // Fill the new image surface with the contents of our surface.
    cairo_t* ctx = cairo_create(imageSurf);
    cairo_set_source_surface(ctx, mSurface, 0, 0);
    cairo_paint(ctx);
    cairo_destroy(ctx);

    dataSurf = new DataSourceSurfaceCairo(imageSurf);
    cairo_surface_destroy(imageSurf);
  }

  // We also need to make sure that the returned surface has
  // surface->GetType() == SurfaceType::DATA.
  return MakeAndAddRef<DataSourceSurfaceWrapper>(dataSurf);
}

cairo_surface_t* SourceSurfaceCairo::GetSurface() const { return mSurface; }

void SourceSurfaceCairo::DrawTargetWillChange() {
  if (mDrawTarget) {
    mDrawTarget = nullptr;

    // We're about to lose our version of the surface, so make a copy of it.
    cairo_surface_t* surface = cairo_surface_create_similar(
        mSurface, GfxFormatToCairoContent(mFormat), mSize.width, mSize.height);
    cairo_t* ctx = cairo_create(surface);
    cairo_pattern_t* pat = cairo_pattern_create_for_surface(mSurface);
    cairo_set_source(ctx, pat);
    cairo_paint(ctx);
    cairo_destroy(ctx);
    cairo_pattern_destroy(pat);

    // Swap in this new surface.
    cairo_surface_destroy(mSurface);
    mSurface = surface;
  }
}

DataSourceSurfaceCairo::DataSourceSurfaceCairo(cairo_surface_t* imageSurf)
    : mImageSurface(imageSurf) {
  cairo_surface_reference(mImageSurface);
}

DataSourceSurfaceCairo::~DataSourceSurfaceCairo() {
  cairo_surface_destroy(mImageSurface);
}

unsigned char* DataSourceSurfaceCairo::GetData() {
  return cairo_image_surface_get_data(mImageSurface);
}

int32_t DataSourceSurfaceCairo::Stride() {
  return cairo_image_surface_get_stride(mImageSurface);
}

IntSize DataSourceSurfaceCairo::GetSize() const {
  IntSize size;
  size.width = cairo_image_surface_get_width(mImageSurface);
  size.height = cairo_image_surface_get_height(mImageSurface);

  return size;
}

SurfaceFormat DataSourceSurfaceCairo::GetFormat() const {
  return CairoFormatToSurfaceFormat(
      cairo_image_surface_get_format(mImageSurface));
}

cairo_surface_t* DataSourceSurfaceCairo::GetSurface() const {
  return mImageSurface;
}

}  // namespace gfx
}  // namespace mozilla
