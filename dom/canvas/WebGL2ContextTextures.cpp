/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::dom;

bool
WebGL2Context::ValidateSizedInternalFormat(GLenum internalformat, const char* info)
{
    switch (internalformat) {
        // Sized Internal Formats
        // https://www.khronos.org/opengles/sdk/docs/man3/html/glTexStorage2D.xhtml
    case LOCAL_GL_R8:
    case LOCAL_GL_R8_SNORM:
    case LOCAL_GL_R16F:
    case LOCAL_GL_R32F:
    case LOCAL_GL_R8UI:
    case LOCAL_GL_R8I:
    case LOCAL_GL_R16UI:
    case LOCAL_GL_R16I:
    case LOCAL_GL_R32UI:
    case LOCAL_GL_R32I:
    case LOCAL_GL_RG8:
    case LOCAL_GL_RG8_SNORM:
    case LOCAL_GL_RG16F:
    case LOCAL_GL_RG32F:
    case LOCAL_GL_RG8UI:
    case LOCAL_GL_RG8I:
    case LOCAL_GL_RG16UI:
    case LOCAL_GL_RG16I:
    case LOCAL_GL_RG32UI:
    case LOCAL_GL_RG32I:
    case LOCAL_GL_RGB8:
    case LOCAL_GL_SRGB8:
    case LOCAL_GL_RGB565:
    case LOCAL_GL_RGB8_SNORM:
    case LOCAL_GL_R11F_G11F_B10F:
    case LOCAL_GL_RGB9_E5:
    case LOCAL_GL_RGB16F:
    case LOCAL_GL_RGB32F:
    case LOCAL_GL_RGB8UI:
    case LOCAL_GL_RGB8I:
    case LOCAL_GL_RGB16UI:
    case LOCAL_GL_RGB16I:
    case LOCAL_GL_RGB32UI:
    case LOCAL_GL_RGB32I:
    case LOCAL_GL_RGBA8:
    case LOCAL_GL_SRGB8_ALPHA8:
    case LOCAL_GL_RGBA8_SNORM:
    case LOCAL_GL_RGB5_A1:
    case LOCAL_GL_RGBA4:
    case LOCAL_GL_RGB10_A2:
    case LOCAL_GL_RGBA16F:
    case LOCAL_GL_RGBA32F:
    case LOCAL_GL_RGBA8UI:
    case LOCAL_GL_RGBA8I:
    case LOCAL_GL_RGB10_A2UI:
    case LOCAL_GL_RGBA16UI:
    case LOCAL_GL_RGBA16I:
    case LOCAL_GL_RGBA32I:
    case LOCAL_GL_RGBA32UI:
    case LOCAL_GL_DEPTH_COMPONENT16:
    case LOCAL_GL_DEPTH_COMPONENT24:
    case LOCAL_GL_DEPTH_COMPONENT32F:
    case LOCAL_GL_DEPTH24_STENCIL8:
    case LOCAL_GL_DEPTH32F_STENCIL8:
        return true;
    }

    if (IsCompressedTextureFormat(internalformat)) {
        return true;
    }

    const char* name = EnumName(internalformat);
    if (name && name[0] != '[')
        ErrorInvalidEnum("%s: invalid internal format %s", info, name);
    else
        ErrorInvalidEnum("%s: invalid internal format 0x%04X", info, internalformat);

    return false;
}

