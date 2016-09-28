/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SurfaceTypes.h"

#include "mozilla/layers/TextureForwarder.h"

namespace mozilla {
namespace gl {

SurfaceCaps::SurfaceCaps()
{
    Clear();
}

/* These are defined out of line so that we don't need to include
 * ISurfaceAllocator.h in the header */
SurfaceCaps::SurfaceCaps(const SurfaceCaps& other) = default;
SurfaceCaps&
SurfaceCaps::operator=(const SurfaceCaps& other) = default;

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
    premultAlpha = true;
    preserve = false;
    surfaceAllocator = nullptr;
}

SurfaceCaps::~SurfaceCaps()
{
}

} // namespace gl
} // namespace mozilla
