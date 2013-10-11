/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdarg.h>

#include "WebGLContext.h"
#include "GLContext.h"

#include "prprf.h"

#include "jsapi.h"
#include "nsIScriptSecurityManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIVariant.h"
#include "nsCxPusher.h"

#include "nsIDOMEvent.h"
#include "nsIDOMDataContainerEvent.h"

#include "mozilla/Preferences.h"

using namespace mozilla;

void
WebGLContext::GenerateWarning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    GenerateWarning(fmt, ap);

    va_end(ap);
}

void
WebGLContext::GenerateWarning(const char *fmt, va_list ap)
{
    if (!ShouldGenerateWarnings())
        return;

    mAlreadyGeneratedWarnings++;

    char buf[1024];
    PR_vsnprintf(buf, 1024, fmt, ap);

    // no need to print to stderr, as JS_ReportWarning takes care of this for us.

    AutoJSContext cx;
    JS_ReportWarning(cx, "WebGL: %s", buf);
    if (!ShouldGenerateWarnings()) {
        JS_ReportWarning(cx,
            "WebGL: No further warnings will be reported for this WebGL context "
            "(already reported %d warnings)", mAlreadyGeneratedWarnings);
    }
}

bool
WebGLContext::ShouldGenerateWarnings() const
{
    if (mMaxWarnings == -1) {
        return true;
    }

    return mAlreadyGeneratedWarnings < mMaxWarnings;
}

CheckedUint32
WebGLContext::GetImageSize(GLsizei height, 
                           GLsizei width, 
                           uint32_t pixelSize,
                           uint32_t packOrUnpackAlignment)
{
    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * pixelSize;

    // alignedRowSize = row size rounded up to next multiple of packAlignment
    CheckedUint32 checked_alignedRowSize = RoundedToNextMultipleOf(checked_plainRowSize, packOrUnpackAlignment);

    // if height is 0, we don't need any memory to store this; without this check, we'll get an overflow
    CheckedUint32 checked_neededByteLength
        = height <= 0 ? 0 : (height-1) * checked_alignedRowSize + checked_plainRowSize;

    return checked_neededByteLength;
}

void
WebGLContext::SynthesizeGLError(GLenum err)
{
    // If there is already a pending error, don't overwrite it;
    // but if there isn't, then we need to check for a gl error
    // that may have occurred before this one and use that code
    // instead.
    
    MakeContextCurrent();

    UpdateWebGLErrorAndClearGLError();

    if (!mWebGLError)
        mWebGLError = err;
}

void
WebGLContext::SynthesizeGLError(GLenum err, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    GenerateWarning(fmt, va);
    va_end(va);

    return SynthesizeGLError(err);
}

void
WebGLContext::ErrorInvalidEnum(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    GenerateWarning(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_ENUM);
}

void
WebGLContext::ErrorInvalidEnumInfo(const char *info, GLenum enumvalue)
{
    return ErrorInvalidEnum("%s: invalid enum value 0x%x", info, enumvalue);
}

void
WebGLContext::ErrorInvalidOperation(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    GenerateWarning(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_OPERATION);
}

void
WebGLContext::ErrorInvalidValue(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    GenerateWarning(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_VALUE);
}

void
WebGLContext::ErrorInvalidFramebufferOperation(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    GenerateWarning(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_FRAMEBUFFER_OPERATION);
}

void
WebGLContext::ErrorOutOfMemory(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    GenerateWarning(fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_OUT_OF_MEMORY);
}

const char *
WebGLContext::ErrorName(GLenum error)
{
    switch(error) {
        case LOCAL_GL_INVALID_ENUM:
            return "INVALID_ENUM";
        case LOCAL_GL_INVALID_OPERATION:
            return "INVALID_OPERATION";
        case LOCAL_GL_INVALID_VALUE:
            return "INVALID_VALUE";
        case LOCAL_GL_OUT_OF_MEMORY:
            return "OUT_OF_MEMORY";
        case LOCAL_GL_INVALID_FRAMEBUFFER_OPERATION:
            return "INVALID_FRAMEBUFFER_OPERATION";
        case LOCAL_GL_NO_ERROR:
            return "NO_ERROR";
        default:
            MOZ_ASSERT(false);
            return "[unknown WebGL error!]";
    }
}

bool
WebGLContext::IsTextureFormatCompressed(GLenum format)
{
    switch(format) {
        case LOCAL_GL_RGB:
        case LOCAL_GL_RGBA:
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_LUMINANCE:
        case LOCAL_GL_LUMINANCE_ALPHA:
        case LOCAL_GL_DEPTH_COMPONENT:
        case LOCAL_GL_DEPTH_STENCIL:
            return false;

        case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case LOCAL_GL_ATC_RGB:
        case LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA:
        case LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA:
        case LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1:
        case LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1:
        case LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1:
        case LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1:
            return true;
    }

    MOZ_ASSERT(false, "Invalid WebGL texture format?");
    return false;
}

void
WebGLContext::UpdateWebGLErrorAndClearGLError(GLenum *currentGLError)
{
    // get and clear GL error in ALL cases
    GLenum error = gl->GetAndClearError();
    if (currentGLError)
        *currentGLError = error;
    // only store in mWebGLError if is hasn't already recorded an error
    if (!mWebGLError)
        mWebGLError = error;
}
