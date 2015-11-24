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
#include "mozilla/Endian.h"

namespace mozilla {

static bool
IsValidTexTarget(WebGLContext* webgl, GLenum rawTexTarget, TexTarget* const out)
{
    switch (rawTexTarget) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP:
        break;

    case LOCAL_GL_TEXTURE_3D:
        if (!webgl->IsWebGL2())
            return false;

        break;

    default:
        return false;
    }

    *out = rawTexTarget;
    return true;
}

static bool
IsValidTexImageTarget(WebGLContext* webgl, GLenum rawTexImageTarget,
                      TexImageTarget* const out)
{
    switch (rawTexImageTarget) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        break;

    case LOCAL_GL_TEXTURE_3D:
        if (!webgl->IsWebGL2())
            return false;

        break;

    default:
        return false;
    }

    *out = rawTexImageTarget;
    return true;
}

bool
ValidateTexTarget(WebGLContext* webgl, GLenum rawTexTarget, const char* funcName,
                  TexTarget* const out_texTarget, WebGLTexture** const out_tex)
{
    if (webgl->IsContextLost())
        return false;

    TexTarget texTarget;
    if (!IsValidTexTarget(webgl, rawTexTarget, &texTarget)) {
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
ValidateTexImageTarget(WebGLContext* webgl, GLenum rawTexImageTarget,
                       const char* funcName, TexImageTarget* const out_texImageTarget,
                       WebGLTexture** const out_tex)
{
    if (webgl->IsContextLost())
        return false;

    TexImageTarget texImageTarget;
    if (!IsValidTexImageTarget(webgl, rawTexImageTarget, &texImageTarget)) {
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

bool
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

//////////////////////////////////////////////////////////////////////////////////////////
// GL calls

void
WebGLContext::BindTexture(GLenum rawTarget, WebGLTexture* newTex)
{
    if (IsContextLost())
        return;

     if (!ValidateObjectAllowDeletedOrNull("bindTexture", newTex))
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
            if (!IsWebGL2())
                return ErrorInvalidEnum("bindTexture: target TEXTURE_3D is only available in WebGL version 2.0 or newer");

            currentTexPtr = &mBound3DTextures[mActiveTexture];
            break;

       default:
            return ErrorInvalidEnumInfo("bindTexture: target", rawTarget);
    }
    const TexTarget texTarget(rawTarget);

    MakeContextCurrent();

    if (newTex && !newTex->BindTexture(texTarget))
        return;

    if (!newTex) {
        gl->fBindTexture(texTarget.get(), 0);
    }

    *currentTexPtr = newTex;
}

void
WebGLContext::GenerateMipmap(GLenum rawTexTarget)
{
    TexTarget texTarget;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, rawTexTarget, "texParameter", &texTarget, &tex))
        return;

    tex->GenerateMipmap(texTarget);
}

JS::Value
WebGLContext::GetTexParameter(GLenum rawTexTarget, GLenum pname)
{
    TexTarget texTarget;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, rawTexTarget, "texParameter", &texTarget, &tex))
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
    if (IsContextLost())
        return false;

    if (!ValidateObjectAllowDeleted("isTexture", tex))
        return false;

    return tex->IsTexture();
}

void
WebGLContext::TexParameter_base(GLenum rawTexTarget, GLenum pname, GLint* maybeIntParam,
                                GLfloat* maybeFloatParam)
{
    MOZ_ASSERT(maybeIntParam || maybeFloatParam);

    TexTarget texTarget;
    WebGLTexture* tex;
    if (!ValidateTexTarget(this, rawTexTarget, "texParameter", &texTarget, &tex))
        return;

    tex->TexParameter(texTarget, pname, maybeIntParam, maybeFloatParam);
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// Uploads


//////////////////////////////////////////////////////////////////////////////////////////
// TexImage

void
WebGLContext::TexImage2D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                         GLenum unpackFormat, GLenum unpackType, dom::Element* elem,
                         ErrorResult* const out_rv)
{
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, "texImage2D", &texImageTarget,
                                &tex))
    {
        return;
    }

    tex->TexImage2D(texImageTarget, level, internalFormat, unpackFormat, unpackType, elem,
                    out_rv);
}

