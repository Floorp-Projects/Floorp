/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TEESURFACE_H
#define GFX_TEESURFACE_H

#include "gfxASurface.h"
#include "nsTArrayForwardDeclare.h"
#include "nsSize.h"

template<class T> class nsRefPtr;

/**
 * Wraps a cairo_tee_surface. The first surface in the surface list is the
 * primary surface, which answers all surface queries (including size).
 * All drawing is performed on all the surfaces.
 *
 * The device transform of a tee surface is applied before drawing to the
 * underlying surfaces --- which also applies the device transforms of the
 * underlying surfaces.
 */
class gfxTeeSurface : public gfxASurface {
public:
    gfxTeeSurface(cairo_surface_t *csurf);
    gfxTeeSurface(gfxASurface **aSurfaces, int32_t aSurfaceCount);

    virtual const gfxIntSize GetSize() const;

    /**
     * Returns the list of underlying surfaces.
     */
    void GetSurfaces(nsTArray<nsRefPtr<gfxASurface> > *aSurfaces);
};

#endif /* GFX_TEESURFACE_H */
