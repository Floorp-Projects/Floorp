/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"
#include "WebGL2Context.h"
#include "WebGLContextUtils.h"
#include "WebGLTexture.h"

namespace mozilla {

void
WebGL2Context::TexStorage2D(GLenum rawTexTarget, GLsizei levels, GLenum internalFormat,
                            GLsizei width, GLsizei height)
{
    const char funcName[] = "TexStorage2D";
    const uint8_t funcDims = 2;

    TexTarget target;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, funcName, funcDims, rawTexTarget, &target, &tex))
        return;

    const GLsizei depth = 1;
    tex->TexStorage(funcName, target, levels, internalFormat, width, height, depth);
}

void
WebGL2Context::TexStorage3D(GLenum rawTexTarget, GLsizei levels, GLenum internalFormat,
                            GLsizei width, GLsizei height, GLsizei depth)
{
    const char funcName[] = "texStorage3D";
    const uint8_t funcDims = 3;

    TexTarget target;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, funcName, funcDims, rawTexTarget, &target, &tex))
        return;

    tex->TexStorage(funcName, target, levels, internalFormat, width, height, depth);
}

void
WebGL2Context::TexImage3D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                          GLsizei width, GLsizei height, GLsizei depth, GLint border,
                          GLenum unpackFormat, GLenum unpackType,
                          const dom::Nullable<dom::ArrayBufferView>& maybeView)
{
    const char funcName[] = "texImage3D";
    const uint8_t funcDims = 3;

    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget, &target,
                                &tex))
    {
        return;
    }

    const bool isSubImage = false;
    const GLint xOffset = 0;
    const GLint yOffset = 0;
    const GLint zOffset = 0;
    tex->TexOrSubImage(isSubImage, funcName, target, level, internalFormat, xOffset,
                       yOffset, zOffset, width, height, depth, border, unpackFormat,
                       unpackType, maybeView);
}

void
WebGL2Context::TexSubImage3D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                             GLint yOffset, GLint zOffset, GLsizei width, GLsizei height,
                             GLsizei depth, GLenum unpackFormat, GLenum unpackType,
                             const dom::Nullable<dom::ArrayBufferView>& maybeView,
                             ErrorResult& /*out_rv*/)
{
    const char funcName[] = "texSubImage3D";
    const uint8_t funcDims = 3;

    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget, &target,
                                &tex))
    {
        return;
    }

    const bool isSubImage = true;
    const GLenum internalFormat = 0;
    const GLint border = 0;
    tex->TexOrSubImage(isSubImage, funcName, target, level, internalFormat, xOffset,
                       yOffset, zOffset, width, height, depth, border, unpackFormat,
                       unpackType, maybeView);
}

void
WebGL2Context::TexSubImage3D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                             GLint yOffset, GLint zOffset, GLenum unpackFormat,
                             GLenum unpackType, dom::ImageData* imageData,
                             ErrorResult& /*out_rv*/)
{
    const char funcName[] = "texSubImage3D";
    const uint8_t funcDims = 3;

    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget, &target,
                                &tex))
    {
        return;
    }

    const bool isSubImage = true;
    const GLenum internalFormat = 0;
    tex->TexOrSubImage(isSubImage, funcName, target, level, internalFormat, xOffset,
                       yOffset, zOffset, unpackFormat, unpackType, imageData);
}

void
WebGL2Context::TexSubImage3D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                             GLint yOffset, GLint zOffset, GLenum unpackFormat,
                             GLenum unpackType, dom::Element* elem,
                             ErrorResult* const out_rv)
{
    const char funcName[] = "texSubImage3D";
    const uint8_t funcDims = 3;

    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget, &target,
                                &tex))
    {
        return;
    }

    const bool isSubImage = true;
    const GLenum internalFormat = 0;
    tex->TexOrSubImage(isSubImage, funcName, target, level, internalFormat, xOffset,
                       yOffset, zOffset, unpackFormat, unpackType, elem, out_rv);
}

void
WebGL2Context::CompressedTexImage3D(GLenum rawTexImageTarget, GLint level,
                                    GLenum internalFormat, GLsizei width, GLsizei height,
                                    GLsizei depth, GLint border,
                                    const dom::ArrayBufferView& view)
{
    const char funcName[] = "compressedTexImage3D";
    const uint8_t funcDims = 3;

    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget, &target,
                                &tex))
    {
        return;
    }

    tex->CompressedTexImage(funcName, target, level, internalFormat, width, height, depth,
                            border, view);
}

