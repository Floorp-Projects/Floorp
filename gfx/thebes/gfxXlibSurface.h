/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_XLIBSURFACE_H
#define GFX_XLIBSURFACE_H

#include "gfxASurface.h"

#include <X11/extensions/Xrender.h>
#include <X11/Xlib.h>
#include "X11UndefineNone.h"

#include "GLXLibrary.h"

#include "nsSize.h"

// Although the dimension parameters in the xCreatePixmapReq wire protocol are
// 16-bit unsigned integers, the server's CreatePixmap returns BadAlloc if
// either dimension cannot be represented by a 16-bit *signed* integer.
#define XLIB_IMAGE_SIDE_SIZE_LIMIT 0x7fff


class gfxXlibSurface final : public gfxASurface {
public:
    // construct a wrapper around the specified drawable with dpy/visual.
    // Will use XGetGeometry to query the window/pixmap size.
    gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual);

    // construct a wrapper around the specified drawable with dpy/visual,
    // and known width/height.
    gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual, const mozilla::gfx::IntSize& size);

    // construct a wrapper around the specified drawable with dpy/format,
    // and known width/height.
    gfxXlibSurface(::Screen *screen, Drawable drawable, XRenderPictFormat *format,
                   const mozilla::gfx::IntSize& size);

    explicit gfxXlibSurface(cairo_surface_t *csurf);

    // create a new Pixmap and wrapper surface.
    // |relatedDrawable| provides a hint to the server for determining whether
    // the pixmap should be in video or system memory.  It must be on
    // |screen| (if specified).
    static already_AddRefed<gfxXlibSurface>
    Create(::Screen *screen, Visual *visual, const mozilla::gfx::IntSize& size,
           Drawable relatedDrawable = X11None);
    static cairo_surface_t *
    CreateCairoSurface(::Screen *screen, Visual *visual, const mozilla::gfx::IntSize& size,
                       Drawable relatedDrawable = X11None);
    static already_AddRefed<gfxXlibSurface>
    Create(::Screen* screen, XRenderPictFormat *format, const mozilla::gfx::IntSize& size,
           Drawable relatedDrawable = X11None);

    virtual ~gfxXlibSurface();

    virtual already_AddRefed<gfxASurface>
    CreateSimilarSurface(gfxContentType aType,
                         const mozilla::gfx::IntSize& aSize) override;
    virtual void Finish() override;

    virtual const mozilla::gfx::IntSize GetSize() const override;

    Display* XDisplay() { return mDisplay; }
    ::Screen* XScreen();
    Drawable XDrawable() { return mDrawable; }
    XRenderPictFormat* XRenderFormat();

    static int DepthOfVisual(const ::Screen* screen, const Visual* visual);
    static Visual* FindVisual(::Screen* screen, gfxImageFormat format);
    static XRenderPictFormat *FindRenderFormat(Display *dpy, gfxImageFormat format);
    static bool GetColormapAndVisual(cairo_surface_t* aXlibSurface, Colormap* colormap, Visual **visual);

    // take ownership of a passed-in Pixmap, calling XFreePixmap on it
    // when the gfxXlibSurface is destroyed.
    void TakePixmap();

    // Release ownership of this surface's Pixmap.  This is only valid
    // on gfxXlibSurfaces for which the user called TakePixmap(), or
    // on those created by a Create() factory method.
    Drawable ReleasePixmap();

    // Find a visual and colormap pair suitable for rendering to this surface.
    bool GetColormapAndVisual(Colormap* colormap, Visual **visual);

    GLXPixmap GetGLXPixmap();
    // Binds a GLXPixmap backed by this context's surface.
    // Primarily for use in sharing surfaces.
    void BindGLXPixmap(GLXPixmap aPixmap);

    // Return true if cairo will take its slow path when this surface is used
    // in a pattern with EXTEND_PAD.  As a workaround for XRender's RepeatPad
    // not being implemented correctly on old X servers, cairo avoids XRender
    // and instead reads back to perform EXTEND_PAD with pixman.  Cairo does
    // this for servers older than xorg-server 1.7.
    bool IsPadSlow() {
        // The test here matches that for buggy_pad_reflect in
        // _cairo_xlib_device_create.
        return VendorRelease(mDisplay) >= 60700000 ||
            VendorRelease(mDisplay) < 10699000;
    }

protected:
    // if TakePixmap() has been called on this
    bool mPixmapTaken;
    
    Display *mDisplay;
    Drawable mDrawable;

    const mozilla::gfx::IntSize DoSizeQuery();

    GLXPixmap mGLXPixmap;
};

#endif /* GFX_XLIBSURFACE_H */
