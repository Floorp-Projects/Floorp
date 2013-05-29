/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GFXWINDOWSNATIVEDRAWING_H_
#define _GFXWINDOWSNATIVEDRAWING_H_

#include <windows.h>

#include "gfxContext.h"
#include "gfxWindowsSurface.h"

class gfxWindowsNativeDrawing {
public:

    /* Flags for notifying this class what kind of operations the native
     * drawing supports
     */

    enum {
        /* Whether the native drawing can draw to a surface of content COLOR_ALPHA */
        CAN_DRAW_TO_COLOR_ALPHA    = 1 << 0,
        CANNOT_DRAW_TO_COLOR_ALPHA = 0 << 0,

        /* Whether the native drawing can be scaled using SetWorldTransform */
        CAN_AXIS_ALIGNED_SCALE     = 1 << 1,
        CANNOT_AXIS_ALIGNED_SCALE  = 0 << 1,

        /* Whether the native drawing can be both scaled and rotated arbitrarily using SetWorldTransform */
        CAN_COMPLEX_TRANSFORM      = 1 << 2,
        CANNOT_COMPLEX_TRANSFORM   = 0 << 2,

        /* If we have to do transforms with cairo, should we use nearest-neighbour filtering? */
        DO_NEAREST_NEIGHBOR_FILTERING = 1 << 3,
        DO_BILINEAR_FILTERING         = 0 << 3
    };

    /* Create native win32 drawing for a rectangle bounded by
     * nativeRect.
     *
     * Typical usage looks like:
     *
     *   gfxWindowsNativeDrawing nativeDraw(ctx, destGfxRect, capabilities);
     *   do {
     *     HDC dc = nativeDraw.BeginNativeDrawing();
     *     if (!dc)
     *       return NS_ERROR_FAILURE;
     *
     *     RECT winRect;
     *     nativeDraw.TransformToNativeRect(rect, winRect);
     *
     *       ... call win32 operations on HDC to draw to winRect ...
     *
     *     nativeDraw.EndNativeDrawing();
     *   } while (nativeDraw.ShouldRenderAgain());
     *   nativeDraw.PaintToContext();
     */
    gfxWindowsNativeDrawing(gfxContext *ctx,
                            const gfxRect& nativeRect,
                            uint32_t nativeDrawFlags = CANNOT_DRAW_TO_COLOR_ALPHA |
                                                       CANNOT_AXIS_ALIGNED_SCALE |
                                                       CANNOT_COMPLEX_TRANSFORM |
                                                       DO_BILINEAR_FILTERING);

    /* Returns a HDC which may be used for native drawing.  This HDC is valid
     * until EndNativeDrawing is called; if it is used for drawing after that time,
     * the result is undefined. */
    HDC BeginNativeDrawing();

    /* Transform the native rect into something valid for rendering
     * to the HDC.  This may or may not change RECT, depending on
     * whether SetWorldTransform is used or not. */
    void TransformToNativeRect(const gfxRect& r, RECT& rout);

    /* Marks the end of native drawing */
    void EndNativeDrawing();

    /* Returns true if the native drawing should be executed again */
    bool ShouldRenderAgain();

    /* Returns true if double pass alpha extraction is taking place. */
    bool IsDoublePass();

    /* Places the result to the context, if necessary */
    void PaintToContext();

private:

    nsRefPtr<gfxContext> mContext;
    gfxRect mNativeRect;
    uint32_t mNativeDrawFlags;

    // what state the rendering is in
    uint8_t mRenderState;

    gfxPoint mDeviceOffset;
    nsRefPtr<gfxPattern> mBlackPattern, mWhitePattern;

    enum TransformType {
        TRANSLATION_ONLY,
        AXIS_ALIGNED_SCALE,
        COMPLEX
    };

    TransformType mTransformType;
    gfxPoint mTranslation;
    gfxSize mScale;
    XFORM mWorldTransform;

    // saved state
    nsRefPtr<gfxWindowsSurface> mWinSurface, mBlackSurface, mWhiteSurface;
    HDC mDC;
    XFORM mOldWorldTransform;
    POINT mOrigViewportOrigin;
    gfxIntSize mTempSurfaceSize;
};

#endif
