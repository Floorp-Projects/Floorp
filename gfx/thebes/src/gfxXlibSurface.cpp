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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#include "gfxXlibSurface.h"

#include "cairo.h"
#include "cairo-xlib.h"
#include "cairo-xlib-xrender.h"

static cairo_user_data_key_t pixmap_free_key;

typedef struct {
    Display* dpy;
    Pixmap pixmap;
} pixmap_free_struct;

static void pixmap_free_func (void *);

gfxXlibSurface::gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual)
    : mPixmapTaken(PR_FALSE), mDisplay(dpy), mDrawable(drawable)
{
    DoSizeQuery();
    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, drawable, visual, mWidth, mHeight);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual,
                               unsigned long width, unsigned long height)
    : mPixmapTaken(PR_FALSE), mDisplay(dpy), mDrawable(drawable), mWidth(width), mHeight(height)
{
    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, drawable, visual, width, height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Display *dpy, Visual *visual, unsigned long width, unsigned long height)
    : mPixmapTaken(PR_FALSE), mDisplay(dpy), mWidth(width), mHeight(height)

{
    mDrawable = (Drawable)XCreatePixmap(dpy,
                                        RootWindow(dpy, DefaultScreen(dpy)),
                                        width, height,
                                        DefaultDepth(dpy, DefaultScreen(dpy)));

    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, mDrawable, visual, width, height);

    Init(surf);
    TakePixmap();
}

gfxXlibSurface::gfxXlibSurface(Display *dpy, Drawable drawable, XRenderPictFormat *format,
                               unsigned long width, unsigned long height)
    : mPixmapTaken(PR_FALSE), mDisplay(dpy), mDrawable(drawable),
      mWidth(width), mHeight(height)
{
    cairo_surface_t *surf = cairo_xlib_surface_create_with_xrender_format (dpy, drawable,
                                                                           ScreenOfDisplay(dpy,DefaultScreen(dpy)),
                                                                           format, width, height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Display *dpy, XRenderPictFormat *format,
                               unsigned long width, unsigned long height)
    : mPixmapTaken(PR_FALSE), mDisplay(dpy), mWidth(width), mHeight(height)
{
    mDrawable = (Drawable)XCreatePixmap(dpy,
                                        RootWindow(dpy, DefaultScreen(dpy)),
                                        width, height,
                                        format->depth);

    cairo_surface_t *surf = cairo_xlib_surface_create_with_xrender_format (dpy, mDrawable,
                                                                           ScreenOfDisplay(dpy,DefaultScreen(dpy)),
                                                                           format, width, height);
    Init(surf);
    TakePixmap();
}

gfxXlibSurface::gfxXlibSurface(cairo_surface_t *csurf)
    : mPixmapTaken(PR_FALSE), mWidth(-1), mHeight(-1)
{
    mDrawable = cairo_xlib_surface_get_drawable(csurf);
    mDisplay = cairo_xlib_surface_get_display(csurf);

    Init(csurf, PR_TRUE);
}

gfxXlibSurface::~gfxXlibSurface()
{
}

void
gfxXlibSurface::DoSizeQuery()
{
    // figure out width/height/depth
    Window root_ignore;
    int x_ignore, y_ignore;
    unsigned int bwidth_ignore, width, height, depth;

    XGetGeometry(mDisplay,
                 mDrawable,
                 &root_ignore, &x_ignore, &y_ignore,
                 &width, &height,
                 &bwidth_ignore, &depth);

    mWidth = width;
    mHeight = height;
}

XRenderPictFormat*
gfxXlibSurface::FindRenderFormat(Display *dpy, gfxImageFormat format)
{
    switch (format) {
        case ImageFormatARGB32:
            return XRenderFindStandardFormat (dpy, PictStandardARGB32);
            break;
        case ImageFormatRGB24:
            return XRenderFindStandardFormat (dpy, PictStandardRGB24);
            break;
        case ImageFormatA8:
            return XRenderFindStandardFormat (dpy, PictStandardA8);
            break;
        case ImageFormatA1:
            return XRenderFindStandardFormat (dpy, PictStandardA1);
            break;
        default:
            return NULL;
    }

    return (XRenderPictFormat*)NULL;
}

void
gfxXlibSurface::TakePixmap()
{
    if (mPixmapTaken)
        return;

    pixmap_free_struct *pfs = new pixmap_free_struct;
    pfs->dpy = mDisplay;
    pfs->pixmap = mDrawable;

    cairo_surface_set_user_data (CairoSurface(),
                                 &pixmap_free_key,
                                 pfs,
                                 pixmap_free_func);

    mPixmapTaken = PR_TRUE;
}

void
pixmap_free_func (void *data)
{
    pixmap_free_struct *pfs = (pixmap_free_struct*) data;

    XFreePixmap (pfs->dpy, pfs->pixmap);

    delete pfs;
}
