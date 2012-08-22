/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxTeeSurface.h"

#include "cairo-tee.h"

gfxTeeSurface::gfxTeeSurface(cairo_surface_t *csurf)
{
    Init(csurf, true);
}

gfxTeeSurface::gfxTeeSurface(gfxASurface **aSurfaces, int32_t aSurfaceCount)
{
    NS_ASSERTION(aSurfaceCount > 0, "Must have a least one surface");
    cairo_surface_t *csurf = cairo_tee_surface_create(aSurfaces[0]->CairoSurface());
    Init(csurf, false);

    for (int32_t i = 1; i < aSurfaceCount; ++i) {
        cairo_tee_surface_add(csurf, aSurfaces[i]->CairoSurface());
    }
}

const gfxIntSize
gfxTeeSurface::GetSize() const
{
    nsRefPtr<gfxASurface> master = Wrap(cairo_tee_surface_index(mSurface, 0));
    return master->GetSize();
}

void
gfxTeeSurface::GetSurfaces(nsTArray<nsRefPtr<gfxASurface> >* aSurfaces)
{
    for (int32_t i = 0; ; ++i) {
        cairo_surface_t *csurf = cairo_tee_surface_index(mSurface, i);
        if (cairo_surface_status(csurf))
            break;
        nsRefPtr<gfxASurface> *elem = aSurfaces->AppendElement();
        if (!elem)
            return;
        *elem = Wrap(csurf);
    }
}
