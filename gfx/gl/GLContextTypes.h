/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXT_TYPES_H_
#define GLCONTEXT_TYPES_H_

#include "GLTypes.h"
#include "mozilla/TypedEnum.h"

namespace mozilla {
namespace gl {

class GLContext;

typedef uintptr_t SharedTextureHandle;

MOZ_BEGIN_ENUM_CLASS(SharedTextureShareType)
    SameProcess = 0,
    CrossProcess
MOZ_END_ENUM_CLASS(SharedTextureShareType)

MOZ_BEGIN_ENUM_CLASS(SharedTextureBufferType)
    TextureID,
    SurfaceTexture,
    IOSurface
MOZ_END_ENUM_CLASS(SharedTextureBufferType)

MOZ_BEGIN_ENUM_CLASS(GLContextType)
    Unknown,
    WGL,
    CGL,
    GLX,
    EGL
MOZ_END_ENUM_CLASS(GLContextType)

struct GLFormats
{
    // Constructs a zeroed object:
    GLFormats();

    GLenum color_texInternalFormat;
    GLenum color_texFormat;
    GLenum color_texType;
    GLenum color_rbFormat;

    GLenum depthStencil;
    GLenum depth;
    GLenum stencil;

    GLsizei samples;
};


struct PixelBufferFormat
{
    // Constructs a zeroed object:
    PixelBufferFormat();

    int red, green, blue;
    int alpha;
    int depth, stencil;
    int samples;

    int ColorBits() const { return red + green + blue; }
};


} /* namespace gl */
} /* namespace mozilla */

#endif /* GLCONTEXT_TYPES_H_ */
