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
#include <X11/Xlibint.h>	/* For XESetCloseDisplay */

#include "nsTArray.h"
#include "nsAlgorithm.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

// Although the dimension parameters in the xCreatePixmapReq wire protocol are
// 16-bit unsigned integers, the server's CreatePixmap returns BadAlloc if
// either dimension cannot be represented by a 16-bit *signed* integer.
#define XLIB_IMAGE_SIDE_SIZE_LIMIT 0x7fff

gfxXlibSurface::gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual)
    : mPixmapTaken(false), mDisplay(dpy), mDrawable(drawable)
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
    , mGLXPixmap(None)
#endif
{
    DoSizeQuery();
    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, drawable, visual, mSize.width, mSize.height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual, const gfxIntSize& size)
    : mPixmapTaken(false), mDisplay(dpy), mDrawable(drawable), mSize(size)
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
    , mGLXPixmap(None)
#endif
{
    NS_ASSERTION(CheckSurfaceSize(size, XLIB_IMAGE_SIDE_SIZE_LIMIT),
                 "Bad size");

    cairo_surface_t *surf = cairo_xlib_surface_create(dpy, drawable, visual, mSize.width, mSize.height);
    Init(surf);
}

gfxXlibSurface::gfxXlibSurface(Screen *screen, Drawable drawable, XRenderPictFormat *format,
                               const gfxIntSize& size)
    : mPixmapTaken(false), mDisplay(DisplayOfScreen(screen)),
      mDrawable(drawable), mSize(size)
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
      , mGLXPixmap(None)
#endif
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
    : mPixmapTaken(false),
      mSize(cairo_xlib_surface_get_width(csurf),
            cairo_xlib_surface_get_height(csurf))
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
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
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
    if (mGLXPixmap) {
        gl::sGLXLibrary.DestroyPixmap(mGLXPixmap);
    }
#endif
    // gfxASurface's destructor calls RecordMemoryFreed().
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
    // X gives us a fatal error if we try to create a pixmap of width
    // or height 0
    return XCreatePixmap(dpy, relatedDrawable,
                         NS_MAX(1, size.width), NS_MAX(1, size.height),
                         depth);
}

void
gfxXlibSurface::TakePixmap()
{
    NS_ASSERTION(!mPixmapTaken, "I already own the Pixmap!");
    mPixmapTaken = true;

    // Divide by 8 because surface_get_depth gives us the number of *bits* per
    // pixel.
    RecordMemoryUsed(mSize.width * mSize.height *
        cairo_xlib_surface_get_depth(CairoSurface()) / 8);
}

Drawable
gfxXlibSurface::ReleasePixmap() {
    NS_ASSERTION(mPixmapTaken, "I don't own the Pixmap!");
    mPixmapTaken = false;
    RecordMemoryFreed();
    return mDrawable;
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

static bool GetForce24bppPref()
{
    return Preferences::GetBool("mozilla.widget.force-24bpp", false);
}

already_AddRefed<gfxASurface>
gfxXlibSurface::CreateSimilarSurface(gfxContentType aContent,
                                     const gfxIntSize& aSize)
{
    if (!mSurface || !mSurfaceValid) {
      return nsnull;
    }

    if (aContent == CONTENT_COLOR) {
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
                nsRefPtr<gfxXlibSurface> depth24reference =
                    gfxXlibSurface::Create(screen, format,
                                           gfxIntSize(1, 1), mDrawable);
                if (depth24reference)
                    return depth24reference->
                        gfxASurface::CreateSimilarSurface(aContent, aSize);
            }
        }
    }

    return gfxASurface::CreateSimilarSurface(aContent, aSize);
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
        DisplayInfo(Display* display) : mDisplay(display) { }
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
    PRUint32 d = displays->IndexOf(display, 0, FindDisplay());

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
    for (PRUint32 i = 0; i < entries->Length(); ++i) {
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
        sDisplayTable = nsnull;
    }
    return 0;
}

bool
gfxXlibSurface::GetColormapAndVisual(Colormap* aColormap, Visual** aVisual)
{
    if (!mSurfaceValid)
        return false;

    XRenderPictFormat* format =
        cairo_xlib_surface_get_xrender_format(CairoSurface());
    Screen* screen = cairo_xlib_surface_get_screen(CairoSurface());
    Visual* visual = cairo_xlib_surface_get_visual(CairoSurface());

    return DisplayTable::GetColormapAndVisual(screen, format, visual,
                                              aColormap, aVisual);
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
        case ImageFormatARGB32:
            depth = 32;
            red_mask = 0xff0000;
            green_mask = 0xff00;
            blue_mask = 0xff;
            break;
        case ImageFormatRGB24:
            depth = 24;
            red_mask = 0xff0000;
            green_mask = 0xff00;
            blue_mask = 0xff;
            break;
        case ImageFormatRGB16_565:
            depth = 16;
            red_mask = 0xf800;
            green_mask = 0x7e0;
            blue_mask = 0x1f;
            break;
        case ImageFormatA8:
        case ImageFormatA1:
        default:
            return NULL;
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

    return NULL;
}

/* static */
XRenderPictFormat*
gfxXlibSurface::FindRenderFormat(Display *dpy, gfxImageFormat format)
{
    switch (format) {
        case ImageFormatARGB32:
            return XRenderFindStandardFormat (dpy, PictStandardARGB32);
        case ImageFormatRGB24:
            return XRenderFindStandardFormat (dpy, PictStandardRGB24);
        case ImageFormatRGB16_565: {
            // PictStandardRGB16_565 is not standard Xrender format
            // we should try to find related visual
            // and find xrender format by visual
            Visual *visual = FindVisual(DefaultScreenOfDisplay(dpy), format);
            if (!visual)
                return NULL;
            return XRenderFindVisualFormat(dpy, visual);
        }
        case ImageFormatA8:
            return XRenderFindStandardFormat (dpy, PictStandardA8);
        case ImageFormatA1:
            return XRenderFindStandardFormat (dpy, PictStandardA1);
        default:
            break;
    }

    return (XRenderPictFormat*)NULL;
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

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
GLXPixmap
gfxXlibSurface::GetGLXPixmap()
{
    if (!mGLXPixmap) {
        mGLXPixmap = gl::sGLXLibrary.CreatePixmap(this);
    }
    return mGLXPixmap;
}
#endif

gfxASurface::MemoryLocation
gfxXlibSurface::GetMemoryLocation() const
{
    return MEMORY_OUT_OF_PROCESS;
}
