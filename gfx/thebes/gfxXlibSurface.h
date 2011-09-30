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

#ifndef GFX_XLIBSURFACE_H
#define GFX_XLIBSURFACE_H

#include "gfxASurface.h"

#include <X11/extensions/Xrender.h>
#include <X11/Xlib.h>

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
#include "GLXLibrary.h"
#endif

class THEBES_API gfxXlibSurface : public gfxASurface {
public:
    // construct a wrapper around the specified drawable with dpy/visual.
    // Will use XGetGeometry to query the window/pixmap size.
    gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual);

    // construct a wrapper around the specified drawable with dpy/visual,
    // and known width/height.
    gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual, const gfxIntSize& size);

    // construct a wrapper around the specified drawable with dpy/format,
    // and known width/height.
    gfxXlibSurface(Screen *screen, Drawable drawable, XRenderPictFormat *format,
                   const gfxIntSize& size);

    gfxXlibSurface(cairo_surface_t *csurf);

    // create a new Pixmap and wrapper surface.
    // |relatedDrawable| provides a hint to the server for determining whether
    // the pixmap should be in video or system memory.  It must be on
    // |screen| (if specified).
    static already_AddRefed<gfxXlibSurface>
    Create(Screen *screen, Visual *visual, const gfxIntSize& size,
           Drawable relatedDrawable = None);
    static already_AddRefed<gfxXlibSurface>
    Create(Screen* screen, XRenderPictFormat *format, const gfxIntSize& size,
           Drawable relatedDrawable = None);

    virtual ~gfxXlibSurface();

    virtual already_AddRefed<gfxASurface>
    CreateSimilarSurface(gfxContentType aType, const gfxIntSize& aSize);

    virtual const gfxIntSize GetSize() const { return mSize; }

    Display* XDisplay() { return mDisplay; }
    Screen* XScreen();
    Drawable XDrawable() { return mDrawable; }
    XRenderPictFormat* XRenderFormat();

    static int DepthOfVisual(const Screen* screen, const Visual* visual);
    static Visual* FindVisual(Screen* screen, gfxImageFormat format);
    static XRenderPictFormat *FindRenderFormat(Display *dpy, gfxImageFormat format);

    // take ownership of a passed-in Pixmap, calling XFreePixmap on it
    // when the gfxXlibSurface is destroyed.
    void TakePixmap();

    // Release ownership of this surface's Pixmap.  This is only valid
    // on gfxXlibSurfaces for which the user called TakePixmap(), or
    // on those created by a Create() factory method.
    Drawable ReleasePixmap();

    // Find a visual and colormap pair suitable for rendering to this surface.
    bool GetColormapAndVisual(Colormap* colormap, Visual **visual);

    // This surface is a wrapper around X pixmaps, which are stored in the X
    // server, not the main application.
    virtual gfxASurface::MemoryLocation GetMemoryLocation() const;

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
    GLXPixmap GetGLXPixmap();
#endif

protected:
    // if TakePixmap() has been called on this
    bool mPixmapTaken;
    
    Display *mDisplay;
    Drawable mDrawable;

    void DoSizeQuery();

    gfxIntSize mSize;

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
    GLXPixmap mGLXPixmap;
#endif
};

#endif /* GFX_XLIBSURFACE_H */
