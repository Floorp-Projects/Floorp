/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_POINT3D_H
#define GFX_POINT3D_H

#include "mozilla/gfx/BasePoint3D.h"
#include "gfxTypes.h"

struct gfxPoint3D : public mozilla::gfx::BasePoint3D<gfxFloat, gfxPoint3D> {
    typedef mozilla::gfx::BasePoint3D<gfxFloat, gfxPoint3D> Super;

    gfxPoint3D() : Super() {}
    gfxPoint3D(gfxFloat aX, gfxFloat aY, gfxFloat aZ) : Super(aX, aY, aZ) {}
};

#endif /* GFX_POINT3D_H */ 
