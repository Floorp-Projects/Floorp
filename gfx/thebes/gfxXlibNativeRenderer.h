/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXXLIBNATIVERENDER_H_
#define GFXXLIBNATIVERENDER_H_

#include "nsPoint.h"
#include "nsRect.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Point.h"
#include <X11/Xlib.h>

namespace mozilla {
namespace gfx {
  class DrawTarget;
}
}

class gfxASurface;
class gfxContext;
typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;

/**
 * This class lets us take code that draws into an X drawable and lets us
 * use it to draw into any Thebes context. The user should subclass this class,
 * override DrawWithXib, and then call Draw(). The drawing will be subjected
 * to all Thebes transformations, clipping etc.
 */
class gfxXlibNativeRenderer {
public:
    /**
     * Perform the native drawing.
     * @param surface the cairo_surface_t for drawing. Must be a cairo_xlib_surface_t.
     *                The extents of this surface do not necessarily cover the
     *                entire rectangle with size provided to Draw().
     * @param offset  draw at this offset into the given drawable
     * @param clipRects an array of rectangles; clip to the union.
     *                  Any rectangles provided will be contained by the
     *                  rectangle with size provided to Draw and by the
     *                  surface extents.
     * @param numClipRects the number of rects in the array, or zero if
     *                     no clipping is required.
     */
    virtual nsresult DrawWithXlib(cairo_surface_t* surface,
                                  mozilla::gfx::IntPoint offset,
                                  mozilla::gfx::IntRect* clipRects,
                                  uint32_t numClipRects) = 0;
  
    enum {
        // If set, then Draw() is opaque, i.e., every pixel in the intersection
        // of the clipRect and (offset.x,offset.y,bounds.width,bounds.height)
        // will be set and there is no dependence on what the existing pixels
        // in the drawable are set to.
        DRAW_IS_OPAQUE = 0x01,
        // If set, then numClipRects can be zero or one
        DRAW_SUPPORTS_CLIP_RECT = 0x04,
        // If set, then numClipRects can be any value. If neither this
        // nor CLIP_RECT are set, then numClipRects will be zero
        DRAW_SUPPORTS_CLIP_LIST = 0x08,
        // If set, then the surface in the callback may have any visual;
        // otherwise the pixels will have the same format as the visual
        // passed to 'Draw'.
        DRAW_SUPPORTS_ALTERNATE_VISUAL = 0x10,
        // If set, then the Screen 'screen' in the callback can be different
        // from the default Screen of the display passed to 'Draw' and can be
        // on a different display.
        DRAW_SUPPORTS_ALTERNATE_SCREEN = 0x20
    };

    /**
     * @param flags see above
     * @param size the size of the rectangle being drawn;
     * the caller guarantees that drawing will not extend beyond the rectangle
     * (0,0,size.width,size.height).
     * @param screen a Screen to use for the drawing if ctx doesn't have one.
     * @param visual a Visual to use for the drawing if ctx doesn't have one.
     * @param result if non-null, we will try to capture a copy of the
     * rendered image into a surface similar to the surface of ctx; if
     * successful, a pointer to the new gfxASurface is stored in *resultSurface,
     * otherwise *resultSurface is set to nullptr.
     */
    void Draw(gfxContext* ctx, mozilla::gfx::IntSize size,
              uint32_t flags, Screen *screen, Visual *visual);

private:
    bool DrawDirect(mozilla::gfx::DrawTarget* aDT, mozilla::gfx::IntSize bounds,
                    uint32_t flags, Screen *screen, Visual *visual);

    bool DrawCairo(cairo_t* cr, mozilla::gfx::IntSize size,
                   uint32_t flags, Screen *screen, Visual *visual);

    void DrawFallback(mozilla::gfx::DrawTarget* dt, gfxContext* ctx,
                      gfxASurface* aSurface, mozilla::gfx::IntSize& size,
                      mozilla::gfx::IntRect& drawingRect, bool canDrawOverBackground,
                      uint32_t flags, Screen* screen, Visual* visual);

    bool DrawOntoTempSurface(cairo_surface_t *tempXlibSurface,
                             mozilla::gfx::IntPoint offset);

};

#endif /*GFXXLIBNATIVERENDER_H_*/
