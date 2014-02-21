/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXQTNATIVERENDER_H_
#define GFXQTNATIVERENDER_H_

#include "gfxColor.h"
#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxXlibSurface.h"

class QRect;
struct nsIntRect;

/**
 * This class lets us take code that draws into an Xlib surface drawable and lets us
 * use it to draw into any Thebes context. The user should subclass this class,
 * override NativeDraw, and then call Draw(). The drawing will be subjected
 * to all Thebes transformations, clipping etc.
 */
class gfxQtNativeRenderer {
public:
    /**
     * Perform the native drawing.
     * @param offsetX draw at this offset into the given drawable
     * @param offsetY draw at this offset into the given drawable
     * @param clipRects an array of rects; clip to the union
     * @param numClipRects the number of rects in the array, or zero if
     * no clipping is required
     */
    virtual nsresult DrawWithXlib(cairo_surface_t* surface,
                                  nsIntPoint offset,
                                  nsIntRect* clipRects, uint32_t numClipRects) = 0;
  
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
        // If set, then the visual passed in can be any visual, otherwise the
        // visual passed in must be the default visual for dpy's default screen
        DRAW_SUPPORTS_ALTERNATE_VISUAL = 0x10,
        // If set, then the Screen 'screen' in the callback can be different
        // from the default Screen of the display passed to 'Draw' and can be
        // on a different display.
        DRAW_SUPPORTS_ALTERNATE_SCREEN = 0x20
    };

    /**
     * @param flags see above
     * @param size Draw()'s drawing is guaranteed to be restricted to
     * the rectangle (offset.x,offset.y,size.width,size.height)
     * @param dpy a display to use for the drawing if ctx doesn't have one
     * @param resultSurface if non-null, we will try to capture a copy of the
     * rendered image into a surface similar to the surface of ctx; if
     * successful, a pointer to the new gfxASurface is stored in *resultSurface,
     * otherwise *resultSurface is set to nullptr.
     */
    nsresult Draw(gfxContext* ctx, nsIntSize size,
                  uint32_t flags, Screen* screen, Visual* visual);
};

#endif /*GFXQTNATIVERENDER_H_*/
