/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurface.h"
#include "SharedSurfaceGL.h"

using namespace mozilla::gl;

namespace mozilla {
namespace gfx {

// |src| must begin and end locked, though it
// can be temporarily unlocked if needed.
void
SharedSurface::Copy(SharedSurface* src, SharedSurface* dest, SurfaceFactory* factory)
{
    MOZ_ASSERT( src->APIType() == APITypeT::OpenGL);
    MOZ_ASSERT(dest->APIType() == APITypeT::OpenGL);

    SharedSurface_GL* srcGL = (SharedSurface_GL*)src;
    SharedSurface_GL* destGL = (SharedSurface_GL*)dest;

    SharedSurface_GL::ProdCopy(srcGL, destGL, (SurfaceFactory_GL*)factory);
}

} /* namespace gfx */
} /* namespace mozilla */
