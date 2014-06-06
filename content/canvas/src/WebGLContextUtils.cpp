/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include <stdarg.h>

#include "GLContext.h"
#include "jsapi.h"
#include "mozilla/Preferences.h"
#include "nsCxPusher.h"
#include "nsIDOMDataContainerEvent.h"
#include "nsIDOMEvent.h"
#include "nsIScriptSecurityManager.h"
#include "nsIVariant.h"
#include "nsServiceManagerUtils.h"
#include "prprf.h"
#include "WebGLBuffer.h"
#include "WebGLExtensions.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLTexture.h"
#include "WebGLVertexArray.h"

namespace mozilla {

using namespace gl;

bool
IsGLDepthFormat(GLenum webGLFormat)
{
    return (webGLFormat == LOCAL_GL_DEPTH_COMPONENT ||
            webGLFormat == LOCAL_GL_DEPTH_COMPONENT16 ||
            webGLFormat == LOCAL_GL_DEPTH_COMPONENT32);
}

bool
IsGLDepthStencilFormat(GLenum webGLFormat)
{
    return (webGLFormat == LOCAL_GL_DEPTH_STENCIL ||
            webGLFormat == LOCAL_GL_DEPTH24_STENCIL8);
}

bool
FormatHasAlpha(GLenum webGLFormat)
{
    return webGLFormat == LOCAL_GL_RGBA ||
           webGLFormat == LOCAL_GL_LUMINANCE_ALPHA ||
           webGLFormat == LOCAL_GL_ALPHA ||
           webGLFormat == LOCAL_GL_RGBA4 ||
           webGLFormat == LOCAL_GL_RGB5_A1 ||
           webGLFormat == LOCAL_GL_SRGB_ALPHA;
}

/**
 * Convert WebGL/ES format and type into GL format and GL internal
 * format valid for underlying driver.
 */
void
DriverFormatsFromFormatAndType(GLContext* gl, GLenum webGLFormat, GLenum webGLType,
                               GLenum* out_driverInternalFormat, GLenum* out_driverFormat)
{
    MOZ_ASSERT(out_driverInternalFormat, "out_driverInternalFormat can't be nullptr.");
    MOZ_ASSERT(out_driverFormat, "out_driverFormat can't be nullptr.");
    if (!out_driverInternalFormat || !out_driverFormat)
        return;

    // ES2 requires that format == internalformat; floating-point is
    // indicated purely by the type that's loaded.  For desktop GL, we
    // have to specify a floating point internal format.
    if (gl->IsGLES()) {
        *out_driverInternalFormat = webGLFormat;
        *out_driverFormat = webGLFormat;

        return;
    }

    GLenum format = webGLFormat;
    GLenum internalFormat = LOCAL_GL_NONE;

    if (format == LOCAL_GL_DEPTH_COMPONENT) {
        if (webGLType == LOCAL_GL_UNSIGNED_SHORT)
            internalFormat = LOCAL_GL_DEPTH_COMPONENT16;
        else if (webGLType == LOCAL_GL_UNSIGNED_INT)
            internalFormat = LOCAL_GL_DEPTH_COMPONENT32;
    } else if (format == LOCAL_GL_DEPTH_STENCIL) {
        if (webGLType == LOCAL_GL_UNSIGNED_INT_24_8_EXT)
            internalFormat = LOCAL_GL_DEPTH24_STENCIL8;
    } else {
        switch (webGLType) {
        case LOCAL_GL_UNSIGNED_BYTE:
        case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
        case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
        case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
            internalFormat = format;
            break;

        case LOCAL_GL_FLOAT:
            switch (format) {
            case LOCAL_GL_RGBA:
                internalFormat = LOCAL_GL_RGBA32F;
                break;

            case LOCAL_GL_RGB:
                internalFormat = LOCAL_GL_RGB32F;
                break;

            case LOCAL_GL_ALPHA:
                internalFormat = LOCAL_GL_ALPHA32F_ARB;
                break;

            case LOCAL_GL_LUMINANCE:
                internalFormat = LOCAL_GL_LUMINANCE32F_ARB;
                break;

            case LOCAL_GL_LUMINANCE_ALPHA:
                internalFormat = LOCAL_GL_LUMINANCE_ALPHA32F_ARB;
                break;
            }
            break;

        case LOCAL_GL_HALF_FLOAT_OES:
            switch (format) {
            case LOCAL_GL_RGBA:
                internalFormat = LOCAL_GL_RGBA16F;
                break;

            case LOCAL_GL_RGB:
                internalFormat = LOCAL_GL_RGB16F;
                break;

            case LOCAL_GL_ALPHA:
                internalFormat = LOCAL_GL_ALPHA16F_ARB;
                break;

            case LOCAL_GL_LUMINANCE:
                internalFormat = LOCAL_GL_LUMINANCE16F_ARB;
                break;

            case LOCAL_GL_LUMINANCE_ALPHA:
                internalFormat = LOCAL_GL_LUMINANCE_ALPHA16F_ARB;
                break;
            }
            break;

        default:
            break;
        }

        // Handle ES2 and GL differences when supporting sRGB internal formats. GL ES
        // requires that format == internalformat, but GL will fail in this case.
        // GL requires:
        //      format  ->  internalformat
        //      GL_RGB      GL_SRGB_EXT
        //      GL_RGBA     GL_SRGB_ALPHA_EXT
        switch (format) {
        case LOCAL_GL_SRGB:
            internalFormat = format;
            format = LOCAL_GL_RGB;
            break;

        case LOCAL_GL_SRGB_ALPHA:
            internalFormat = format;
            format = LOCAL_GL_RGBA;
            break;
        }
    }

    MOZ_ASSERT(format != LOCAL_GL_NONE && internalFormat != LOCAL_GL_NONE,
               "Coding mistake -- bad format/type passed?");

    *out_driverInternalFormat = internalFormat;
    *out_driverFormat = format;
}

GLenum
DriverTypeFromType(GLContext* gl, GLenum webGLType)
{
    if (gl->IsGLES())
        return webGLType;

    // convert type for half float if not on GLES2
    GLenum type = webGLType;
    if (type == LOCAL_GL_HALF_FLOAT_OES) {
        if (gl->IsSupported(gl::GLFeature::texture_half_float)) {
            return LOCAL_GL_HALF_FLOAT;
        } else {
            MOZ_ASSERT(gl->IsExtensionSupported(gl::GLContext::OES_texture_half_float));
        }
    }

    return webGLType;
}

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
    /* ES2 section 2.5 "GL Errors" states that implementations can have
     * multiple 'flags', as errors might be caught in different parts of
     * a distributed implementation.
     * We're signing up as a distributed implementation here, with
     * separate flags for WebGL and the underlying GLContext.
     */
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

const char*
WebGLContext::EnumName(GLenum glenum)
{
    switch (glenum) {
#define XX(x) case LOCAL_GL_##x: return #x
        XX(ALPHA);
        XX(ATC_RGB);
        XX(ATC_RGBA_EXPLICIT_ALPHA);
        XX(ATC_RGBA_INTERPOLATED_ALPHA);
        XX(COMPRESSED_RGBA_PVRTC_2BPPV1);
        XX(COMPRESSED_RGBA_PVRTC_4BPPV1);
        XX(COMPRESSED_RGBA_S3TC_DXT1_EXT);
        XX(COMPRESSED_RGBA_S3TC_DXT3_EXT);
        XX(COMPRESSED_RGBA_S3TC_DXT5_EXT);
        XX(COMPRESSED_RGB_PVRTC_2BPPV1);
        XX(COMPRESSED_RGB_PVRTC_4BPPV1);
        XX(COMPRESSED_RGB_S3TC_DXT1_EXT);
        XX(DEPTH_COMPONENT);
        XX(DEPTH_COMPONENT16);
        XX(DEPTH_COMPONENT32);
        XX(DEPTH_STENCIL);
        XX(DEPTH24_STENCIL8);
        XX(ETC1_RGB8_OES);
        XX(FLOAT);
        XX(HALF_FLOAT);
        XX(LUMINANCE);
        XX(LUMINANCE_ALPHA);
        XX(RGB);
        XX(RGB16F);
        XX(RGB32F);
        XX(RGBA);
        XX(RGBA16F);
        XX(RGBA32F);
        XX(SRGB);
        XX(SRGB_ALPHA);
        XX(TEXTURE_2D);
        XX(TEXTURE_3D);
        XX(TEXTURE_CUBE_MAP);
        XX(TEXTURE_CUBE_MAP_NEGATIVE_X);
        XX(TEXTURE_CUBE_MAP_NEGATIVE_Y);
        XX(TEXTURE_CUBE_MAP_NEGATIVE_Z);
        XX(TEXTURE_CUBE_MAP_POSITIVE_X);
        XX(TEXTURE_CUBE_MAP_POSITIVE_Y);
        XX(TEXTURE_CUBE_MAP_POSITIVE_Z);
        XX(UNSIGNED_BYTE);
        XX(UNSIGNED_INT);
        XX(UNSIGNED_INT_24_8);
        XX(UNSIGNED_SHORT);
        XX(UNSIGNED_SHORT_4_4_4_4);
        XX(UNSIGNED_SHORT_5_5_5_1);
        XX(UNSIGNED_SHORT_5_6_5);
#undef XX
    }

    return "[Unknown enum name]";
}


bool
WebGLContext::IsTextureFormatCompressed(GLenum format)
{
    switch (format) {
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
        case LOCAL_GL_ETC1_RGB8_OES:
            return true;
        default:
            return false;
    }
}

GLenum
WebGLContext::GetAndFlushUnderlyingGLErrors()
{
    // Get and clear GL error in ALL cases.
    GLenum error = gl->GetAndClearError();

    // Only store in mUnderlyingGLError if is hasn't already recorded an
    // error.
    if (!mUnderlyingGLError)
        mUnderlyingGLError = error;

    return error;
}

#ifdef DEBUG
// For NaNs, etc.
static bool
IsCacheCorrect(float cached, float actual)
{
    if (IsNaN(cached)) {
        // GL is allowed to do anything it wants for NaNs, so if we're shadowing
        // a NaN, then whatever `actual` is might be correct.
        return true;
    }

    return cached == actual;
}

void
AssertUintParamCorrect(gl::GLContext* gl, GLenum pname, GLuint shadow)
{
    GLuint val = 0;
    gl->GetUIntegerv(pname, &val);
    if (val != shadow) {
      printf_stderr("Failed 0x%04x shadow: Cached 0x%x/%u, should be 0x%x/%u.\n",
                    pname, shadow, shadow, val, val);
      MOZ_ASSERT(false, "Bad cached value.");
    }
}
#else
void
AssertUintParamCorrect(gl::GLContext*, GLenum, GLuint)
{
}
#endif

void
WebGLContext::AssertCachedBindings()
{
#ifdef DEBUG
    MakeContextCurrent();

    GetAndFlushUnderlyingGLErrors();

    if (IsExtensionEnabled(WebGLExtensionID::OES_vertex_array_object)) {
        GLuint bound = mBoundVertexArray ? mBoundVertexArray->GLName() : 0;
        AssertUintParamCorrect(gl, LOCAL_GL_VERTEX_ARRAY_BINDING, bound);
    }

    // Bound object state
    GLuint bound = mBoundFramebuffer ? mBoundFramebuffer->GLName() : 0;
    AssertUintParamCorrect(gl, LOCAL_GL_FRAMEBUFFER_BINDING, bound);

    bound = mCurrentProgram ? mCurrentProgram->GLName() : 0;
    AssertUintParamCorrect(gl, LOCAL_GL_CURRENT_PROGRAM, bound);

    // Textures
    GLenum activeTexture = mActiveTexture + LOCAL_GL_TEXTURE0;
    AssertUintParamCorrect(gl, LOCAL_GL_ACTIVE_TEXTURE, activeTexture);

    WebGLTexture* curTex = activeBoundTextureForTarget(LOCAL_GL_TEXTURE_2D);
    bound = curTex ? curTex->GLName() : 0;
    AssertUintParamCorrect(gl, LOCAL_GL_TEXTURE_BINDING_2D, bound);

    curTex = activeBoundTextureForTarget(LOCAL_GL_TEXTURE_CUBE_MAP);
    bound = curTex ? curTex->GLName() : 0;
    AssertUintParamCorrect(gl, LOCAL_GL_TEXTURE_BINDING_CUBE_MAP, bound);

    // Buffers
    bound = mBoundArrayBuffer ? mBoundArrayBuffer->GLName() : 0;
    AssertUintParamCorrect(gl, LOCAL_GL_ARRAY_BUFFER_BINDING, bound);

    MOZ_ASSERT(mBoundVertexArray);
    WebGLBuffer* curBuff = mBoundVertexArray->mBoundElementArrayBuffer;
    bound = curBuff ? curBuff->GLName() : 0;
    AssertUintParamCorrect(gl, LOCAL_GL_ELEMENT_ARRAY_BUFFER_BINDING, bound);

    MOZ_ASSERT(!GetAndFlushUnderlyingGLErrors());
#endif
}

void
WebGLContext::AssertCachedState()
{
#ifdef DEBUG
    MakeContextCurrent();

    GetAndFlushUnderlyingGLErrors();

    // extensions
    if (IsExtensionEnabled(WebGLExtensionID::WEBGL_draw_buffers)) {
        AssertUintParamCorrect(gl, LOCAL_GL_MAX_COLOR_ATTACHMENTS, mGLMaxColorAttachments);
        AssertUintParamCorrect(gl, LOCAL_GL_MAX_DRAW_BUFFERS, mGLMaxDrawBuffers);
    }

    // Draw state
    MOZ_ASSERT(gl->fIsEnabled(LOCAL_GL_SCISSOR_TEST) == mScissorTestEnabled);
    MOZ_ASSERT(gl->fIsEnabled(LOCAL_GL_DITHER) == mDitherEnabled);
    MOZ_ASSERT_IF(IsWebGL2(),
                  gl->fIsEnabled(LOCAL_GL_RASTERIZER_DISCARD) == mRasterizerDiscardEnabled);


    realGLboolean colorWriteMask[4] = {0, 0, 0, 0};
    gl->fGetBooleanv(LOCAL_GL_COLOR_WRITEMASK, colorWriteMask);
    MOZ_ASSERT(colorWriteMask[0] == mColorWriteMask[0] &&
               colorWriteMask[1] == mColorWriteMask[1] &&
               colorWriteMask[2] == mColorWriteMask[2] &&
               colorWriteMask[3] == mColorWriteMask[3]);

    GLfloat colorClearValue[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl->fGetFloatv(LOCAL_GL_COLOR_CLEAR_VALUE, colorClearValue);
    MOZ_ASSERT(IsCacheCorrect(mColorClearValue[0], colorClearValue[0]) &&
               IsCacheCorrect(mColorClearValue[1], colorClearValue[1]) &&
               IsCacheCorrect(mColorClearValue[2], colorClearValue[2]) &&
               IsCacheCorrect(mColorClearValue[3], colorClearValue[3]));

    realGLboolean depthWriteMask = 0;
    gl->fGetBooleanv(LOCAL_GL_DEPTH_WRITEMASK, &depthWriteMask);
    MOZ_ASSERT(depthWriteMask == mDepthWriteMask);

    GLfloat depthClearValue = 0.0f;
    gl->fGetFloatv(LOCAL_GL_DEPTH_CLEAR_VALUE, &depthClearValue);
    MOZ_ASSERT(IsCacheCorrect(mDepthClearValue, depthClearValue));

    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_CLEAR_VALUE, mStencilClearValue);

    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_REF,      mStencilRefFront);
    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_REF, mStencilRefBack);

    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_VALUE_MASK,      mStencilValueMaskFront);
    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_VALUE_MASK, mStencilValueMaskBack);

    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_WRITEMASK,      mStencilWriteMaskFront);
    AssertUintParamCorrect(gl, LOCAL_GL_STENCIL_BACK_WRITEMASK, mStencilWriteMaskBack);

    // Viewport
    GLint int4[4] = {0, 0, 0, 0};
    gl->fGetIntegerv(LOCAL_GL_VIEWPORT, int4);
    MOZ_ASSERT(int4[0] == mViewportX &&
               int4[1] == mViewportY &&
               int4[2] == mViewportWidth &&
               int4[3] == mViewportHeight);

    AssertUintParamCorrect(gl, LOCAL_GL_PACK_ALIGNMENT, mPixelStorePackAlignment);
    AssertUintParamCorrect(gl, LOCAL_GL_UNPACK_ALIGNMENT, mPixelStoreUnpackAlignment);

    MOZ_ASSERT(!GetAndFlushUnderlyingGLErrors());
#endif
}

} // namespace mozilla
