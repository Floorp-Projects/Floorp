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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#include "gfxPlatformGtk.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "gfxImageSurface.h"
#include "gfxXlibSurface.h"

#ifdef MOZ_ENABLE_GLITZ
#include "gfxGlitzSurface.h"
#include "glitz-glx.h"
#endif

static cairo_user_data_key_t cairo_gdk_window_key;
static cairo_user_data_key_t cairo_gdk_pixmap_key;
static void do_gdk_pixmap_unref (void *data)
{
    GdkPixmap *pmap = (GdkPixmap*)data;
    gdk_pixmap_unref (pmap);
}

gfxPlatformGtk::gfxPlatformGtk()
{
#ifdef MOZ_ENABLE_GLITZ
    if (UseGlitz())
        glitz_glx_init(NULL);
#endif
}

gfxASurface*
gfxPlatformGtk::CreateOffscreenSurface (PRUint32 width,
                                        PRUint32 height,
                                        gfxASurface::gfxImageFormat imageFormat,
                                        PRBool fastPixelAccess)
{
    gfxASurface *newSurface = nsnull;

    if (fastPixelAccess) {
        newSurface = new gfxImageSurface(imageFormat, width, height);
    } else {
        int bpp, glitzf;
        switch (imageFormat) {
            case gfxASurface::ImageFormatARGB32:
                bpp = 24;
                glitzf = 0; // GLITZ_STANDARD_ARGB32;
                break;
            case gfxASurface::ImageFormatRGB24:
                bpp = 24;
                glitzf = 1; // GLITZ_STANDARD_RGB24;
                break;
            case gfxASurface::ImageFormatA8:
                bpp = 8;
                glitzf = 2; // GLITZ_STANDARD_A8;
            case gfxASurface::ImageFormatA1:
                bpp = 1;
                glitzf = 3; // GLITZ_STANDARD_A1;
                break;
            default:
                return nsnull;
        }

        if (!UseGlitz()) {
            // XXX do we always want 24 here?  We don't really have any other choice..
            GdkPixmap *pixmap = ::gdk_pixmap_new(nsnull, width, height, bpp);
            gdk_drawable_set_colormap(GDK_DRAWABLE(pixmap), gdk_rgb_get_colormap());

            newSurface = new gfxXlibSurface(GDK_WINDOW_XDISPLAY(GDK_DRAWABLE(pixmap)),
                                            GDK_WINDOW_XWINDOW(GDK_DRAWABLE(pixmap)),
                                            GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(GDK_DRAWABLE(pixmap))));

            // set up the surface to auto-unref the gdk pixmap when the surface
            // is released
            cairo_surface_set_user_data (newSurface->CairoSurface(),
                                         &cairo_gdk_pixmap_key,
                                         pixmap,
                                         do_gdk_pixmap_unref);
        } else {
#ifdef MOZ_ENABLE_GLITZ
            glitz_drawable_format_t *gdformat = glitz_glx_find_pbuffer_format
                (GDK_DISPLAY(),
                 gdk_x11_get_default_screen(),
                 0, NULL, 0);

            glitz_drawable_t *gdraw =
                glitz_glx_create_pbuffer_drawable (GDK_DISPLAY(),
                                                   DefaultScreen(GDK_DISPLAY()),
                                                   gdformat,
                                                   width,
                                                   height);
            glitz_format_t *gformat =
                glitz_find_standard_format (gdraw, (glitz_format_name_t) glitzf);

            glitz_surface_t *gsurf =
                glitz_surface_create (gdraw,
                                      gformat,
                                      width,
                                      height,
                                      0,
                                      NULL);

            glitz_surface_attach (gsurf, gdraw, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);
            newSurface = new gfxGlitzSurface (gdraw, gsurf, PR_TRUE);
#endif
        }
    }

    return newSurface;
}

GdkDrawable*
gfxPlatformGtk::GetSurfaceGdkDrawable(gfxASurface *aSurf)
{
    GdkDrawable *gd;
    gd = (GdkDrawable*) cairo_surface_get_user_data(aSurf->CairoSurface(), &cairo_gdk_pixmap_key);
    if (gd)
        return gd;

    gd = (GdkDrawable*) cairo_surface_get_user_data(aSurf->CairoSurface(), &cairo_gdk_window_key);
    if (gd)
        return gd;

    return nsnull;
}

void
gfxPlatformGtk::SetSurfaceGdkWindow(gfxASurface *aSurf,
                                    GdkWindow *win)
{
    cairo_surface_set_user_data (aSurf->CairoSurface(),
                                 &cairo_gdk_window_key,
                                 win,
                                 nsnull);
}
