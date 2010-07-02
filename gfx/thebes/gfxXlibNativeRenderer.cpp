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
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   rocallahan@novell.com
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

#include "gfxXlibNativeRenderer.h"
#include "gfxContext.h"

#include "cairo-xlib-utils.h"

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#endif

// Look for an existing Colormap that known to be associated with visual.
static Colormap
LookupColormapForVisual(const Screen* screen, const Visual* visual)
{
    // common case
    if (visual == DefaultVisualOfScreen(screen))
        return DefaultColormapOfScreen(screen);

#ifdef MOZ_WIDGET_GTK2
    // I wish there were a gdk_x11_display_lookup_screen.
    Display* dpy = DisplayOfScreen(screen);
    GdkDisplay* gdkDpy = gdk_x11_lookup_xdisplay(dpy);
    if (gdkDpy) {
        gint screen_num = 0;
        for (int s = 0; s < ScreenCount(dpy); ++s) {
            if (ScreenOfDisplay(dpy, s) == screen) {
                screen_num = s;
                break;
            }
        }
        GdkScreen* gdkScreen = gdk_display_get_screen(gdkDpy, screen_num);

        GdkColormap* gdkColormap = NULL;
        if (visual ==
            GDK_VISUAL_XVISUAL(gdk_screen_get_rgb_visual(gdkScreen))) {
            // widget/src/gtk2/mozcontainer.c uses gdk_rgb_get_colormap()
            // which is inherited by child widgets, so this is the visual
            // expected when drawing directly to widget surfaces or surfaces
            // created using cairo_surface_create_similar with
            // CAIRO_CONTENT_COLOR.
            // gdk_screen_get_rgb_colormap is the generalization of
            // gdk_rgb_get_colormap for any screen.
            gdkColormap = gdk_screen_get_rgb_colormap(gdkScreen);
        }
        else if (visual ==
             GDK_VISUAL_XVISUAL(gdk_screen_get_rgba_visual(gdkScreen))) {
            // This is the visual expected on displays with the Composite
            // extension enabled when the surface has been created using
            // cairo_surface_create_similar with CAIRO_CONTENT_COLOR_ALPHA,
            // as happens with non-unit opacity.
            gdkColormap = gdk_screen_get_rgba_colormap(gdkScreen);
        }
        if (gdkColormap != NULL)
            return GDK_COLORMAP_XCOLORMAP(gdkColormap);
    }
#endif

    return None;
}

typedef struct {
    gfxXlibNativeRenderer* mRenderer;
    nsresult               mRV;
} NativeRenderingClosure;

static cairo_bool_t
NativeRendering(void *closure,
                Screen *screen,
                Drawable drawable,
                Visual *visual,
                short offset_x, short offset_y,
                XRectangle* rectangles, unsigned int num_rects)
{
    // Cairo doesn't provide a Colormap.
    // See if a suitable existing Colormap is known.
    Colormap colormap = LookupColormapForVisual(screen, visual);
    PRBool allocColormap = colormap == None;
    if (allocColormap) {
        // No existing colormap found.
        // This case is not expected with MOZ_WIDGET_GTK2.
        // Create a Colormap for the Visual.
        // This is only really useful for Visual classes with predefined
        // Colormap entries, but cairo would be all confused with
        // non-default non-TrueColor colormaps anyway.
        NS_ASSERTION(visual->c_class == TrueColor ||
                     visual->c_class == StaticColor ||
                     visual->c_class == StaticGray,
                     "Creating empty colormap");
        // If this case were expected then it might be worth considering
        // using a service that maintains a set of Colormaps for associated
        // Visuals so as to avoid repeating the LockDisplay required in
        // XCreateColormap, but then it's no worse than the XCreatePixmap
        // that produced the Drawable here.
        colormap = XCreateColormap(DisplayOfScreen(screen),
                                   RootWindowOfScreen(screen),
                                   visual, AllocNone);
    }

    NativeRenderingClosure* cl = (NativeRenderingClosure*)closure;
    nsresult rv = cl->mRenderer->
        NativeDraw(screen, drawable, visual, colormap, offset_x, offset_y,
                   rectangles, num_rects);
    cl->mRV = rv;

    if (allocColormap) {
        XFreeColormap(DisplayOfScreen(screen), colormap);
    }
    return NS_SUCCEEDED(rv);
}

nsresult
gfxXlibNativeRenderer::Draw(Display* dpy, gfxContext* ctx, int width, int height,
                            PRUint32 flags, DrawOutput* output)
{
    NativeRenderingClosure closure = { this, NS_OK };
    cairo_xlib_drawing_result_t result;
    // Make sure result.surface is null to start with; we rely on it
    // being non-null meaning that a surface actually got allocated.
    result.surface = NULL;

    if (output) {
        output->mSurface = NULL;
        output->mUniformAlpha = PR_FALSE;
        output->mUniformColor = PR_FALSE;
    }

    int cairoFlags = 0;
    if (flags & DRAW_SUPPORTS_OFFSET) {
        cairoFlags |= CAIRO_XLIB_DRAWING_SUPPORTS_OFFSET;
    }
    if (flags & DRAW_SUPPORTS_CLIP_RECT) {
        cairoFlags |= CAIRO_XLIB_DRAWING_SUPPORTS_CLIP_RECT;
    }
    if (flags & DRAW_SUPPORTS_CLIP_LIST) {
        cairoFlags |= CAIRO_XLIB_DRAWING_SUPPORTS_CLIP_LIST;
    }
    if (flags & DRAW_SUPPORTS_ALTERNATE_SCREEN) {
        cairoFlags |= CAIRO_XLIB_DRAWING_SUPPORTS_ALTERNATE_SCREEN;
    }
    if (flags & DRAW_SUPPORTS_NONDEFAULT_VISUAL) {
        cairoFlags |= CAIRO_XLIB_DRAWING_SUPPORTS_NONDEFAULT_VISUAL;
    }
    cairo_draw_with_xlib(ctx->GetCairo(), NativeRendering, &closure, dpy,
                         width, height,
                         (flags & DRAW_IS_OPAQUE) ? CAIRO_XLIB_DRAWING_OPAQUE
                                                  : CAIRO_XLIB_DRAWING_TRANSPARENT,
                         (cairo_xlib_drawing_support_t)cairoFlags,
                         output ? &result : NULL);
    if (NS_FAILED(closure.mRV)) {
        if (result.surface) {
            NS_ASSERTION(output, "How did that happen?");
            cairo_surface_destroy (result.surface);
        }
        return closure.mRV;
    }

    if (output) {
        if (result.surface) {
            output->mSurface = gfxASurface::Wrap(result.surface);
            if (!output->mSurface) {
                cairo_surface_destroy (result.surface);
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }

        output->mUniformAlpha = result.uniform_alpha;
        output->mUniformColor = result.uniform_color;
        output->mColor = gfxRGBA(result.r, result.g, result.b, result.alpha);
    }
  
    return NS_OK;
}
