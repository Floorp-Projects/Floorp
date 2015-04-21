/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_D2DSURFACE_H
#define GFX_D2DSURFACE_H

#include "gfxASurface.h"
#include "nsPoint.h"

#include <windows.h>

struct ID3D10Texture2D;

class gfxD2DSurface : public gfxASurface {
public:

    gfxD2DSurface(HWND wnd,
                  gfxContentType aContent);

    gfxD2DSurface(const gfxIntSize& size,
                  gfxImageFormat imageFormat = gfxImageFormat::RGB24);

    gfxD2DSurface(HANDLE handle, gfxContentType aContent);

    gfxD2DSurface(ID3D10Texture2D *texture, gfxContentType aContent);

    gfxD2DSurface(cairo_surface_t *csurf);

    virtual ~gfxD2DSurface();

    void Present();
    void Scroll(const nsIntPoint &aDelta, const mozilla::gfx::IntRect &aClip);

    virtual const mozilla::gfx::IntSize GetSize() const;

    ID3D10Texture2D *GetTexture();

    HDC GetDC(bool aRetainContents);
    void ReleaseDC(const mozilla::gfx::IntRect *aUpdatedRect);
};

#endif /* GFX_D2DSURFACE_H */
