/* vim: set sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxOS2Surface.h"

#include <stdio.h>

/**********************************************************************
 * class gfxOS2Surface
 **********************************************************************/

gfxOS2Surface::gfxOS2Surface(const gfxIntSize& aSize,
                             gfxASurface::gfxImageFormat aImageFormat)
    : mWnd(0), mSize(aSize)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Surface[%#x]::gfxOS2Surface(Size=%dx%d, %d)\n", (unsigned int)this,
           aSize.width, aSize.height, aImageFormat);
#endif
    // in this case we don't have a window, so we create a memory presentation
    // space to construct the cairo surface on

    // create a PS, partly taken from nsOffscreenSurface::Init(), i.e. nsDrawingSurfaceOS2.cpp
    DEVOPENSTRUC dop = { 0, 0, 0, 0, 0 };
    SIZEL sizel = { 0, 0 }; // use same page size as device
    mDC = DevOpenDC(0, OD_MEMORY, (PSZ)"*", 5, (PDEVOPENDATA)&dop, NULLHANDLE);
    NS_ASSERTION(mDC != DEV_ERROR, "Could not create memory DC");

    mPS = GpiCreatePS(0, mDC, &sizel, PU_PELS | GPIT_MICRO | GPIA_ASSOC);
    NS_ASSERTION(mPS != GPI_ERROR, "Could not create PS on memory DC!");

    // now create a bitmap of the right size
    BITMAPINFOHEADER2 hdr = { 0 };
    hdr.cbFix = sizeof(BITMAPINFOHEADER2);
    hdr.cx = mSize.width;
    hdr.cy = mSize.height;
    hdr.cPlanes = 1;

    // find bit depth
    LONG lBitCount = 0;
    DevQueryCaps(mDC, CAPS_COLOR_BITCOUNT, 1, &lBitCount);
    hdr.cBitCount = (USHORT)lBitCount;

    mBitmap = GpiCreateBitmap(mPS, &hdr, 0, 0, 0);
    NS_ASSERTION(mBitmap != GPI_ERROR, "Could not create bitmap in memory!");
    // set final stats & select bitmap into PS
    GpiSetBitmap(mPS, mBitmap);

    // now we can finally create the cairo surface on the in-memory PS
    cairo_surface_t *surf = cairo_os2_surface_create(mPS, mSize.width, mSize.height);
#ifdef DEBUG_thebes_2
    printf("  type(%#x)=%d (ID=%#x, h/w=%d/%d)\n", (unsigned int)surf,
           cairo_surface_get_type(surf), (unsigned int)mPS, mSize.width, mSize.height);
#endif
    // Normally, OS/2 cairo surfaces have to be forced to redraw completely
    // by calling cairo_surface_mark_dirty(surf), but Mozilla paints them in
    // full, so that is not necessary here.

    // manual refresh is done from nsWindow::OnPaint
    cairo_os2_surface_set_manual_window_refresh(surf, 1);

    Init(surf);
}

gfxOS2Surface::gfxOS2Surface(HWND aWnd)
    : mWnd(aWnd), mDC(nullptr), mPS(nullptr), mBitmap(nullptr)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Surface[%#x]::gfxOS2Surface(HWND=%#x)\n", (unsigned int)this,
           (unsigned int)aWnd);
#endif

    RECTL rectl;
    WinQueryWindowRect(aWnd, &rectl);
    mSize.width = rectl.xRight - rectl.xLeft;
    mSize.height = rectl.yTop - rectl.yBottom;
    if (mSize.width == 0) mSize.width = 1;   // fake a minimal surface area to let
    if (mSize.height == 0) mSize.height = 1; // cairo_os2_surface_create() return something

    // This variation on cairo_os2_surface_create() avoids creating a
    // persistent HPS that may never be used.  It also enables manual
    // refresh so nsWindow::OnPaint() controls when the screen is updated.
    cairo_surface_t *surf =
        cairo_os2_surface_create_for_window(mWnd, mSize.width, mSize.height);
#ifdef DEBUG_thebes_2
    printf("  type(%#x)=%d (ID=%#x, h/w=%d/%d)\n", (unsigned int)surf,
           cairo_surface_get_type(surf), (unsigned int)mPS, mSize.width, mSize.height);
#endif

    Init(surf);
}

