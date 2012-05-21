/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_QPAINTERSURFACE_H
#define GFX_QPAINTERSURFACE_H

#include "gfxASurface.h"
#include "gfxImageSurface.h"

#include "cairo-features.h"
#ifdef CAIRO_HAS_QT_SURFACE

class QPainter;
class QImage;

class THEBES_API gfxQPainterSurface : public gfxASurface {
public:
    gfxQPainterSurface(QPainter *painter);
    gfxQPainterSurface(const gfxIntSize& size, gfxImageFormat format);
    gfxQPainterSurface(const gfxIntSize& size, gfxContentType content);

    gfxQPainterSurface(cairo_surface_t *csurf);

    virtual ~gfxQPainterSurface();

    QPainter *GetQPainter() { return mPainter; }

    QImage *GetQImage();
    already_AddRefed<gfxImageSurface> GetAsImageSurface();

protected:
    QPainter *mPainter;
};

#endif

#endif /* GFX_QPAINTERSURFACE_H */
