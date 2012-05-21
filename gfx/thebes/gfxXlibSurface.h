/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
