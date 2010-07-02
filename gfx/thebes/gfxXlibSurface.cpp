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

// Although the dimension parameters in the xCreatePixmapReq wire protocol are
// 16-bit unsigned integers, the server's CreatePixmap returns BadAlloc if
// either dimension cannot be represented by a 16-bit *signed* integer.
#define XLIB_IMAGE_SIDE_SIZE_LIMIT 0x7fff

gfxXlibSurface::gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual)
    : mPixmapTaken(PR_FALSE), mDisplay(dpy), mDrawable(drawable)
{
    DoSizeQuery();
    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, drawable, visual, mSize.width, mSize.height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual, const gfxIntSize& size)
    : mPixmapTaken(PR_FALSE), mDisplay(dpy), mDrawable(drawable), mSize(size)
{
    NS_ASSERTION(CheckSurfaceSize(size, XLIB_IMAGE_SIDE_SIZE_LIMIT),
                 "Bad size");

    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, drawable, visual, mSize.width, mSize.height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Screen *screen, Drawable drawable, XRenderPictFormat *format,
                               const gfxIntSize& size)
    : mPixmapTaken(PR_FALSE), mDisplay(DisplayOfScreen(screen)),
      mDrawable(drawable), mSize(size)
{
    NS_ASSERTION(CheckSurfaceSize(size, XLIB_IMAGE_SIDE_SIZE_LIMIT),
                 "Bad Size");

    cairo_surface_t *surf =
        cairo_xlib_surface_create_with_xrender_format(mDisplay, drawable,
                                                      screen, format,
                                                      mSize.width, mSize.height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(cairo_surface_t *csurf)
    : mPixmapTaken(PR_FALSE),
      mSize(cairo_xlib_surface_get_width(csurf),
            cairo_xlib_surface_get_height(csurf))
{
    NS_PRECONDITION(cairo_surface_status(csurf) == 0,
                    "Not expecting an error surface");

    mDrawable = cairo_xlib_surface_get_drawable(csurf);
    mDisplay = cairo_xlib_surface_get_display(csurf);

    Init(csurf, PR_TRUE);
}

gfxXlibSurface::~gfxXlibSurface()
{
    if (mPixmapTaken) {
        XFreePixmap (mDisplay, mDrawable);
    }
}

static Drawable
CreatePixmap(Screen *screen, const gfxIntSize& size, unsigned int depth,
             Drawable relatedDrawable)
{
    if (!gfxASurface::CheckSurfaceSize(size, XLIB_IMAGE_SIDE_SIZE_LIMIT))
        return None;

    if (relatedDrawable == None) {
        relatedDrawable = RootWindowOfScreen(screen);
    }
    Display *dpy = DisplayOfScreen(screen);
    return XCreatePixmap(dpy, relatedDrawable,
                         size.width, size.height, depth);
}

/* static */
already_AddRefed<gfxXlibSurface>
gfxXlibSurface::Create(Screen *screen, Visual *visual,
                       const gfxIntSize& size, Drawable relatedDrawable)
{
    Drawable drawable =
        CreatePixmap(screen, size, DepthOfVisual(screen, visual),
                     relatedDrawable);
    if (!drawable)
        return nsnull;

    nsRefPtr<gfxXlibSurface> result =
        new gfxXlibSurface(DisplayOfScreen(screen), drawable, visual, size);
    result->TakePixmap();

    if (result->CairoStatus() != 0)
        return nsnull;

    return result.forget();
}

/* static */
already_AddRefed<gfxXlibSurface>
gfxXlibSurface::Create(Screen *screen, XRenderPictFormat *format,
                       const gfxIntSize& size, Drawable relatedDrawable)
{
    Drawable drawable =
        CreatePixmap(screen, size, format->depth, relatedDrawable);
    if (!drawable)
        return nsnull;

    nsRefPtr<gfxXlibSurface> result =
        new gfxXlibSurface(screen, drawable, format, size);
    result->TakePixmap();

    if (result->CairoStatus() != 0)
        return nsnull;

    return result.forget();
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

    mSize.width = width;
    mSize.height = height;
}

/* static */
int
gfxXlibSurface::DepthOfVisual(const Screen* screen, const Visual* visual)
{
    for (int d = 0; d < screen->ndepths; d++) {
        const Depth& d_info = screen->depths[d];
        if (visual >= &d_info.visuals[0]
            && visual < &d_info.visuals[d_info.nvisuals])
            return d_info.depth;
    }

    NS_ERROR("Visual not on Screen.");
    return 0;
}
    
/* static */
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
        case ImageFormatRGB16_565: {
            // PictStandardRGB16_565 is not standard Xrender format
            // we should try to find related visual
            // and find xrender format by visual
            Visual *visual = NULL;
            Screen *screen = DefaultScreenOfDisplay(dpy);
            int j;
            for (j = 0; j < screen->ndepths; j++) {
                Depth *d = &screen->depths[j];
                if (d->depth == 16 && d->nvisuals && &d->visuals[0]) {
                    if (d->visuals[0].red_mask   == 0xf800 &&
                        d->visuals[0].green_mask == 0x7e0 &&
                        d->visuals[0].blue_mask  == 0x1f)
                        visual = &d->visuals[0];
                    break;
                }
            }
            if (!visual)
                return NULL;
            return XRenderFindVisualFormat(dpy, visual);
            break;
        }
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
