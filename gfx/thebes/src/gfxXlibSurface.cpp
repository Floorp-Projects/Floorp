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

#include <stdio.h>
#include "gfxXlibSurface.h"

THEBES_IMPL_REFCOUNTING(gfxXlibSurface)

gfxXlibSurface::gfxXlibSurface(Display* dpy, Drawable drawable, Visual* visual)
    : mOwnsPixmap(PR_FALSE), mDisplay(dpy), mDrawable(drawable)
{
    // figure out width/height/depth
    Window root_ignore;
    int x_ignore, y_ignore;
    unsigned int bwidth_ignore, width, height, depth;

    XGetGeometry(dpy,
                 drawable,
                 &root_ignore, &x_ignore, &y_ignore,
                 &width, &height,
                 &bwidth_ignore, &depth);

    mWidth = width;
    mHeight = height;

    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, drawable, visual, width, height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Display* dpy, Drawable drawable, Visual* visual,
                               unsigned long width, unsigned long height)
    : mOwnsPixmap(PR_FALSE), mDisplay(dpy), mDrawable(drawable), mWidth(width), mHeight(height)
{
    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, drawable, visual, width, height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Display* dpy, Visual* visual, unsigned long width, unsigned long height)
    : mOwnsPixmap(PR_TRUE), mDisplay(dpy), mWidth(width), mHeight(height)

{
    mDrawable = (Drawable)XCreatePixmap(dpy,
                                        RootWindow(dpy, DefaultScreen(dpy)),
                                        width, height,
                                        DefaultDepth(dpy, DefaultScreen(dpy)));

    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, mDrawable, visual, width, height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Display* dpy, Drawable drawable, XRenderPictFormat *format,
                               unsigned long width, unsigned long height)
    : mOwnsPixmap(PR_FALSE), mDisplay(dpy), mDrawable(drawable),
      mWidth(width), mHeight(height)
{
    cairo_surface_t *surf = cairo_xlib_surface_create_with_xrender_format (dpy, drawable,
                                                                           ScreenOfDisplay(dpy,DefaultScreen(dpy)),
                                                                           format, width, height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Display* dpy, XRenderPictFormat *format,
                               unsigned long width, unsigned long height)
    : mOwnsPixmap(PR_TRUE), mDisplay(dpy), mWidth(width), mHeight(height)
{
    mDrawable = (Drawable)XCreatePixmap(dpy,
                                        RootWindow(dpy, DefaultScreen(dpy)),
                                        width, height,
                                        format->depth);

    cairo_surface_t *surf = cairo_xlib_surface_create_with_xrender_format (dpy, mDrawable,
                                                                           ScreenOfDisplay(dpy,DefaultScreen(dpy)),
                                                                           format, width, height);
    Init(surf);
}

gfxXlibSurface::~gfxXlibSurface()
{
    Destroy();

    if (mOwnsPixmap)
        XFreePixmap(mDisplay, mDrawable);
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
    }

    return (XRenderPictFormat*)NULL;
}