/** Validates parameters to texStorage{2D,3D} */
bool
WebGL2Context::ValidateTexStorage(GLenum target, GLsizei levels, GLenum internalformat,
                                      GLsizei width, GLsizei height, GLsizei depth,
                                      const char* info)
{
    // GL_INVALID_OPERATION is generated if the default texture object is curently bound to target.
    WebGLTexture* tex = activeBoundTextureForTarget(target);
    if (!tex) {
        ErrorInvalidOperation("%s: no texture is bound to target %s", info, EnumName(target));
        return false;
    }

    // GL_INVALID_OPERATION is generated if the texture object currently bound to target already has
    // GL_TEXTURE_IMMUTABLE_FORMAT set to GL_TRUE.
    if (tex->IsImmutable()) {
        ErrorInvalidOperation("%s: texture bound to target %s is already immutable", info, EnumName(target));
        return false;
    }

    // GL_INVALID_ENUM is generated if internalformat is not a valid sized internal format.
    if (!ValidateSizedInternalFormat(internalformat, info))
        return false;

    // GL_INVALID_VALUE is generated if width, height or levels are less than 1.
    if (width < 1)  { ErrorInvalidValue("%s: width is < 1", info);  return false; }
    if (height < 1) { ErrorInvalidValue("%s: height is < 1", info); return false; }
    if (depth < 1)  { ErrorInvalidValue("%s: depth is < 1", info);  return false; }
    if (levels < 1) { ErrorInvalidValue("%s: levels is < 1", info); return false; }

    // The following check via FloorLog2 only requires a depth value if target is TEXTURE_3D.
    bool is3D = (target != LOCAL_GL_TEXTURE_3D);
    if (!is3D)
        depth = 1;

    // GL_INVALID_OPERATION is generated if levels is greater than floor(log2(max(width, height, depth)))+1.
    if (FloorLog2(std::max(std::max(width, height), depth))+1 < levels) {
        ErrorInvalidOperation("%s: levels > floor(log2(max(width, height%s)))+1", info, is3D ? ", depth" : "");
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------
// Texture objects

void
WebGL2Context::TexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    if (IsContextLost())
        return;

    // GL_INVALID_ENUM is generated if target is not one of the accepted target enumerants.
    if (target != LOCAL_GL_TEXTURE_2D && target != LOCAL_GL_TEXTURE_CUBE_MAP)
        return ErrorInvalidEnum("texStorage2D: target is not TEXTURE_2D or TEXTURE_CUBE_MAP.");

    if (!ValidateTexStorage(target, levels, internalformat, width, height, 1, "texStorage2D"))
        return;

    WebGLTexture* tex = activeBoundTextureForTarget(target);
    tex->SetImmutable();

    const size_t facesCount = (target == LOCAL_GL_TEXTURE_2D) ? 1 : 6;
    GLsizei w = width;
    GLsizei h = height;
    for (size_t l = 0; l < size_t(levels); l++) {
        for (size_t f = 0; f < facesCount; f++) {
            tex->SetImageInfo(TexImageTargetForTargetAndFace(target, f),
                              l, w, h, 1,
                              internalformat,
                              WebGLImageDataStatus::UninitializedImageData);
        }
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }

    gl->fTexStorage2D(target, levels, internalformat, width, height);
}

void
WebGL2Context::TexStorage3D(GLenum target, GLsizei levels, GLenum internalformat,
                            GLsizei width, GLsizei height, GLsizei depth)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::TexSubImage3D(GLenum target, GLint level,
                             GLint xoffset, GLint yoffset, GLint zoffset,
                             GLsizei width, GLsizei height, GLsizei depth,
                             GLenum format, GLenum type, const Nullable<dom::ArrayBufferView>& pixels,
                             ErrorResult& rv)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::TexSubImage3D(GLenum target, GLint level,
                             GLint xoffset, GLint yoffset, GLint zoffset,
                             GLenum format, GLenum type, dom::ImageData* data,
                             ErrorResult& rv)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::CopyTexSubImage3D(GLenum target, GLint level,
                                 GLint xoffset, GLint yoffset, GLint zoffset,
                                 GLint x, GLint y, GLsizei width, GLsizei height)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::CompressedTexImage3D(GLenum target, GLint level, GLenum internalformat,
                                    GLsizei width, GLsizei height, GLsizei depth,
                                    GLint border, GLsizei imageSize, const dom::ArrayBufferView& data)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::CompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                       GLsizei width, GLsizei height, GLsizei depth,
                                       GLenum format, GLsizei imageSize, const dom::ArrayBufferView& data)
{
    MOZ_CRASH("Not Implemented.");
}
