/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SURFACE_TYPES_H_
#define SURFACE_TYPES_H_

#include "mozilla/TypedEnum.h"
#include "mozilla/StandardInteger.h"

#include <cstring>

namespace mozilla {
namespace layers {
class ISurfaceAllocator;
}

namespace gfx {

typedef uintptr_t SurfaceStreamHandle;

struct SurfaceCaps
{
    bool any;
    bool color, alpha;
    bool bpp16;
    bool depth, stencil;
    bool antialias;
    bool preserve;

    // The surface allocator that we want to create this
    // for.  May be null.
    layers::ISurfaceAllocator* surfaceAllocator;

    SurfaceCaps() {
        Clear();
    }

    void Clear() {
        std::memset(this, 0, sizeof(SurfaceCaps));
    }

    // We can't use just 'RGB' here, since it's an ancient Windows macro.
    static SurfaceCaps ForRGB() {
        SurfaceCaps caps;

        caps.color = true;

        return caps;
    }

    static SurfaceCaps ForRGBA() {
        SurfaceCaps caps;

        caps.color = true;
        caps.alpha = true;

        return caps;
    }

    static SurfaceCaps Any() {
        SurfaceCaps caps;

        caps.any = true;

        return caps;
    }
};

MOZ_BEGIN_ENUM_CLASS(SharedSurfaceType, uint8_t)
    Unknown = 0,

    Basic,
    GLTextureShare,
    EGLImageShare,
    EGLSurfaceANGLE,
    DXGLInterop,
    DXGLInterop2,
    Gralloc,

    Max
MOZ_END_ENUM_CLASS(SharedSurfaceType)


MOZ_BEGIN_ENUM_CLASS(SurfaceStreamType, uint8_t)
    SingleBuffer,
    TripleBuffer_Copy,
    TripleBuffer_Async,
    TripleBuffer,
    Max
MOZ_END_ENUM_CLASS(SurfaceStreamType)


MOZ_BEGIN_ENUM_CLASS(APITypeT, uint8_t)
    Generic = 0,

    OpenGL,

    Max
MOZ_END_ENUM_CLASS(APITypeT)


MOZ_BEGIN_ENUM_CLASS(AttachmentType, uint8_t)
    Screen = 0,

    GLTexture,
    GLRenderbuffer,

    Max
MOZ_END_ENUM_CLASS(AttachmentType)

} /* namespace gfx */
} /* namespace mozilla */

#endif /* SURFACE_TYPES_H_ */
