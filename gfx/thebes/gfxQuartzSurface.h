/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_QUARTZSURFACE_H
#define GFX_QUARTZSURFACE_H

#include "gfxASurface.h"
#include "nsSize.h"
#include "gfxPoint.h"

#include <Carbon/Carbon.h>

class gfxContext;
class gfxImageSurface;

class gfxQuartzSurface : public gfxASurface {
 public:
  gfxQuartzSurface(const mozilla::gfx::IntSize&, gfxImageFormat format);
  gfxQuartzSurface(CGContextRef context, const mozilla::gfx::IntSize& size);
  gfxQuartzSurface(cairo_surface_t* csurf, const mozilla::gfx::IntSize& aSize);
  gfxQuartzSurface(unsigned char* data, const mozilla::gfx::IntSize& size,
                   long stride, gfxImageFormat format);

  virtual ~gfxQuartzSurface();

  virtual const mozilla::gfx::IntSize GetSize() const { return mSize; }

  CGContextRef GetCGContext() { return mCGContext; }

  already_AddRefed<gfxImageSurface> GetAsImageSurface();

 protected:
  void MakeInvalid();

  CGContextRef mCGContext;
  mozilla::gfx::IntSize mSize;
};

#endif /* GFX_QUARTZSURFACE_H */
