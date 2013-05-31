/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_QUARTZIMAGESURFACE_H
#define GFX_QUARTZIMAGESURFACE_H

#include "gfxASurface.h"
#include "gfxImageSurface.h"

class gfxQuartzImageSurface : public gfxASurface {
public:
    gfxQuartzImageSurface(gfxImageSurface *imageSurface);
    gfxQuartzImageSurface(cairo_surface_t *csurf);

    virtual ~gfxQuartzImageSurface();

    already_AddRefed<gfxImageSurface> GetAsImageSurface();
    virtual int32_t KnownMemoryUsed();
    virtual const gfxIntSize GetSize() const { return gfxIntSize(mSize.width, mSize.height); }

protected:
    gfxIntSize mSize;

private:
    gfxIntSize ComputeSize();
};

#endif /* GFX_QUARTZIMAGESURFACE_H */
