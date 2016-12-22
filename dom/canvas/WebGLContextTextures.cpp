/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLBuffer.h"
#include "WebGLVertexAttribData.h"
#include "WebGLShader.h"
#include "WebGLProgram.h"
#include "WebGLUniformLocation.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShaderPrecisionFormat.h"
#include "WebGLTexture.h"
#include "WebGLExtensions.h"
#include "WebGLVertexArray.h"

#include "nsString.h"
#include "nsDebug.h"
#include "nsReadableUtils.h"

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "GLContext.h"

#include "nsContentUtils.h"
#include "nsError.h"
#include "nsLayoutUtils.h"

#include "CanvasUtils.h"
#include "gfxUtils.h"

#include "jsfriendapi.h"

#include "WebGLTexelConversions.h"
#include "WebGLValidateStrings.h"
#include <algorithm>

// needed to check if current OS is lower than 10.7
#if defined(MOZ_WIDGET_COCOA)
#include "nsCocoaFeatures.h"
#endif

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/EndianUtils.h"

namespace mozilla {

static bool
IsValidTexTarget(WebGLContext* webgl, uint8_t funcDims, GLenum rawTexTarget,
                 TexTarget* const out)
{
    uint8_t targetDims;

    switch (rawTexTarget) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP:
        targetDims = 2;
        break;

    case LOCAL_GL_TEXTURE_3D:
    case LOCAL_GL_TEXTURE_2D_ARRAY:
        if (!webgl->IsWebGL2())
            return false;

        targetDims = 3;
        break;

    default:
        return false;
    }

    // Some funcs (like GenerateMipmap) doesn't know the dimension, so don't check it.
    if (funcDims && targetDims != funcDims)
        return false;

    *out = rawTexTarget;
    return true;
}

static bool
IsValidTexImageTarget(WebGLContext* webgl, uint8_t funcDims, GLenum rawTexImageTarget,
                      TexImageTarget* const out)
{
    uint8_t targetDims;

    switch (rawTexImageTarget) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        targetDims = 2;
        break;

    case LOCAL_GL_TEXTURE_3D:
    case LOCAL_GL_TEXTURE_2D_ARRAY:
        if (!webgl->IsWebGL2())
            return false;

        targetDims = 3;
        break;

    default:
        return false;
    }

    if (targetDims != funcDims)
        return false;

    *out = rawTexImageTarget;
    return true;
}

bool
ValidateTexTarget(WebGLContext* webgl, const char* funcName, uint8_t funcDims,
                  GLenum rawTexTarget, TexTarget* const out_texTarget,
                  WebGLTexture** const out_tex)
{
    if (webgl->IsContextLost())
        return false;

    TexTarget texTarget;
    if (!IsValidTexTarget(webgl, funcDims, rawTexTarget, &texTarget)) {
        webgl->ErrorInvalidEnum("%s: Invalid texTarget.", funcName);
        return false;
    }

    WebGLTexture* tex = webgl->ActiveBoundTextureForTarget(texTarget);
    if (!tex) {
        webgl->ErrorInvalidOperation("%s: No texture is bound to this target.", funcName);
        return false;
    }

    *out_texTarget = texTarget;
    *out_tex = tex;
    return true;
}

bool
ValidateTexImageTarget(WebGLContext* webgl, const char* funcName, uint8_t funcDims,
                       GLenum rawTexImageTarget, TexImageTarget* const out_texImageTarget,
                       WebGLTexture** const out_tex)
{
    if (webgl->IsContextLost())
        return false;

    TexImageTarget texImageTarget;
    if (!IsValidTexImageTarget(webgl, funcDims, rawTexImageTarget, &texImageTarget)) {
        webgl->ErrorInvalidEnum("%s: Invalid texImageTarget.", funcName);
        return false;
    }

    WebGLTexture* tex = webgl->ActiveBoundTextureForTexImageTarget(texImageTarget);
    if (!tex) {
        webgl->ErrorInvalidOperation("%s: No texture is bound to this target.", funcName);
        return false;
    }

    *out_texImageTarget = texImageTarget;
    *out_tex = tex;
    return true;
}

/*virtual*/ bool
WebGLContext::IsTexParamValid(GLenum pname) const
{
    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
    case LOCAL_GL_TEXTURE_MAG_FILTER:
    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
        return true;

    case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
        return IsExtensionEnabled(WebGLExtensionID::EXT_texture_filter_anisotropic);

    default:
        return false;
    }
}

void
WebGLContext::InvalidateResolveCacheForTextureWithTexUnit(const GLuint texUnit)
{
    if (mBound2DTextures[texUnit])
        mBound2DTextures[texUnit]->InvalidateResolveCache();
    if (mBoundCubeMapTextures[texUnit])
        mBoundCubeMapTextures[texUnit]->InvalidateResolveCache();
    if (mBound3DTextures[texUnit])
        mBound3DTextures[texUnit]->InvalidateResolveCache();
    if (mBound2DArrayTextures[texUnit])
        mBound2DArrayTextures[texUnit]->InvalidateResolveCache();
}

//////////////////////////////////////////////////////////////////////////////////////////
// GL calls

