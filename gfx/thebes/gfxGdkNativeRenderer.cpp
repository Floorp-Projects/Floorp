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
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
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

#include "gfxGdkNativeRenderer.h"
#include "gfxContext.h"
#include "gfxPlatformGtk.h"

#ifdef MOZ_X11
#include <gdk/gdkx.h>
#include "cairo-xlib.h"
#include "gfxXlibSurface.h"
nsresult
gfxGdkNativeRenderer::DrawWithXlib(gfxXlibSurface* surface,
                                   nsIntPoint offset,
                                   nsIntRect* clipRects, PRUint32 numClipRects)
{
    GdkDrawable *drawable = gfxPlatformGtk::GetGdkDrawable(surface);
    if (!drawable) {
        gfxIntSize size = surface->GetSize();
        int depth = cairo_xlib_surface_get_depth(surface->CairoSurface());
        GdkScreen* screen = gdk_colormap_get_screen(mColormap);
        drawable =
            gdk_pixmap_foreign_new_for_screen(screen, surface->XDrawable(),
                                              size.width, size.height, depth);
        if (!drawable)
            return NS_ERROR_FAILURE;

        gdk_drawable_set_colormap(drawable, mColormap);
        gfxPlatformGtk::SetGdkDrawable(surface, drawable);
        g_object_unref(drawable); // The drawable now belongs to |surface|.
    }
    
    GdkRectangle clipRect;
    if (numClipRects) {
        NS_ASSERTION(numClipRects == 1, "Too many clip rects");
        clipRect.x = clipRects[0].x;
        clipRect.y = clipRects[0].y;
        clipRect.width = clipRects[0].width;
        clipRect.height = clipRects[0].height;
    }

    return DrawWithGDK(drawable, offset.x, offset.y,
                       numClipRects ? &clipRect : NULL, numClipRects);
}

void
gfxGdkNativeRenderer::Draw(gfxContext* ctx, nsIntSize size,
                           PRUint32 flags, GdkColormap* colormap)
{
    mColormap = colormap;

    Visual* visual =
        gdk_x11_visual_get_xvisual(gdk_colormap_get_visual(colormap));
    Screen* screen =
        gdk_x11_screen_get_xscreen(gdk_colormap_get_screen(colormap));

    gfxXlibNativeRenderer::Draw(ctx, size, flags, screen, visual, nsnull);
}

#endif
