/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SurfaceTypes.h"

#include "mozilla/layers/ISurfaceAllocator.h"

namespace mozilla {
namespace gl {

SurfaceCaps::SurfaceCaps()
{
    Clear();
}

SurfaceCaps::SurfaceCaps(const SurfaceCaps& other)
{
    *this = other;
}

SurfaceCaps&
SurfaceCaps::operator=(const SurfaceCaps& other)
{
    any = other.any;
    color = other.color;
    alpha = other.alpha;
    bpp16 = other.bpp16;
    depth = other.depth;
    stencil = other.stencil;
    antialias = other.antialias;
    preserve = other.preserve;
    surfaceAllocator = other.surfaceAllocator;

    return *this;
}

void
SurfaceCaps::Clear()
{
    any = false;
    color = false;
    alpha = false;
    bpp16 = false;
    depth = false;
    stencil = false;
    antialias = false;
    preserve = false;
    surfaceAllocator = nullptr;
}

SurfaceCaps::~SurfaceCaps()
{
}

} // namespace gl
} // namespace mozilla
