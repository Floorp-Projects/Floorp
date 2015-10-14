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
WebGL2Context::TexStorage2D(GLenum rawTexTarget, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height)
{
    const char funcName[] = "TexStorage2D";
    TexTarget texTarget;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, rawTexTarget, funcName, &texTarget, &tex))
        return;

    tex->TexStorage2D(texTarget, levels, internalFormat, width, height);
}

void
WebGL2Context::TexStorage3D(GLenum rawTexTarget, GLsizei levels, GLenum internalFormat,
                            GLsizei width, GLsizei height, GLsizei depth)
{
    const char funcName[] = "texStorage3D";
    TexTarget texTarget;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, rawTexTarget, funcName, &texTarget, &tex))
        return;

    tex->TexStorage3D(texTarget, levels, internalFormat, width, height, depth);
}

void
WebGL2Context::TexImage3D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                          GLsizei width, GLsizei height, GLsizei depth,
                          GLint border, GLenum unpackFormat, GLenum unpackType,
                          const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                          ErrorResult& out_rv)
{
    const char funcName[] = "texImage3D";
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, funcName, &texImageTarget, &tex))
    {
        return;
    }

    tex->TexImage3D(texImageTarget, level, internalFormat, width, height, depth, border,
                    unpackFormat, unpackType, maybeView, &out_rv);
}

void
WebGL2Context::TexSubImage3D(GLenum rawTexImageTarget, GLint level,
                             GLint xOffset, GLint yOffset, GLint zOffset,
                             GLsizei width, GLsizei height, GLsizei depth,
                             GLenum unpackFormat, GLenum unpackType,
                             const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                             ErrorResult& out_rv)
{
    const char funcName[] = "texSubImage3D";
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, funcName, &texImageTarget, &tex))
    {
        return;
    }

    tex->TexSubImage3D(texImageTarget, level, xOffset, yOffset, zOffset, width, height,
                       depth, unpackFormat, unpackType, maybeView, &out_rv);

}

void
WebGL2Context::TexSubImage3D(GLenum target, GLint level,
                             GLint xOffset, GLint yOffset, GLint zOffset,
                             GLenum unpackFormat, GLenum unpackType, dom::ImageData* imageData,
                             ErrorResult& out_rv)
{
    GenerateWarning("texSubImage3D: Not implemented.");
}

void
WebGL2Context::CopyTexSubImage3D(GLenum target, GLint level,
                                 GLint xOffset, GLint yOffset, GLint zOffset,
                                 GLint x, GLint y, GLsizei width, GLsizei height)
{
    GenerateWarning("copyTexSubImage3D: Not implemented.");
}

void
WebGL2Context::CompressedTexImage3D(GLenum target, GLint level, GLenum internalFormat,
                                    GLsizei width, GLsizei height, GLsizei depth,
                                    GLint border, GLsizei imageSize, const dom::ArrayBufferViewOrSharedArrayBufferView& view)
{
    GenerateWarning("compressedTexImage3D: Not implemented.");
}

void
WebGL2Context::CompressedTexSubImage3D(GLenum target, GLint level, GLint xOffset, GLint yOffset, GLint zOffset,
                                       GLsizei width, GLsizei height, GLsizei depth,
                                       GLenum unpackFormat, GLsizei imageSize, const dom::ArrayBufferViewOrSharedArrayBufferView& view)
{
    GenerateWarning("compressedTexSubImage3D: Not implemented.");
}

bool
WebGL2Context::IsTexParamValid(GLenum pname) const
{
    switch (pname) {
    case LOCAL_GL_TEXTURE_BASE_LEVEL:
    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
    case LOCAL_GL_TEXTURE_COMPARE_MODE:
    case LOCAL_GL_TEXTURE_IMMUTABLE_FORMAT:
    case LOCAL_GL_TEXTURE_IMMUTABLE_LEVELS:
    case LOCAL_GL_TEXTURE_MAX_LEVEL:
    case LOCAL_GL_TEXTURE_SWIZZLE_A:
    case LOCAL_GL_TEXTURE_SWIZZLE_B:
    case LOCAL_GL_TEXTURE_SWIZZLE_G:
    case LOCAL_GL_TEXTURE_SWIZZLE_R:
    case LOCAL_GL_TEXTURE_WRAP_R:
    case LOCAL_GL_TEXTURE_MAX_LOD:
    case LOCAL_GL_TEXTURE_MIN_LOD:
        return true;

    default:
        return WebGLContext::IsTexParamValid(pname);
    }
}

} // namespace mozilla
