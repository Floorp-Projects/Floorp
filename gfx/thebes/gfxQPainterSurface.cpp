/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>

#include "cairo-features.h"
#ifdef CAIRO_HAS_QT_SURFACE
#include "cairo-qt.h"
#include "gfxQPainterSurface.h"

gfxQPainterSurface::gfxQPainterSurface(QPainter *painter)
{
    cairo_surface_t *csurf = cairo_qt_surface_create (painter);

    mPainter = painter;

    Init (csurf);
}

gfxQPainterSurface::gfxQPainterSurface(const mozilla::gfx::IntSize& size, gfxImageFormat format)
{
    cairo_format_t cformat = GfxFormatToCairoFormat(format);
    cairo_surface_t *csurf =
        cairo_qt_surface_create_with_qimage(cformat, size.width, size.height);
    mPainter = cairo_qt_surface_get_qpainter (csurf);

    Init (csurf);
}

gfxQPainterSurface::gfxQPainterSurface(const mozilla::gfx::IntSize& size, gfxContentType content)
{
    cairo_surface_t *csurf = cairo_qt_surface_create_with_qpixmap ((cairo_content_t) content,
                                                                         size.width,
                                                                         size.height);
    mPainter = cairo_qt_surface_get_qpainter (csurf);

    Init (csurf);
}

gfxQPainterSurface::gfxQPainterSurface(cairo_surface_t *csurf)
{
    mPainter = cairo_qt_surface_get_qpainter (csurf);

    Init(csurf, true);
}

gfxQPainterSurface::~gfxQPainterSurface()
{
}

QImage *
gfxQPainterSurface::GetQImage()
{
    if (!mSurfaceValid)
        return nullptr;

    return cairo_qt_surface_get_qimage(CairoSurface());
}

already_AddRefed<gfxImageSurface>
gfxQPainterSurface::GetAsImageSurface()
{
    if (!mSurfaceValid)
        return nullptr;

    cairo_surface_t *isurf = cairo_qt_surface_get_image(CairoSurface());
    if (!isurf)
        return nullptr;

    assert(cairo_surface_get_type(isurf) == CAIRO_SURFACE_TYPE_IMAGE);

    RefPtr<gfxImageSurface> asurf = new gfxImageSurface(isurf);
    asurf->SetOpaqueRect(GetOpaqueRect());
    return asurf.forget();
}
#endif
