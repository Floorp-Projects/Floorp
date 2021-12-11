/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxQuartzSurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/HelpersCairo.h"

#include "cairo-quartz.h"

void gfxQuartzSurface::MakeInvalid() { mSize = mozilla::gfx::IntSize(-1, -1); }

gfxQuartzSurface::gfxQuartzSurface(const mozilla::gfx::IntSize& desiredSize,
                                   gfxImageFormat format)
    : mCGContext(nullptr), mSize(desiredSize) {
  if (!mozilla::gfx::Factory::CheckSurfaceSize(desiredSize)) MakeInvalid();

  unsigned int width = static_cast<unsigned int>(mSize.width);
  unsigned int height = static_cast<unsigned int>(mSize.height);

  cairo_format_t cformat = GfxFormatToCairoFormat(format);
  cairo_surface_t* surf = cairo_quartz_surface_create(cformat, width, height);

  mCGContext = cairo_quartz_surface_get_cg_context(surf);

  CGContextRetain(mCGContext);

  Init(surf);
  if (mSurfaceValid) {
    RecordMemoryUsed(mSize.height * 4 + sizeof(gfxQuartzSurface));
  }
}

gfxQuartzSurface::gfxQuartzSurface(CGContextRef context,
                                   const mozilla::gfx::IntSize& size)
    : mCGContext(context), mSize(size) {
  if (!mozilla::gfx::Factory::CheckSurfaceSize(size)) MakeInvalid();

  unsigned int width = static_cast<unsigned int>(mSize.width);
  unsigned int height = static_cast<unsigned int>(mSize.height);

  cairo_surface_t* surf =
      cairo_quartz_surface_create_for_cg_context(context, width, height);

  CGContextRetain(mCGContext);

  Init(surf);
  if (mSurfaceValid) {
    RecordMemoryUsed(mSize.height * 4 + sizeof(gfxQuartzSurface));
  }
}

gfxQuartzSurface::gfxQuartzSurface(cairo_surface_t* csurf,
                                   const mozilla::gfx::IntSize& aSize)
    : mSize(aSize) {
  mCGContext = cairo_quartz_surface_get_cg_context(csurf);
  CGContextRetain(mCGContext);

  Init(csurf, true);
}

gfxQuartzSurface::gfxQuartzSurface(unsigned char* data,
                                   const mozilla::gfx::IntSize& aSize,
                                   long stride, gfxImageFormat format)
    : mCGContext(nullptr), mSize(aSize.width, aSize.height) {
  if (!mozilla::gfx::Factory::CheckSurfaceSize(aSize)) MakeInvalid();

  cairo_format_t cformat = GfxFormatToCairoFormat(format);
  cairo_surface_t* surf = cairo_quartz_surface_create_for_data(
      data, cformat, aSize.width, aSize.height, stride);

  mCGContext = cairo_quartz_surface_get_cg_context(surf);

  CGContextRetain(mCGContext);

  Init(surf);
  if (mSurfaceValid) {
    RecordMemoryUsed(mSize.height * stride + sizeof(gfxQuartzSurface));
  }
}

already_AddRefed<gfxImageSurface> gfxQuartzSurface::GetAsImageSurface() {
  cairo_surface_t* surface = cairo_quartz_surface_get_image(mSurface);
  if (!surface || cairo_surface_status(surface)) return nullptr;

  RefPtr<gfxASurface> img = Wrap(surface);

  // cairo_quartz_surface_get_image returns a referenced image, and thebes
  // shares the refcounts of Cairo surfaces. However, Wrap also adds a
  // reference to the image. We need to remove one of these references
  // explicitly so we don't leak.
  img.get()->Release();

  img->SetOpaqueRect(GetOpaqueRect());

  return img.forget().downcast<gfxImageSurface>();
}

gfxQuartzSurface::~gfxQuartzSurface() { CGContextRelease(mCGContext); }
