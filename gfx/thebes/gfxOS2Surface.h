/* vim: set sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_OS2_SURFACE_H
#define GFX_OS2_SURFACE_H

#include "gfxASurface.h"

#define INCL_GPIBITMAPS
#include <os2.h>
#include <cairo-os2.h>

class gfxOS2Surface : public gfxASurface {

public:
    // constructor used to create a memory surface of given size
    gfxOS2Surface(const gfxIntSize& aSize,
                  gfxASurface::gfxImageFormat aImageFormat);
    // constructor for surface connected to an onscreen window
    gfxOS2Surface(HWND aWnd);
    // constructor for surface connected to a printing device context
    gfxOS2Surface(HDC aDC, const gfxIntSize& aSize);
    virtual ~gfxOS2Surface();

    // Special functions that only make sense for the OS/2 port of cairo:

    // Update the cairo surface.
    // While gfxOS2Surface keeps track of the presentation handle itself,
    // use the one from WinBeginPaint() here.
    void Refresh(RECTL *aRect, HPS aPS);

    // Reset the cairo surface to the given size.
    int Resize(const gfxIntSize& aSize);

    HPS GetPS();
    virtual const gfxIntSize GetSize() const { return mSize; }

private:
    HWND mWnd; // non-null if created through the HWND constructor
    HDC mDC; // memory device context
    HPS mPS; // presentation space connected to window or memory device
    HBITMAP mBitmap; // bitmap for initialization of memory surface
    gfxIntSize mSize; // current size of the surface
};

#endif /* GFX_OS2_SURFACE_H */
