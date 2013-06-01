/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_POINTH3D_H
#define GFX_POINTH3D_H

#include "mozilla/gfx/BasePoint4D.h"
#include "gfxTypes.h"

struct gfxPointH3D : public mozilla::gfx::BasePoint4D<float, gfxPointH3D> {
    typedef mozilla::gfx::BasePoint4D<float, gfxPointH3D> Super;

    gfxPointH3D() : Super() {}
    gfxPointH3D(float aX, float aY, float aZ, float aW) : Super(aX, aY, aZ, aW) {}
};

#endif /* GFX_POINTH3D_H */