void
WebGLContext::BindTexture(GLenum rawTarget, WebGLTexture* newTex)
{
    if (IsContextLost())
        return;

     if (newTex && !ValidateObject("bindTexture", *newTex))
        return;

    // Need to check rawTarget first before comparing against newTex->Target() as
    // newTex->Target() returns a TexTarget, which will assert on invalid value.
    WebGLRefPtr<WebGLTexture>* currentTexPtr = nullptr;
    switch (rawTarget) {
    case LOCAL_GL_TEXTURE_2D:
        currentTexPtr = &mBound2DTextures[mActiveTexture];
        break;

   case LOCAL_GL_TEXTURE_CUBE_MAP:
        currentTexPtr = &mBoundCubeMapTextures[mActiveTexture];
        break;

   case LOCAL_GL_TEXTURE_3D:
        if (IsWebGL2())
            currentTexPtr = &mBound3DTextures[mActiveTexture];
        break;

   case LOCAL_GL_TEXTURE_2D_ARRAY:
        if (IsWebGL2())
            currentTexPtr = &mBound2DArrayTextures[mActiveTexture];
        break;
    }

    if (!currentTexPtr) {
        ErrorInvalidEnumInfo("bindTexture: target", rawTarget);
        return;
    }

    const TexTarget texTarget(rawTarget);

    MakeContextCurrent();

    if (newTex) {
        if (!newTex->BindTexture(texTarget))
            return;
    } else {
        gl->fBindTexture(texTarget.get(), 0);
    }

    *currentTexPtr = newTex;
}

void
WebGLContext::GenerateMipmap(GLenum rawTexTarget)
{
    const char funcName[] = "generateMipmap";
    const uint8_t funcDims = 0;

    TexTarget texTarget;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, funcName, funcDims, rawTexTarget, &texTarget, &tex))
        return;

    tex->GenerateMipmap(texTarget);
}

JS::Value
WebGLContext::GetTexParameter(GLenum rawTexTarget, GLenum pname)
{
    const char funcName[] = "getTexParameter";
    const uint8_t funcDims = 0;

    TexTarget texTarget;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, funcName, funcDims, rawTexTarget, &texTarget, &tex))
        return JS::NullValue();

    if (!IsTexParamValid(pname)) {
        ErrorInvalidEnumInfo("getTexParameter: pname", pname);
        return JS::NullValue();
    }

    return tex->GetTexParameter(texTarget, pname);
}

bool
WebGLContext::IsTexture(WebGLTexture* tex)
{
    if (!ValidateIsObject("isTexture", tex))
        return false;

    return tex->IsTexture();
}

void
WebGLContext::TexParameter_base(GLenum rawTexTarget, GLenum pname,
                                const FloatOrInt& param)
{
    const char funcName[] = "texParameter";
    const uint8_t funcDims = 0;

    TexTarget texTarget;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, funcName, funcDims, rawTexTarget, &texTarget, &tex))
        return;

    tex->TexParameter(texTarget, pname, param);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Uploads

void
WebGLContext::CompressedTexImage(const char* funcName, uint8_t funcDims, GLenum rawTarget,
                                 GLint level, GLenum internalFormat, GLsizei width,
                                 GLsizei height, GLsizei depth, GLint border,
                                 const TexImageSource& src)
{
    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTarget, &target, &tex))
        return;

    tex->CompressedTexImage(funcName, target, level, internalFormat, width, height, depth,
                            border, src);
}

void
WebGLContext::CompressedTexSubImage(const char* funcName, uint8_t funcDims,
                                    GLenum rawTarget, GLint level, GLint xOffset,
                                    GLint yOffset, GLint zOffset, GLsizei width,
                                    GLsizei height, GLsizei depth, GLenum unpackFormat,
                                    const TexImageSource& src)
{
    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTarget, &target, &tex))
        return;

    tex->CompressedTexSubImage(funcName, target, level, xOffset, yOffset, zOffset, width,
                               height, depth, unpackFormat, src);
}

////

void
WebGLContext::CopyTexImage2D(GLenum rawTarget, GLint level, GLenum internalFormat,
                             GLint x, GLint y, GLsizei width, GLsizei height,
                             GLint border)
{
    const char funcName[] = "copyTexImage2D";
    const uint8_t funcDims = 2;

    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTarget, &target, &tex))
        return;

    tex->CopyTexImage2D(target, level, internalFormat, x, y, width, height, border);
}

void
WebGLContext::CopyTexSubImage(const char* funcName, uint8_t funcDims, GLenum rawTarget,
                              GLint level, GLint xOffset, GLint yOffset, GLint zOffset,
                              GLint x, GLint y, GLsizei width, GLsizei height)
{
    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTarget, &target, &tex))
        return;

    tex->CopyTexSubImage(funcName, target, level, xOffset, yOffset, zOffset, x, y, width,
                         height);
}

////

void
WebGLContext::TexImage(const char* funcName, uint8_t funcDims, GLenum rawTarget,
                       GLint level, GLenum internalFormat, GLsizei width, GLsizei height,
                       GLsizei depth, GLint border, GLenum unpackFormat,
                       GLenum unpackType, const TexImageSource& src)
{
    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTarget, &target, &tex))
        return;

    const webgl::PackingInfo pi = {unpackFormat, unpackType};
    tex->TexImage(funcName, target, level, internalFormat, width, height, depth, border,
                  pi, src);
}

void
WebGLContext::TexSubImage(const char* funcName, uint8_t funcDims, GLenum rawTarget,
                          GLint level, GLint xOffset, GLint yOffset, GLint zOffset,
                          GLsizei width, GLsizei height, GLsizei depth,
                          GLenum unpackFormat, GLenum unpackType,
                          const TexImageSource& src)
{
    TexImageTarget target;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, funcName, funcDims, rawTarget, &target, &tex))
        return;

    const webgl::PackingInfo pi = {unpackFormat, unpackType};
    tex->TexSubImage(funcName, target, level, xOffset, yOffset, zOffset, width, height,
                     depth, pi, src);
}

} // namespace mozilla