void
WebGL2Context::CompressedTexSubImage3D(GLenum rawTexImageTarget, GLint level,
                                       GLint xOffset, GLint yOffset, GLint zOffset,
                                       GLsizei width, GLsizei height, GLsizei depth,
                                       GLenum sizedUnpackFormat,
                                       const dom::ArrayBufferView& view)
{
    const char funcName[] = "compressedTexSubImage3D";
    const uint8_t funcDims = 3;

    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget, &target,
                                &tex))
    {
        return;
    }

    tex->CompressedTexSubImage(funcName, target, level, xOffset, yOffset, zOffset, width,
                               height, depth, sizedUnpackFormat, view);
}

void
WebGL2Context::CopyTexSubImage3D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                                 GLint yOffset, GLint zOffset, GLint x, GLint y,
                                 GLsizei width, GLsizei height)
{
    const char funcName[] = "copyTexSubImage3D";
    const uint8_t funcDims = 3;

    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget, &target,
                                &tex))
    {
        return;
    }

    tex->CopyTexSubImage(funcName, target, level, xOffset, yOffset, zOffset, x, y, width,
                         height);
}

////////////////////

void
WebGL2Context::TexImage2D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                          GLsizei width, GLsizei height, GLint border,
                          GLenum unpackFormat, GLenum unpackType, WebGLsizeiptr offset,
                          ErrorResult&)
{
    const char funcName[] = "texImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget, &target,
                                &tex))
    {
        return;
    }

    const bool isSubImage = false;
    const GLint xOffset = 0;
    const GLint yOffset = 0;
    const GLint zOffset = 0;
    const GLsizei depth = 1;
    tex->TexOrSubImage(isSubImage, funcName, target, level, internalFormat, xOffset,
                       yOffset, zOffset, width, height, depth, border, unpackFormat,
                       unpackType, offset);
}

void
WebGL2Context::TexSubImage2D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                             GLint yOffset, GLsizei width, GLsizei height,
                             GLenum unpackFormat, GLenum unpackType, WebGLsizeiptr offset,
                             ErrorResult&)
{
    const char funcName[] = "texSubImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget, &target,
                                &tex))
    {
        return;
    }

    const bool isSubImage = true;
    const GLenum internalFormat = 0;
    const GLint zOffset = 0;
    const GLsizei depth = 1;
    const GLint border = 0;
    tex->TexOrSubImage(isSubImage, funcName, target, level, internalFormat, xOffset,
                       yOffset, zOffset, width, height, depth, border, unpackFormat,
                       unpackType, offset);
}

//////////

void
WebGL2Context::TexImage3D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                          GLsizei width, GLsizei height, GLsizei depth, GLint border,
                          GLenum unpackFormat, GLenum unpackType, WebGLsizeiptr offset)
{
    const char funcName[] = "texImage3D";
    const uint8_t funcDims = 3;

    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget, &target,
                                &tex))
    {
        return;
    }

    const bool isSubImage = false;
    const GLint xOffset = 0;
    const GLint yOffset = 0;
    const GLint zOffset = 0;
    tex->TexOrSubImage(isSubImage, funcName, target, level, internalFormat, xOffset,
                       yOffset, zOffset, width, height, depth, border, unpackFormat,
                       unpackType, offset);
}

void
WebGL2Context::TexSubImage3D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                             GLint yOffset, GLint zOffset, GLsizei width, GLsizei height,
                             GLsizei depth, GLenum unpackFormat, GLenum unpackType,
                             WebGLsizeiptr offset, ErrorResult&)
{
    const char funcName[] = "texSubImage3D";
    const uint8_t funcDims = 3;

    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTexImageTarget, &target,
                                &tex))
    {
        return;
    }

    const bool isSubImage = true;
    const GLenum internalFormat = 0;
    const GLint border = 0;
    tex->TexOrSubImage(isSubImage, funcName, target, level, internalFormat, xOffset,
                       yOffset, zOffset, width, height, depth, border, unpackFormat,
                       unpackType, offset);
}

////////////////////

/*virtual*/ bool
WebGL2Context::IsTexParamValid(GLenum pname) const
{
    switch (pname) {
    case LOCAL_GL_TEXTURE_BASE_LEVEL:
    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
    case LOCAL_GL_TEXTURE_COMPARE_MODE:
    case LOCAL_GL_TEXTURE_IMMUTABLE_FORMAT:
    case LOCAL_GL_TEXTURE_IMMUTABLE_LEVELS:
    case LOCAL_GL_TEXTURE_MAX_LEVEL:
    case LOCAL_GL_TEXTURE_WRAP_R:
    case LOCAL_GL_TEXTURE_MAX_LOD:
    case LOCAL_GL_TEXTURE_MIN_LOD:
        return true;

    default:
        return WebGLContext::IsTexParamValid(pname);
    }
}

} // namespace mozilla
