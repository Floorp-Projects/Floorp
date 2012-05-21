/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GFXQUARTZNATIVEDRAWING_H_
#define _GFXQUARTZNATIVEDRAWING_H_

#include "mozilla/Attributes.h"

#include "gfxContext.h"
#include "gfxQuartzSurface.h"

class THEBES_API gfxQuartzNativeDrawing {
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
     */
    gfxQuartzNativeDrawing(gfxContext *ctx,
                           const gfxRect& nativeRect);

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

    // saved state
    nsRefPtr<gfxQuartzSurface> mQuartzSurface;
    CGContextRef mCGContext;
};

#endif
