/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
    gfxQuartzSurface(const gfxSize& size, gfxImageFormat format, bool aForPrinting = false);
    gfxQuartzSurface(CGContextRef context, const gfxSize& size, bool aForPrinting = false);
    gfxQuartzSurface(CGContextRef context, const gfxIntSize& size, bool aForPrinting = false);
    gfxQuartzSurface(cairo_surface_t *csurf, bool aForPrinting = false);
    gfxQuartzSurface(unsigned char *data, const gfxSize& size, long stride, gfxImageFormat format, bool aForPrinting = false);
    gfxQuartzSurface(unsigned char *data, const gfxIntSize& size, long stride, gfxImageFormat format, bool aForPrinting = false);

    virtual ~gfxQuartzSurface();

    virtual already_AddRefed<gfxASurface> CreateSimilarSurface(gfxContentType aType,
                                                               const gfxIntSize& aSize);

    virtual const gfxIntSize GetSize() const { return gfxIntSize(mSize.width, mSize.height); }

    CGContextRef GetCGContext() { return mCGContext; }

    CGContextRef GetCGContextWithClip(gfxContext *ctx);

    virtual int32_t GetDefaultContextFlags() const;

    already_AddRefed<gfxImageSurface> GetAsImageSurface();

    void MovePixels(const nsIntRect& aSourceRect,
                    const nsIntPoint& aDestTopLeft)
    {
        FastMovePixels(aSourceRect, aDestTopLeft);
    }

protected:
    void MakeInvalid();

    CGContextRef mCGContext;
    gfxSize      mSize;
    bool mForPrinting;
};

#endif /* GFX_QUARTZSURFACE_H */
