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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
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
        return cairo_d2d_release_dc(CairoSurface(), NULL);
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