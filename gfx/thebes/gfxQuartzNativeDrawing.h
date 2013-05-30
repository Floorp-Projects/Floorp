/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GFXQUARTZNATIVEDRAWING_H_
#define _GFXQUARTZNATIVEDRAWING_H_

#include "mozilla/Attributes.h"

#include "gfxContext.h"
#include "gfxQuartzSurface.h"

class gfxQuartzNativeDrawing {
public:

    /* Create native Quartz drawing for a rectangle bounded by
     * nativeRect.
     *
     * Typical usage looks like:
     *
     *   gfxQuartzNativeDrawing nativeDraw(ctx, nativeRect);
     *   CGContextRef cgContext = nativeDraw.BeginNativeDrawing();
     *   if (!cgContext)
     *     return NS_ERROR_FAILURE;
     *
     *     ... call Quartz operations on CGContextRef to draw to nativeRect ...
     *
     *   nativeDraw.EndNativeDrawing();
     *
     * aNativeRect is the size of the surface (in Quartz/Cocoa points) that
     * will be created _if_ the gfxQuartzNativeDrawing decides to create a new
     * surface and CGContext for its drawing operations, which it then
     * composites into the target gfxContext.
     *
     * (Note that aNativeRect will be ignored if the gfxQuartzNativeDrawing
     * uses the target gfxContext directly.)
     *
     * The optional aBackingScale parameter is a scaling factor that will be
     * applied when creating and rendering into such a temporary surface.
     */
    gfxQuartzNativeDrawing(gfxContext *ctx,
                           const gfxRect& aNativeRect,
                           gfxFloat aBackingScale = 1.0f);

    /* Returns a CGContextRef which may be used for native drawing.  This
     * CGContextRef is valid until EndNativeDrawing is called; if it is used
     * for drawing after that time, the result is undefined. */
    CGContextRef BeginNativeDrawing();

    /* Marks the end of native drawing */
    void EndNativeDrawing();

private:
    // don't allow copying via construction or assignment
    gfxQuartzNativeDrawing(const gfxQuartzNativeDrawing&) MOZ_DELETE;
    const gfxQuartzNativeDrawing& operator=(const gfxQuartzNativeDrawing&) MOZ_DELETE;

    // Final destination context
    nsRefPtr<gfxContext> mContext;
    // context that draws to mQuartzSurface; can be different from mContext
    // if mContext is not drawing to Quartz
    nsRefPtr<gfxContext> mSurfaceContext;
    gfxRect mNativeRect;
    gfxFloat mBackingScale;

    // saved state
    nsRefPtr<gfxQuartzSurface> mQuartzSurface;
    CGContextRef mCGContext;
};

#endif
