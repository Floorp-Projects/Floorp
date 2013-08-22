/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxQuartzImageSurface.h"

#include "cairo-quartz.h"
#include "cairo-quartz-image.h"

gfxQuartzImageSurface::gfxQuartzImageSurface(gfxImageSurface *imageSurface)
{
    if (imageSurface->CairoSurface() == nullptr)
        return;

    cairo_surface_t *surf = cairo_quartz_image_surface_create (imageSurface->CairoSurface());
    Init (surf);
    mSize = ComputeSize();
}

gfxQuartzImageSurface::gfxQuartzImageSurface(cairo_surface_t *csurf)
{
    Init (csurf, true);
    mSize = ComputeSize();
}

gfxQuartzImageSurface::~gfxQuartzImageSurface()
{
}

gfxIntSize
gfxQuartzImageSurface::ComputeSize()
{
  if (mSurfaceValid) {
    cairo_surface_t* isurf = cairo_quartz_image_surface_get_image(mSurface);
    if (isurf) {
      return gfxIntSize(cairo_image_surface_get_width(isurf),
                        cairo_image_surface_get_height(isurf));
    }
  }

  // If we reach here then something went wrong. Just use the same default
  // value as gfxASurface::GetSize.
  return gfxIntSize(-1, -1);
}

int32_t
gfxQuartzImageSurface::KnownMemoryUsed()
{
  // This surface doesn't own any memory itself, but we want to report here the
  // amount of memory that the surface it wraps uses.
  nsRefPtr<gfxImageSurface> imgSurface = GetAsImageSurface();
  if (imgSurface)
    return imgSurface->KnownMemoryUsed();
  return 0;
}

already_AddRefed<gfxImageSurface>
gfxQuartzImageSurface::GetAsImageSurface()
{
    if (!mSurfaceValid)
        return nullptr;

    cairo_surface_t *isurf = cairo_quartz_image_surface_get_image (CairoSurface());
    if (!isurf) {
        NS_WARNING ("Couldn't obtain an image surface from a QuartzImageSurface?!");
        return nullptr;
    }

    return gfxASurface::Wrap(isurf).downcast<gfxImageSurface>();
}
