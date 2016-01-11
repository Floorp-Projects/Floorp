/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxXlibSurface.h"

#include "cairo.h"
#include "cairo-xlib.h"
#include "cairo-xlib-xrender.h"
#include <X11/Xlibint.h>  /* For XESetCloseDisplay */
#undef max // Xlibint.h defines this and it breaks std::max
#undef min // Xlibint.h defines this and it breaks std::min

#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsAlgorithm.h"
#include "mozilla/Preferences.h"
#include <algorithm>
#include "mozilla/CheckedInt.h"

using namespace mozilla;

gfxXlibSurface::gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual)
    : mPixmapTaken(false), mDisplay(dpy), mDrawable(drawable)
#if defined(GL_PROVIDER_GLX)
    , mGLXPixmap(None)
#endif
{
    const gfx::IntSize size = DoSizeQuery();
    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, drawable, visual, size.width, size.height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual, const gfx::IntSize& size)
    : mPixmapTaken(false), mDisplay(dpy), mDrawable(drawable)
#if defined(GL_PROVIDER_GLX)
    , mGLXPixmap(None)
#endif
{
    NS_ASSERTION(CheckSurfaceSize(size, XLIB_IMAGE_SIDE_SIZE_LIMIT),
                 "Bad size");

    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, drawable, visual, size.width, size.height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Screen *screen, Drawable drawable, XRenderPictFormat *format,
                               const gfx::IntSize& size)
    : mPixmapTaken(false), mDisplay(DisplayOfScreen(screen)),
      mDrawable(drawable)
#if defined(GL_PROVIDER_GLX)
      , mGLXPixmap(None)
