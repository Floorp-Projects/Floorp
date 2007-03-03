/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Thebes gfx.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _GFXWINDOWSNATIVEDRAWING_H_
#define _GFXWINDOWSNATIVEDRAWING_H_

#include <windows.h>

#include "gfxContext.h"
#include "gfxWindowsSurface.h"

class THEBES_API gfxWindowsNativeDrawing {
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
     * This class assumes that native drawing can take place only if
     * the destination surface has a content type of COLOR (that is,
     * RGB24), and that the transformation matrix consists of only a
     * translation (in which case the coordinates are munged directly)
     * or a translation and scale (in which case SetWorldTransform is used).
     *
     * If the destination is of a non-win32 surface type, a win32
     * surface of content COLOR_ALPHA, or if there is a complex
     * transform (i.e., one with rotation) set, then the native drawing
     * code will fall back to alpha recovery, but will still take advantage
     * of native axis-aligned scaling.
     *
     * Typical usage looks like:
     *
     *   gfxWindowsNativeDrawing nativeDraw(ctx, destGfxRect);
     *   do {
     *     HDC dc = nativeDraw.BeginNativeDrawing();
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
                            PRUint32 nativeDrawFlags = CANNOT_DRAW_TO_COLOR_ALPHA |
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

    /* Returns PR_TRUE if the native drawing should be executed again */
    PRBool ShouldRenderAgain();

    /* Places the result to the context, if necessary */
    void PaintToContext();

private:

    nsRefPtr<gfxContext> mContext;
    gfxRect mNativeRect;
    PRUint32 mNativeDrawFlags;

    // what state the rendering is in
    PRUint8 mRenderState;

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
