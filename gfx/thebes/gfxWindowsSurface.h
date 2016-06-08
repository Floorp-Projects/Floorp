/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WINDOWSSURFACE_H
#define GFX_WINDOWSSURFACE_H

#include "gfxASurface.h"
#include "gfxImageSurface.h"

/* include windows.h for the HWND and HDC definitions that we need. */
#include <windows.h>

struct IDirect3DSurface9;

/* undefine LoadImage because our code uses that name */
#undef LoadImage

class gfxContext;

class gfxWindowsSurface : public gfxASurface {
public:
    enum {
        FLAG_TAKE_DC = (1 << 0),
        FLAG_FOR_PRINTING = (1 << 1),
        FLAG_IS_TRANSPARENT = (1 << 2)
    };

    gfxWindowsSurface(HDC dc, uint32_t flags = 0);

    // Create from a shared d3d9surface
    gfxWindowsSurface(IDirect3DSurface9 *surface, uint32_t flags = 0);

    // Create a DIB surface
    gfxWindowsSurface(const mozilla::gfx::IntSize& size,
                      gfxImageFormat imageFormat = mozilla::gfx::SurfaceFormat::X8R8G8B8_UINT32);

    gfxWindowsSurface(cairo_surface_t *csurf);

    virtual already_AddRefed<gfxASurface> CreateSimilarSurface(gfxContentType aType,
                                                               const mozilla::gfx::IntSize& aSize);

    void InitWithDC(uint32_t flags);

    virtual ~gfxWindowsSurface();

    HDC GetDC();

    already_AddRefed<gfxImageSurface> GetAsImageSurface();

    nsresult BeginPrinting(const nsAString& aTitle, const nsAString& aPrintToFileName);
    nsresult EndPrinting();
    nsresult AbortPrinting();
    nsresult BeginPage();
    nsresult EndPage();

    const mozilla::gfx::IntSize GetSize() const;

private:
    void MakeInvalid(mozilla::gfx::IntSize& size);

    bool mOwnsDC;
    bool mForPrinting;

    HDC mDC;
    HWND mWnd;
};

#endif /* GFX_WINDOWSSURFACE_H */
