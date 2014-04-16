/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXGDKNATIVERENDER_H_
#define GFXGDKNATIVERENDER_H_

#include <gdk/gdk.h>
#include "nsSize.h"
#ifdef MOZ_X11
#include "gfxXlibNativeRenderer.h"
#endif

class gfxContext;

/**
 * This class lets us take code that draws into an GDK drawable and lets us
 * use it to draw into any Thebes context. The user should subclass this class,
 * override DrawWithGDK, and then call Draw(). The drawing will be subjected
 * to all Thebes transformations, clipping etc.
 */
class gfxGdkNativeRenderer
#ifdef MOZ_X11
    : private gfxXlibNativeRenderer
#endif
{
public:
    /**
     * Perform the native drawing.
     * @param offsetX draw at this offset into the given drawable
     * @param offsetY draw at this offset into the given drawable
     * @param clipRects an array of rects; clip to the union
     * @param numClipRects the number of rects in the array, or zero if
     * no clipping is required
     */

#if (MOZ_WIDGET_GTK == 2)
    virtual nsresult DrawWithGDK(GdkDrawable * drawable, gint offsetX, 
            gint offsetY, GdkRectangle * clipRects, uint32_t numClipRects) = 0;
#endif

    enum {
        // If set, then Draw() is opaque, i.e., every pixel in the intersection
        // of the clipRect and (offset.x,offset.y,bounds.width,bounds.height)
        // will be set and there is no dependence on what the existing pixels
        // in the drawable are set to.
        DRAW_IS_OPAQUE =
#ifdef MOZ_X11
            gfxXlibNativeRenderer::DRAW_IS_OPAQUE
#else
            0x1
#endif
        // If set, then numClipRects can be zero or one.
        // If not set, then numClipRects will be zero.
        , DRAW_SUPPORTS_CLIP_RECT =
#ifdef MOZ_X11
            gfxXlibNativeRenderer::DRAW_SUPPORTS_CLIP_RECT
#else
            0x2
#endif
    };

    /**
     * @param flags see above
     * @param bounds Draw()'s drawing is guaranteed to be restricted to
     * the rectangle (offset.x,offset.y,bounds.width,bounds.height)
     * @param dpy a display to use for the drawing if ctx doesn't have one
     */
#if (MOZ_WIDGET_GTK == 2)
    void Draw(gfxContext* ctx, nsIntSize size,
              uint32_t flags, GdkColormap* colormap);
#endif

private:
#ifdef MOZ_X11
    // for gfxXlibNativeRenderer:
    virtual nsresult DrawWithXlib(cairo_surface_t* surface,
                                  nsIntPoint offset,
                                  nsIntRect* clipRects, uint32_t numClipRects) MOZ_OVERRIDE;

#if (MOZ_WIDGET_GTK == 2)
    GdkColormap *mColormap;
#endif
#endif
};

#endif /*GFXGDKNATIVERENDER_H_*/
