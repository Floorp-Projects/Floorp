/* vim: set sw=4 sts=4 et cin: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is OS/2 code in Thebes.
 *
 * The Initial Developer of the Original Code is
 * Peter Weilbacher <mozilla@Weilbacher.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "gfxOS2Surface.h"

#include <stdio.h>

/**********************************************************************
 * class gfxOS2Surface
 **********************************************************************/

gfxOS2Surface::gfxOS2Surface(const gfxIntSize& aSize,
                             gfxASurface::gfxImageFormat aImageFormat)
    : mHasWnd(PR_FALSE), mSize(aSize)
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
    : mHasWnd(PR_TRUE), mDC(nsnull), mBitmap(nsnull)
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Surface[%#x]::gfxOS2Surface(HWND=%#x)\n", (unsigned int)this,
           (unsigned int)aWnd);
#endif

    mPS = WinGetPS(aWnd);

    RECTL rectl;
    WinQueryWindowRect(aWnd, &rectl);
    mSize.width = rectl.xRight - rectl.xLeft;
    mSize.height = rectl.yTop - rectl.yBottom;
    if (mSize.width == 0) mSize.width = 1;   // fake a minimal surface area to let
    if (mSize.height == 0) mSize.height = 1; // cairo_os2_surface_create() return something
    cairo_surface_t *surf = cairo_os2_surface_create(mPS, mSize.width, mSize.height);
#ifdef DEBUG_thebes_2
    printf("  type(%#x)=%d (ID=%#x, h/w=%d/%d)\n", (unsigned int)surf,
           cairo_surface_get_type(surf), (unsigned int)mPS, mSize.width, mSize.height);
#endif
    // Normally, OS/2 cairo surfaces have to be forced to redraw completely
    // by calling cairo_surface_mark_dirty(surf), but Mozilla paints them in
    // full, so that is not necessary here.

    // record the window handle in the cairo surface, so that refresh works
    cairo_os2_surface_set_hwnd(surf, aWnd);
    // manual refresh is done from nsWindow::OnPaint
    cairo_os2_surface_set_manual_window_refresh(surf, 1);

    Init(surf);
}

gfxOS2Surface::~gfxOS2Surface()
{
#ifdef DEBUG_thebes_2
    printf("gfxOS2Surface[%#x]::~gfxOS2Surface()\n", (unsigned int)this);
#endif

    // Surfaces connected to a window were created using WinGetPS so we should
    // release it again with WinReleasePS. Memory surfaces on the other
    // hand were created on memory device contexts with the GPI functions, so
    // use those to clean up stuff.
    if (mHasWnd) {
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
    cairo_os2_surface_refresh_window(CairoSurface(), aPS, aRect);
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