void
WebGLContext::TexImage2D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                         GLsizei width, GLsizei height, GLint border, GLenum unpackFormat,
                         GLenum unpackType,
                         const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                         ErrorResult& out_rv)
{
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, "texImage2D", &texImageTarget,
                                &tex))
    {
        return;
    }

    tex->TexImage2D(texImageTarget, level, internalFormat, width, height, border,
                    unpackFormat, unpackType, maybeView, &out_rv);
}

void
WebGLContext::TexImage2D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                         GLenum unpackFormat, GLenum unpackType,
                         dom::ImageData* imageData, ErrorResult& out_rv)
{
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, "texImage2D", &texImageTarget,
                                &tex))
    {
        return;
    }

    tex->TexImage2D(texImageTarget, level, internalFormat, unpackFormat, unpackType,
                    imageData, &out_rv);
}


//////////////////////////////////////////////////////////////////////////////////////////
// TexSubImage


void
WebGLContext::TexSubImage2D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                            GLint yOffset, GLenum unpackFormat, GLenum unpackType,
                            dom::Element* elem, ErrorResult* const out_rv)
{
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, "texSubImage2D", &texImageTarget,
                                &tex))
    {
        return;
    }

    tex->TexSubImage2D(texImageTarget, level, xOffset, yOffset, unpackFormat, unpackType,
                       elem, out_rv);
}

void
WebGLContext::TexSubImage2D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                            GLint yOffset, GLsizei width, GLsizei height,
                            GLenum unpackFormat, GLenum unpackType,
                            const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                            ErrorResult& out_rv)
{
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, "texSubImage2D", &texImageTarget,
                                &tex))
    {
        return;
    }

    tex->TexSubImage2D(texImageTarget, level, xOffset, yOffset, width, height,
                       unpackFormat, unpackType, maybeView, &out_rv);
}

void
WebGLContext::TexSubImage2D(GLenum rawTexImageTarget, GLint level, GLint xOffset, GLint yOffset,
                            GLenum unpackFormat, GLenum unpackType, dom::ImageData* imageData,
                            ErrorResult& out_rv)
{
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, "texSubImage2D", &texImageTarget,
                                &tex))
    {
        return;
    }

    tex->TexSubImage2D(texImageTarget, level, xOffset, yOffset, unpackFormat, unpackType,
                       imageData, &out_rv);
}


//////////////////////////////////////////////////////////////////////////////////////////
// CopyTex(Sub)Image


void
WebGLContext::CopyTexImage2D(GLenum rawTexImageTarget, GLint level, GLenum internalFormat,
                             GLint x, GLint y, GLsizei width, GLsizei height,
                             GLint border)
{
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, "copyTexImage2D",
                                &texImageTarget, &tex))
    {
        return;
    }

    tex->CopyTexImage2D(texImageTarget, level, internalFormat, x, y, width, height,
                        border);
}

void
WebGLContext::CopyTexSubImage2D(GLenum rawTexImageTarget, GLint level, GLint xOffset,
                                GLint yOffset, GLint x, GLint y, GLsizei width,
                                GLsizei height)
{
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, "copyTexSubImage2D",
                                &texImageTarget, &tex))
    {
        return;
    }

    tex->CopyTexSubImage2D(texImageTarget, level, xOffset, yOffset, x, y, width, height);
}


//////////////////////////////////////////////////////////////////////////////////////////
// CompressedTex(Sub)Image


void
WebGLContext::CompressedTexImage2D(GLenum rawTexImageTarget, GLint level,
                                   GLenum internalFormat, GLsizei width, GLsizei height,
                                   GLint border, const dom::ArrayBufferViewOrSharedArrayBufferView& view)
{
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, "compressedTexImage2D",
                                &texImageTarget, &tex))
    {
        return;
    }

    tex->CompressedTexImage2D(texImageTarget, level, internalFormat, width, height,
                              border, view);
}

void
WebGLContext::CompressedTexSubImage2D(GLenum rawTexImageTarget, GLint level,
                                      GLint xOffset, GLint yOffset, GLsizei width,
                                      GLsizei height, GLenum unpackFormat,
                                      const dom::ArrayBufferViewOrSharedArrayBufferView& view)
{
    TexImageTarget texImageTarget;
    WebGLTexture* tex;
    if (!ValidateTexImageTarget(this, rawTexImageTarget, "compressedTexSubImage2D",
                                &texImageTarget, &tex))
    {
        return;
    }

    tex->CompressedTexSubImage2D(texImageTarget, level, xOffset, yOffset, width, height,
                                 unpackFormat, view);
}

} // namespace mozilla