#endif
{
    NS_ASSERTION(CheckSurfaceSize(size, XLIB_IMAGE_SIDE_SIZE_LIMIT),
                 "Bad Size");

    cairo_surface_t *surf =
        cairo_xlib_surface_create_with_xrender_format(mDisplay, drawable,
                                                      screen, format,
                                                      size.width, size.height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(cairo_surface_t *csurf)
    : mPixmapTaken(false)
#if defined(GL_PROVIDER_GLX)
      , mGLXPixmap(None)
#endif
{
    NS_PRECONDITION(cairo_surface_status(csurf) == 0,
                    "Not expecting an error surface");

    mDrawable = cairo_xlib_surface_get_drawable(csurf);
    mDisplay = cairo_xlib_surface_get_display(csurf);

    Init(csurf, true);
}

gfxXlibSurface::~gfxXlibSurface()
{
    // gfxASurface's destructor calls RecordMemoryFreed().
    if (mPixmapTaken) {
#if defined(GL_PROVIDER_GLX)
        if (mGLXPixmap) {
            gl::sGLXLibrary.DestroyPixmap(mDisplay, mGLXPixmap);
        }
#endif
        XFreePixmap (mDisplay, mDrawable);
    }
}

static Drawable
CreatePixmap(Screen *screen, const gfx::IntSize& size, unsigned int depth,
             Drawable relatedDrawable)
{
    if (!gfxASurface::CheckSurfaceSize(size, XLIB_IMAGE_SIDE_SIZE_LIMIT))
        return None;

    if (relatedDrawable == None) {
        relatedDrawable = RootWindowOfScreen(screen);
    }
    Display *dpy = DisplayOfScreen(screen);
    // X gives us a fatal error if we try to create a pixmap of width
    // or height 0
    return XCreatePixmap(dpy, relatedDrawable,
                         std::max(1, size.width), std::max(1, size.height),
                         depth);
}

void
gfxXlibSurface::TakePixmap()
{
    NS_ASSERTION(!mPixmapTaken, "I already own the Pixmap!");
    mPixmapTaken = true;

    // The bit depth returned from Cairo is technically int, but this is
    // the last place we'd be worried about that scenario.
    unsigned int bitDepth = cairo_xlib_surface_get_depth(CairoSurface());
    MOZ_ASSERT((bitDepth % 8) == 0, "Memory used not recorded correctly");    

    // Divide by 8 because surface_get_depth gives us the number of *bits* per
    // pixel.
    gfx::IntSize size = GetSize();
    CheckedInt32 totalBytes = CheckedInt32(size.width) * CheckedInt32(size.height) * (bitDepth/8);

    // Don't do anything in the "else" case.  We could add INT32_MAX, but that
    // would overflow the memory used counter.  It would also mean we tried for
    // a 2G image.  For now, we'll just assert,
    MOZ_ASSERT(totalBytes.isValid(),"Did not expect to exceed 2Gb image");
    if (totalBytes.isValid()) {
        RecordMemoryUsed(totalBytes.value());
    }
}

Drawable
gfxXlibSurface::ReleasePixmap() {
    NS_ASSERTION(mPixmapTaken, "I don't own the Pixmap!");
    mPixmapTaken = false;
    RecordMemoryFreed();
    return mDrawable;
}

static cairo_user_data_key_t gDestroyPixmapKey;

struct DestroyPixmapClosure {
    DestroyPixmapClosure(Drawable d, Screen *s) : mPixmap(d), mScreen(s) {}
    Drawable mPixmap;
    Screen *mScreen;
};

static void
DestroyPixmap(void *data)
{
    DestroyPixmapClosure *closure = static_cast<DestroyPixmapClosure*>(data);
    XFreePixmap(DisplayOfScreen(closure->mScreen), closure->mPixmap);
    delete closure;
}

/* static */
cairo_surface_t *
gfxXlibSurface::CreateCairoSurface(Screen *screen, Visual *visual,
                                   const gfx::IntSize& size, Drawable relatedDrawable)
{
    Drawable drawable =
        CreatePixmap(screen, size, DepthOfVisual(screen, visual),
                     relatedDrawable);
    if (!drawable)
        return nullptr;

    cairo_surface_t* surface =
        cairo_xlib_surface_create(DisplayOfScreen(screen), drawable, visual,
                                  size.width, size.height);
    if (cairo_surface_status(surface)) {
        cairo_surface_destroy(surface);
        XFreePixmap(DisplayOfScreen(screen), drawable);
        return nullptr;
    }

    DestroyPixmapClosure *closure = new DestroyPixmapClosure(drawable, screen);
    cairo_surface_set_user_data(surface, &gDestroyPixmapKey,
                                closure, DestroyPixmap);
    return surface;
}

/* static */
already_AddRefed<gfxXlibSurface>
gfxXlibSurface::Create(Screen *screen, Visual *visual,
                       const gfx::IntSize& size, Drawable relatedDrawable)
{
    Drawable drawable =
        CreatePixmap(screen, size, DepthOfVisual(screen, visual),
                     relatedDrawable);
    if (!drawable)
        return nullptr;

    RefPtr<gfxXlibSurface> result =
        new gfxXlibSurface(DisplayOfScreen(screen), drawable, visual, size);
    result->TakePixmap();

    if (result->CairoStatus() != 0)
        return nullptr;

    return result.forget();
}

/* static */
already_AddRefed<gfxXlibSurface>
gfxXlibSurface::Create(Screen *screen, XRenderPictFormat *format,
                       const gfx::IntSize& size, Drawable relatedDrawable)
{
    Drawable drawable =
        CreatePixmap(screen, size, format->depth, relatedDrawable);
    if (!drawable)
        return nullptr;

    RefPtr<gfxXlibSurface> result =
        new gfxXlibSurface(screen, drawable, format, size);
    result->TakePixmap();

    if (result->CairoStatus() != 0)
        return nullptr;

    return result.forget();
}

static bool GetForce24bppPref()
{
    return Preferences::GetBool("mozilla.widget.force-24bpp", false);
}

already_AddRefed<gfxASurface>
gfxXlibSurface::CreateSimilarSurface(gfxContentType aContent,
                                     const gfx::IntSize& aSize)
{
    if (!mSurface || !mSurfaceValid) {
      return nullptr;
    }

    if (aContent == gfxContentType::COLOR) {
        // cairo_surface_create_similar will use a matching visual if it can.
        // However, systems with 16-bit or indexed default visuals may benefit
        // from rendering with 24-bit formats.
        static bool force24bpp = GetForce24bppPref();
        if (force24bpp
            && cairo_xlib_surface_get_depth(CairoSurface()) != 24) {
            XRenderPictFormat* format =
                XRenderFindStandardFormat(mDisplay, PictStandardRGB24);
            if (format) {
                // Cairo only performs simple self-copies as desired if it
                // knows that this is a Pixmap surface.  It only knows that
                // surfaces are pixmap surfaces if it creates the Pixmap
                // itself, so we use cairo_surface_create_similar with a
                // temporary reference surface to indicate the format.
                Screen* screen = cairo_xlib_surface_get_screen(CairoSurface());
                RefPtr<gfxXlibSurface> depth24reference =
                    gfxXlibSurface::Create(screen, format,
                                           gfx::IntSize(1, 1), mDrawable);
                if (depth24reference)
                    return depth24reference->
                        gfxASurface::CreateSimilarSurface(aContent, aSize);
            }
        }
    }

    return gfxASurface::CreateSimilarSurface(aContent, aSize);
}

void
gfxXlibSurface::Finish()
{
#if defined(GL_PROVIDER_GLX)
    if (mPixmapTaken && mGLXPixmap) {
        gl::sGLXLibrary.DestroyPixmap(mDisplay, mGLXPixmap);
        mGLXPixmap = None;
    }
#endif
    gfxASurface::Finish();
}

const gfx::IntSize
gfxXlibSurface::GetSize() const
{
    if (!mSurfaceValid)
        return gfx::IntSize(0,0);

    return gfx::IntSize(cairo_xlib_surface_get_width(mSurface),
                      cairo_xlib_surface_get_height(mSurface));
}

const gfx::IntSize
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

    return gfx::IntSize(width, height);
}

class DisplayTable {
public:
    static bool GetColormapAndVisual(Screen* screen,
                                       XRenderPictFormat* format,
                                       Visual* visual, Colormap* colormap,
                                       Visual** visualForColormap);

private:
    struct ColormapEntry {
        XRenderPictFormat* mFormat;
        // The Screen is needed here because colormaps (and their visuals) may
        // only be used on one Screen, but XRenderPictFormats are not unique
        // to any one Screen.
        Screen* mScreen;
        Visual* mVisual;
        Colormap mColormap;
    };

    class DisplayInfo {
    public:
        explicit DisplayInfo(Display* display) : mDisplay(display) { }
        Display* mDisplay;
        nsTArray<ColormapEntry> mColormapEntries;
    };

    // Comparator for finding the DisplayInfo
    class FindDisplay {
    public:
        bool Equals(const DisplayInfo& info, const Display *display) const
        {
            return info.mDisplay == display;
        }
    };

    static int DisplayClosing(Display *display, XExtCodes* codes);

    nsTArray<DisplayInfo> mDisplays;
    static DisplayTable* sDisplayTable;
};

DisplayTable* DisplayTable::sDisplayTable;

// Pixmaps don't have a particular associated visual but the pixel values are
// interpreted according to a visual/colormap pairs.
//
// cairo is designed for surfaces with either TrueColor visuals or the
// default visual (which may not be true color).  TrueColor visuals don't
// really need a colormap because the visual indicates the pixel format,
// and cairo uses the default visual with the default colormap, so cairo
// surfaces don't need an explicit colormap.
//
// However, some toolkits (e.g. GDK) need a colormap even with TrueColor
// visuals.  We can create a colormap for these visuals, but it will use about
// 20kB of memory in the server, so we use the default colormap when
// suitable and share colormaps between surfaces.  Another reason for
// minimizing colormap turnover is that the plugin process must leak resources
// for each new colormap id when using older GDK libraries (bug 569775).
//
// Only the format of the pixels is important for rendering to Pixmaps, so if
// the format of a visual matches that of the surface, then that visual can be
// used for rendering to the surface.  Multiple visuals can match the same
// format (but have different GLX properties), so the visual returned may
// differ from the visual passed in.  Colormaps are tied to a visual, so
// should only be used with their visual.

/* static */ bool
DisplayTable::GetColormapAndVisual(Screen* aScreen, XRenderPictFormat* aFormat,
                                   Visual* aVisual, Colormap* aColormap,
                                   Visual** aVisualForColormap)

{
    Display* display = DisplayOfScreen(aScreen);

    // Use the default colormap if the default visual matches.
    Visual *defaultVisual = DefaultVisualOfScreen(aScreen);
    if (aVisual == defaultVisual
        || (aFormat
            && aFormat == XRenderFindVisualFormat(display, defaultVisual)))
    {
        *aColormap = DefaultColormapOfScreen(aScreen);
        *aVisualForColormap = defaultVisual;
        return true;
    }

    // Only supporting TrueColor non-default visuals
    if (!aVisual || aVisual->c_class != TrueColor)
        return false;

    if (!sDisplayTable) {
        sDisplayTable = new DisplayTable();
    }

    nsTArray<DisplayInfo>* displays = &sDisplayTable->mDisplays;
    size_t d = displays->IndexOf(display, 0, FindDisplay());

    if (d == displays->NoIndex) {
        d = displays->Length();
        // Register for notification of display closing, when this info
        // becomes invalid.
        XExtCodes *codes = XAddExtension(display);
        if (!codes)
            return false;

        XESetCloseDisplay(display, codes->extension, DisplayClosing);
        // Add a new DisplayInfo.
        displays->AppendElement(display);
    }

    nsTArray<ColormapEntry>* entries =
        &displays->ElementAt(d).mColormapEntries;

    // Only a small number of formats are expected to be used, so just do a
    // simple linear search.
    for (uint32_t i = 0; i < entries->Length(); ++i) {
        const ColormapEntry& entry = entries->ElementAt(i);
        // Only the format and screen need to match.  (The visual may differ.)
        // If there is no format (e.g. no RENDER extension) then just compare
        // the visual.
        if ((aFormat && entry.mFormat == aFormat && entry.mScreen == aScreen)
            || aVisual == entry.mVisual) {
            *aColormap = entry.mColormap;
            *aVisualForColormap = entry.mVisual;
            return true;
        }
    }

    // No existing entry.  Create a colormap and add an entry.
    Colormap colormap = XCreateColormap(display, RootWindowOfScreen(aScreen),
                                        aVisual, AllocNone);
    ColormapEntry* newEntry = entries->AppendElement();
    newEntry->mFormat = aFormat;
    newEntry->mScreen = aScreen;
    newEntry->mVisual = aVisual;
    newEntry->mColormap = colormap;

    *aColormap = colormap;
    *aVisualForColormap = aVisual;
    return true;
}

/* static */ int
DisplayTable::DisplayClosing(Display *display, XExtCodes* codes)
{
    // No need to free the colormaps explicitly as they will be released when
    // the connection is closed.
    sDisplayTable->mDisplays.RemoveElement(display, FindDisplay());
    if (sDisplayTable->mDisplays.Length() == 0) {
        delete sDisplayTable;
        sDisplayTable = nullptr;
    }
    return 0;
}

/* static */
bool
gfxXlibSurface::GetColormapAndVisual(cairo_surface_t* aXlibSurface,
                                     Colormap* aColormap, Visual** aVisual)
{
    XRenderPictFormat* format =
        cairo_xlib_surface_get_xrender_format(aXlibSurface);
    Screen* screen = cairo_xlib_surface_get_screen(aXlibSurface);
    Visual* visual = cairo_xlib_surface_get_visual(aXlibSurface);

    return DisplayTable::GetColormapAndVisual(screen, format, visual,
                                              aColormap, aVisual);
}

bool
gfxXlibSurface::GetColormapAndVisual(Colormap* aColormap, Visual** aVisual)
{
    if (!mSurfaceValid)
        return false;

    return GetColormapAndVisual(CairoSurface(), aColormap, aVisual);
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
Visual*
gfxXlibSurface::FindVisual(Screen *screen, gfxImageFormat format)
{
    int depth;
    unsigned long red_mask, green_mask, blue_mask;
    switch (format) {
        case gfx::SurfaceFormat::A8R8G8B8_UINT32:
            depth = 32;
            red_mask = 0xff0000;
            green_mask = 0xff00;
            blue_mask = 0xff;
            break;
        case gfx::SurfaceFormat::X8R8G8B8_UINT32:
            depth = 24;
            red_mask = 0xff0000;
            green_mask = 0xff00;
            blue_mask = 0xff;
            break;
        case gfx::SurfaceFormat::R5G6B5_UINT16:
            depth = 16;
            red_mask = 0xf800;
            green_mask = 0x7e0;
            blue_mask = 0x1f;
            break;
        case gfx::SurfaceFormat::A8:
        default:
            return nullptr;
    }

    for (int d = 0; d < screen->ndepths; d++) {
        const Depth& d_info = screen->depths[d];
        if (d_info.depth != depth)
            continue;

        for (int v = 0; v < d_info.nvisuals; v++) {
            Visual* visual = &d_info.visuals[v];

            if (visual->c_class == TrueColor &&
                visual->red_mask == red_mask &&
                visual->green_mask == green_mask &&
                visual->blue_mask == blue_mask)
                return visual;
        }
    }

    return nullptr;
}

/* static */
XRenderPictFormat*
gfxXlibSurface::FindRenderFormat(Display *dpy, gfxImageFormat format)
{
    switch (format) {
        case gfx::SurfaceFormat::A8R8G8B8_UINT32:
            return XRenderFindStandardFormat (dpy, PictStandardARGB32);
        case gfx::SurfaceFormat::X8R8G8B8_UINT32:
            return XRenderFindStandardFormat (dpy, PictStandardRGB24);
        case gfx::SurfaceFormat::R5G6B5_UINT16: {
            // PictStandardRGB16_565 is not standard Xrender format
            // we should try to find related visual
            // and find xrender format by visual
            Visual *visual = FindVisual(DefaultScreenOfDisplay(dpy), format);
            if (!visual)
                return nullptr;
            return XRenderFindVisualFormat(dpy, visual);
        }
        case gfx::SurfaceFormat::A8:
            return XRenderFindStandardFormat (dpy, PictStandardA8);
        default:
            break;
    }

    return nullptr;
}

Screen*
gfxXlibSurface::XScreen()
{
    return cairo_xlib_surface_get_screen(CairoSurface());
}

XRenderPictFormat*
gfxXlibSurface::XRenderFormat()
{
    return cairo_xlib_surface_get_xrender_format(CairoSurface());
}

#if defined(GL_PROVIDER_GLX)
GLXPixmap
gfxXlibSurface::GetGLXPixmap()
{
    if (!mGLXPixmap) {
#ifdef DEBUG
        // cairo_surface_has_show_text_glyphs is used solely for the
        // side-effect of setting the error on surface if
        // cairo_surface_finish() has been called.
        cairo_surface_has_show_text_glyphs(CairoSurface());
        NS_ASSERTION(CairoStatus() != CAIRO_STATUS_SURFACE_FINISHED,
            "GetGLXPixmap called after surface finished");
#endif
        mGLXPixmap = gl::sGLXLibrary.CreatePixmap(this);
    }
    return mGLXPixmap;
}

void
gfxXlibSurface::BindGLXPixmap(GLXPixmap aPixmap)
{
    MOZ_ASSERT(!mGLXPixmap, "A GLXPixmap is already bound!");
    mGLXPixmap = aPixmap;
}

#endif