gfxOS2Surface::gfxOS2Surface(HDC aDC, const gfxIntSize& aSize)
    : mWnd(0), mDC(aDC), mBitmap(nullptr), mSize(aSize)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Surface[%#x]::gfxOS2Surface(HDC=%#x, Size=%dx%d)\n", (unsigned int)this,
           (unsigned int)aDC, aSize.width, aSize.height);
#endif
    SIZEL sizel = { 0, 0 }; // use same page size as device
    mPS = GpiCreatePS(0, mDC, &sizel, PU_PELS | GPIT_MICRO | GPIA_ASSOC);
    NS_ASSERTION(mPS != GPI_ERROR, "Could not create PS on print DC!");

    // now create a bitmap of the right size
    BITMAPINFOHEADER2 hdr = { 0 };
    hdr.cbFix = sizeof(BITMAPINFOHEADER2);
    hdr.cx = mSize.width;
    hdr.cy = mSize.height;
    hdr.cPlanes = 1;

    // find bit depth
    LONG lBitCount = 0;
    DevQueryCaps(mDC, CAPS_COLOR_BITCOUNT, 1, &lBitCount);
    hdr.cBitCount = (USHORT)lBitCount;

    mBitmap = GpiCreateBitmap(mPS, &hdr, 0, 0, 0);
    NS_ASSERTION(mBitmap != GPI_ERROR, "Could not create bitmap for printer!");
    // set final stats & select bitmap into PS
    GpiSetBitmap(mPS, mBitmap);

    // now we can finally create the cairo surface on the in-memory PS
    cairo_surface_t *surf = cairo_os2_surface_create(mPS, mSize.width, mSize.height);
#ifdef DEBUG_thebes_2
    printf("  type(%#x)=%d (ID=%#x, h/w=%d/%d)\n", (unsigned int)surf,
           cairo_surface_get_type(surf), (unsigned int)mPS, mSize.width, mSize.height);
#endif
    // Normally, OS/2 cairo surfaces have to be forced to redraw completely
    // by calling cairo_surface_mark_dirty(surf), but Mozilla paints them in
    // full, so that is not necessary here.

    Init(surf);
}

gfxOS2Surface::~gfxOS2Surface()
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Surface[%#x]::~gfxOS2Surface()\n", (unsigned int)this);
#endif

    // Surfaces connected to a window were created using WinGetPS so we should
    // release it again with WinReleasePS. Memory or printer surfaces on the
    // other hand were created on device contexts with the GPI functions, so
    // use those to clean up stuff.
    if (mWnd) {
        if (mPS) {
            WinReleasePS(mPS);
        }
    } else {
        if (mBitmap) {
            GpiSetBitmap(mPS, NULL);
            GpiDeleteBitmap(mBitmap);
        }
        if (mPS) {
            GpiDestroyPS(mPS);
        }
        if (mDC) {
            DevCloseDC(mDC);
        }
    }
}

void gfxOS2Surface::Refresh(RECTL *aRect, HPS aPS)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Surface[%#x]::Refresh(x=%ld,%ld/y=%ld,%ld, HPS=%#x), mPS=%#x\n",
           (unsigned int)this,
           aRect->xLeft, aRect->xRight, aRect->yBottom, aRect->yTop,
           (unsigned int)aPS, (unsigned int)mPS);
#endif
    cairo_os2_surface_refresh_window(CairoSurface(), (aPS ? aPS : mPS), aRect);
}

int gfxOS2Surface::Resize(const gfxIntSize& aSize)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Surface[%#x]::Resize(%dx%d)\n", (unsigned int)this,
           aSize.width, aSize.height);
#endif
    mSize = aSize; // record the new size
    // hardcode mutex timeout to 50ms for now
    return cairo_os2_surface_set_size(CairoSurface(), mSize.width, mSize.height, 50);
}

HPS gfxOS2Surface::GetPS()
{
    // Creating an HPS on-the-fly should never be needed because GetPS()
    // is only called for printing surfaces & mPS should only be null for
    // window surfaces.  It would be a bug if Cairo had an HPS but Thebes
    // didn't, but we'll check anyway to avoid leakage.  As a last resort,
    // if this is a window surface we'll create one & hang on to it.
    if (!mPS) {
        cairo_os2_surface_get_hps(CairoSurface(), &mPS);
        if (!mPS && mWnd) {
            mPS = WinGetPS(mWnd);
            cairo_os2_surface_set_hps(CairoSurface(), mPS);
        }
    }

    return mPS;
}

