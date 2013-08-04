/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxD2DSurface.h"
#include "cairo.h"
#include "cairo-win32.h"
#include "gfxWindowsPlatform.h"

gfxD2DSurface::gfxD2DSurface(HWND aWnd, gfxContentType aContent)
{
    Init(cairo_d2d_surface_create_for_hwnd(
        gfxWindowsPlatform::GetPlatform()->GetD2DDevice(),
        aWnd,
        (cairo_content_t)aContent));
}

gfxD2DSurface::gfxD2DSurface(HANDLE handle, gfxContentType aContent)
{
    Init(cairo_d2d_surface_create_for_handle(
        gfxWindowsPlatform::GetPlatform()->GetD2DDevice(),
        handle,
	(cairo_content_t)aContent));
}

gfxD2DSurface::gfxD2DSurface(ID3D10Texture2D *texture, gfxContentType aContent)
{
    Init(cairo_d2d_surface_create_for_texture(
        gfxWindowsPlatform::GetPlatform()->GetD2DDevice(),
        texture,
	(cairo_content_t)aContent));
}

gfxD2DSurface::gfxD2DSurface(cairo_surface_t *csurf)
{
    Init(csurf, true);
}

gfxD2DSurface::gfxD2DSurface(const gfxIntSize& size,
                             gfxImageFormat imageFormat)
{
    Init(cairo_d2d_surface_create(
        gfxWindowsPlatform::GetPlatform()->GetD2DDevice(),
        (cairo_format_t)imageFormat,
        size.width, size.height));
}

gfxD2DSurface::~gfxD2DSurface()
{
}

void
gfxD2DSurface::Present()
{
    cairo_d2d_present_backbuffer(CairoSurface());
}

void
gfxD2DSurface::Scroll(const nsIntPoint &aDelta, const nsIntRect &aClip)
{
    cairo_rectangle_t rect;
    rect.x = aClip.x;
    rect.y = aClip.y;
    rect.width = aClip.width;
    rect.height = aClip.height;
    cairo_d2d_scroll(CairoSurface(), aDelta.x, aDelta.y, &rect);
}

ID3D10Texture2D*
gfxD2DSurface::GetTexture()
{
  return cairo_d2d_surface_get_texture(CairoSurface());
}

HDC
gfxD2DSurface::GetDC(bool aRetainContents)
{
    return cairo_d2d_get_dc(CairoSurface(), aRetainContents);
}

void
gfxD2DSurface::ReleaseDC(const nsIntRect *aUpdatedRect)
{
    if (!aUpdatedRect) {
        return cairo_d2d_release_dc(CairoSurface(), nullptr);
    }

    cairo_rectangle_int_t rect;
    rect.x = aUpdatedRect->x;
    rect.y = aUpdatedRect->y;
    rect.width = aUpdatedRect->width;
    rect.height = aUpdatedRect->height;
    cairo_d2d_release_dc(CairoSurface(), &rect);
}

const gfxIntSize gfxD2DSurface::GetSize() const
{ 
    return gfxIntSize(cairo_d2d_surface_get_width(mSurface),
                      cairo_d2d_surface_get_height(mSurface));
}