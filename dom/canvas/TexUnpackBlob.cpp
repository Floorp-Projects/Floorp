/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TexUnpackBlob.h"

#include "GLBlitHelper.h"
#include "GLContext.h"
#include "GLDefs.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/RefPtr.h"
#include "nsLayoutUtils.h"
#include "WebGLContext.h"
#include "WebGLTexelConversions.h"
#include "WebGLTexture.h"

namespace mozilla {
namespace webgl {

static GLenum
DoTexOrSubImage(bool isSubImage, gl::GLContext* gl, TexImageTarget target, GLint level,
                const DriverUnpackInfo* dui, GLint xOffset, GLint yOffset, GLint zOffset,
                GLsizei width, GLsizei height, GLsizei depth, const void* data)
{
    if (isSubImage) {
        return DoTexSubImage(gl, target, level, xOffset, yOffset, zOffset, width, height,
                             depth, dui->ToPacking(), data);
    } else {
        return DoTexImage(gl, target, level, dui, width, height, depth, data);
    }
}

/*static*/ void
TexUnpackBlob::OriginsForDOM(WebGLContext* webgl, gl::OriginPos* const out_src,
                             gl::OriginPos* const out_dst)
{
    // Our surfaces are TopLeft.
    *out_src = gl::OriginPos::TopLeft;

    // WebGL specs the default as passing DOM elements top-left first.
    // Thus y-flip would give us bottom-left.
    *out_dst = webgl->mPixelStore_FlipY ? gl::OriginPos::BottomLeft
                                        : gl::OriginPos::TopLeft;
}

//////////////////////////////////////////////////////////////////////////////////////////
// TexUnpackBytes

bool
TexUnpackBytes::ValidateUnpack(WebGLContext* webgl, const char* funcName, bool isFunc3D,
                               const webgl::PackingInfo& pi)
{
    if (!mBytes)
        return true;

    const auto bytesPerPixel = webgl::BytesPerPixel(pi);
    const auto bytesNeeded = webgl->GetUnpackSize(isFunc3D, mWidth, mHeight, mDepth,
                                                  bytesPerPixel);
    if (!bytesNeeded.isValid()) {
        webgl->ErrorInvalidOperation("%s: Overflow while computing the needed buffer"
                                     " size.",
                                     funcName);
        return false;
    }

    if (mByteCount < bytesNeeded.value()) {
        webgl->ErrorInvalidOperation("%s: Provided buffer is too small. (needs %u, has"
                                     " %u)",
                                     funcName, bytesNeeded.value(), mByteCount);
        return false;
    }

    return true;
}

static bool
UnpackFormatHasAlpha(GLenum unpackFormat)
{
    switch (unpackFormat) {
    case LOCAL_GL_ALPHA:
    case LOCAL_GL_LUMINANCE_ALPHA:
    case LOCAL_GL_RGBA:
        return true;

    default:
        return false;
    }
}

static WebGLTexelFormat
FormatFromPacking(const webgl::PackingInfo& pi)
{
    switch (pi.type) {
    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
        return WebGLTexelFormat::RGB565;

    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
        return WebGLTexelFormat::RGBA5551;

    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
        return WebGLTexelFormat::RGBA4444;

    case LOCAL_GL_UNSIGNED_BYTE:
        switch (pi.format) {
        case LOCAL_GL_LUMINANCE:        return WebGLTexelFormat::R8;
        case LOCAL_GL_ALPHA:            return WebGLTexelFormat::A8;
        case LOCAL_GL_LUMINANCE_ALPHA:  return WebGLTexelFormat::RA8;
        case LOCAL_GL_RGB:              return WebGLTexelFormat::RGB8;
        case LOCAL_GL_SRGB:             return WebGLTexelFormat::RGB8;
        case LOCAL_GL_RGBA:             return WebGLTexelFormat::RGBA8;
        case LOCAL_GL_SRGB_ALPHA:       return WebGLTexelFormat::RGBA8;
        }

    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
        switch (pi.format) {
        case LOCAL_GL_LUMINANCE:        return WebGLTexelFormat::R16F;
        case LOCAL_GL_ALPHA:            return WebGLTexelFormat::A16F;
        case LOCAL_GL_LUMINANCE_ALPHA:  return WebGLTexelFormat::RA16F;
        case LOCAL_GL_RGB:              return WebGLTexelFormat::RGB16F;
        case LOCAL_GL_RGBA:             return WebGLTexelFormat::RGBA16F;
        }

    case LOCAL_GL_FLOAT:
        switch (pi.format) {
        case LOCAL_GL_LUMINANCE:        return WebGLTexelFormat::R32F;
        case LOCAL_GL_ALPHA:            return WebGLTexelFormat::A32F;
        case LOCAL_GL_LUMINANCE_ALPHA:  return WebGLTexelFormat::RA32F;
        case LOCAL_GL_RGB:              return WebGLTexelFormat::RGB32F;
        case LOCAL_GL_RGBA:             return WebGLTexelFormat::RGBA32F;
        }
    }

    return WebGLTexelFormat::FormatNotSupportingAnyConversion;
}

void
TexUnpackBytes::TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                              WebGLTexture* tex, TexImageTarget target, GLint level,
                              const webgl::DriverUnpackInfo* dui, GLint xOffset,
                              GLint yOffset, GLint zOffset, GLenum* const out_glError)
{
    WebGLContext* webgl = tex->mContext;
    gl::GLContext* gl = webgl->gl;

    const void* uploadBytes = mBytes;
    UniqueBuffer tempBuffer;

    do {
        if (!mBytes || !mWidth || !mHeight || !mDepth)
            break;

        if (webgl->IsWebGL2())
            break;
        MOZ_ASSERT(mDepth == 1);

        const webgl::PackingInfo pi = { dui->unpackFormat, dui->unpackType };

        const bool needsYFlip = webgl->mPixelStore_FlipY;

        bool needsAlphaPremult = webgl->mPixelStore_PremultiplyAlpha;
        if (!UnpackFormatHasAlpha(pi.format))
            needsAlphaPremult = false;

        if (!needsYFlip && !needsAlphaPremult)
            break;

        // This is literally the worst.
        webgl->GenerateWarning("%s: Uploading ArrayBuffers with FLIP_Y or"
                               " PREMULTIPLY_ALPHA is slow.",
                               funcName);

        tempBuffer = malloc(mByteCount);
        if (!tempBuffer) {
            *out_glError = LOCAL_GL_OUT_OF_MEMORY;
            return;
        }

        const auto bytesPerPixel           = webgl::BytesPerPixel(pi);
        const auto rowByteAlignment        = webgl->mPixelStore_UnpackAlignment;

        const size_t bytesPerRow = bytesPerPixel * mWidth;
        const size_t rowStride = RoundUpToMultipleOf(bytesPerRow, rowByteAlignment);

        if (!needsAlphaPremult) {
            MOZ_ASSERT(needsYFlip);

            const uint8_t* src = (const uint8_t*)mBytes;
            const uint8_t* const srcEnd = src + rowStride * mHeight;

            uint8_t* dst = (uint8_t*)tempBuffer.get() + rowStride * (mHeight - 1);

            while (src != srcEnd) {
                memcpy(dst, src, bytesPerRow);
                src += rowStride;
                dst -= rowStride;
            }

            uploadBytes = tempBuffer.get();
            break;
        }

        const auto texelFormat = FormatFromPacking(pi);
        if (texelFormat == WebGLTexelFormat::FormatNotSupportingAnyConversion) {
            MOZ_ASSERT(false, "Bad texelFormat from pi.");
            *out_glError = LOCAL_GL_OUT_OF_MEMORY;
            return;
        }

        const auto srcOrigin = gl::OriginPos::BottomLeft;
        const auto dstOrigin = (needsYFlip ? gl::OriginPos::TopLeft
                                           : gl::OriginPos::BottomLeft);

        const bool srcPremultiplied = false;
        const bool dstPremultiplied = needsAlphaPremult; // Always true here.

        // And go!:
        MOZ_ASSERT(srcOrigin != dstOrigin || srcPremultiplied != dstPremultiplied);
        bool unused_wasTrivial;
        if (!ConvertImage(mWidth, mHeight,
                          mBytes, rowStride, srcOrigin, texelFormat, srcPremultiplied,
                          tempBuffer.get(), rowStride, dstOrigin, texelFormat,
                          dstPremultiplied, &unused_wasTrivial))
        {
            MOZ_ASSERT(false, "ConvertImage failed unexpectedly.");
            *out_glError = LOCAL_GL_OUT_OF_MEMORY;
            return;
        }

        uploadBytes = tempBuffer.get();
    } while (false);

    GLenum error = DoTexOrSubImage(isSubImage, gl, target, level, dui, xOffset, yOffset,
                                   zOffset, mWidth, mHeight, mDepth, uploadBytes);
    *out_glError = error;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TexUnpackImage

TexUnpackImage::TexUnpackImage(const RefPtr<layers::Image>& image, bool isAlphaPremult)
    : TexUnpackBlob(image->GetSize().width, image->GetSize().height, 1, true)
    , mImage(image)
    , mIsAlphaPremult(isAlphaPremult)
{ }

TexUnpackImage::~TexUnpackImage()
{ }

void
TexUnpackImage::TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                              WebGLTexture* tex, TexImageTarget target, GLint level,
                              const webgl::DriverUnpackInfo* dui, GLint xOffset,
                              GLint yOffset, GLint zOffset, GLenum* const out_glError)
{
    MOZ_ASSERT_IF(needsRespec, !isSubImage);
    *out_glError = 0;

    WebGLContext* webgl = tex->mContext;

    gl::GLContext* gl = webgl->GL();
    gl->MakeCurrent();

    if (needsRespec) {
        GLenum error = DoTexOrSubImage(isSubImage, gl, target.get(), level, dui, xOffset,
                                       yOffset, zOffset, mWidth, mHeight, mDepth,
                                       nullptr);
        if (error) {
            MOZ_ASSERT(!error);
            *out_glError = LOCAL_GL_OUT_OF_MEMORY;
            return;
        }
    }

    do {
        if (dui->unpackFormat != LOCAL_GL_RGB && dui->unpackFormat != LOCAL_GL_RGBA)
            break;

        if (dui->unpackType != LOCAL_GL_UNSIGNED_BYTE)
            break;

        gl::ScopedFramebuffer scopedFB(gl);
        gl::ScopedBindFramebuffer bindFB(gl, scopedFB.FB());

        {
            gl::GLContext::LocalErrorScope errorScope(*gl);

            gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                      target.get(), tex->mGLName, level);

            if (errorScope.GetError())
                break;
        }

        const GLenum status = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
        if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE)
            break;

        gl::OriginPos srcOrigin, dstOrigin;
        OriginsForDOM(webgl, &srcOrigin, &dstOrigin);

        const gfx::IntSize destSize(mWidth, mHeight);
        if (!gl->BlitHelper()->BlitImageToFramebuffer(mImage, destSize, scopedFB.FB(),
                                                      dstOrigin))
        {
            break;
        }

        return; // Blitting was successful, so we're done!
    } while (false);

    webgl->GenerateWarning("%s: Failed to hit GPU-copy fast-path. Falling back to CPU"
                           " upload.",
                           funcName);

    RefPtr<SourceSurface> surface = mImage->GetAsSourceSurface();
    if (!surface) {
        *out_glError = LOCAL_GL_OUT_OF_MEMORY;
        return;
    }

    TexUnpackSurface surfBlob(surface, mIsAlphaPremult);

    surfBlob.TexOrSubImage(isSubImage, needsRespec, funcName, tex, target, level, dui,
                           xOffset, yOffset, zOffset, out_glError);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TexUnpackSurface

static bool
GuessAlignment(const void* data, size_t bytesPerRow, size_t stride, size_t maxAlignment,
               size_t* const out_alignment)
{
    size_t alignmentGuess = maxAlignment;
    while (alignmentGuess) {
        size_t guessStride = RoundUpToMultipleOf(bytesPerRow, alignmentGuess);
        if (guessStride == stride &&
            uintptr_t(data) % alignmentGuess == 0)
        {
            *out_alignment = alignmentGuess;
            return true;
        }
        alignmentGuess /= 2;
    }
    return false;
}

static bool
SupportsBGRA(gl::GLContext* gl)
{
    if (gl->IsANGLE())
        return true;

    return false;
}

/*static*/ bool
TexUnpackSurface::UploadDataSurface(bool isSubImage, WebGLContext* webgl,
                                    TexImageTarget target, GLint level,
                                    const webgl::DriverUnpackInfo* dui, GLint xOffset,
                                    GLint yOffset, GLint zOffset, GLsizei width,
                                    GLsizei height, gfx::DataSourceSurface* surf,
                                    bool isSurfAlphaPremult, GLenum* const out_glError)
{
    gl::GLContext* gl = webgl->GL();
    MOZ_ASSERT(gl->IsCurrent());
    *out_glError = 0;

    if (isSurfAlphaPremult != webgl->mPixelStore_PremultiplyAlpha)
        return false;

    gl::OriginPos srcOrigin, dstOrigin;
    OriginsForDOM(webgl, &srcOrigin, &dstOrigin);
    if (srcOrigin != dstOrigin)
        return false;

    // This differs from the raw-data upload in that we choose how we do the unpack.
    // (alignment, etc.)

    // Uploading RGBX as RGBA and blitting to RGB is faster than repacking RGBX into
    // RGB on the CPU. However, this is optimization is out-of-scope for now.

    static const webgl::DriverUnpackInfo kInfoBGRA = {
        LOCAL_GL_BGRA,
        LOCAL_GL_BGRA,
        LOCAL_GL_UNSIGNED_BYTE,
    };

    const webgl::DriverUnpackInfo* chosenDUI = nullptr;

    switch (surf->GetFormat()) {
    case gfx::SurfaceFormat::B8G8R8A8:
        if (SupportsBGRA(gl) &&
            dui->internalFormat == LOCAL_GL_RGBA &&
            dui->unpackFormat == LOCAL_GL_RGBA &&
            dui->unpackType == LOCAL_GL_UNSIGNED_BYTE)
        {
            chosenDUI = &kInfoBGRA;
        }
        break;

    case gfx::SurfaceFormat::R8G8B8A8:
        if (dui->unpackFormat == LOCAL_GL_RGBA &&
            dui->unpackType == LOCAL_GL_UNSIGNED_BYTE)
        {
            chosenDUI = dui;
        }
        break;

    case gfx::SurfaceFormat::R5G6B5_UINT16:
        if (dui->unpackFormat == LOCAL_GL_RGB &&
            dui->unpackType == LOCAL_GL_UNSIGNED_SHORT_5_6_5)
        {
            chosenDUI = dui;
        }
        break;

    default:
        break;
    }

    if (!chosenDUI)
        return false;

    gfx::DataSourceSurface::ScopedMap map(surf, gfx::DataSourceSurface::MapType::READ);
    if (!map.IsMapped())
        return false;

    const webgl::PackingInfo pi = {chosenDUI->unpackFormat, chosenDUI->unpackType};
    const auto bytesPerPixel = webgl::BytesPerPixel(pi);
    const size_t bytesPerRow = width * bytesPerPixel;

    const GLint kMaxUnpackAlignment = 8;
    size_t unpackAlignment;
    if (!GuessAlignment(map.GetData(), bytesPerRow, map.GetStride(), kMaxUnpackAlignment,
                        &unpackAlignment))
    {
        return false;
        // TODO: Consider using UNPACK_ settings to set the stride based on the too-large
        // alignment used for some SourceSurfaces. (D2D allegedy likes alignment=16)
    }

    MOZ_ALWAYS_TRUE( webgl->gl->MakeCurrent() );
    ScopedUnpackReset scopedReset(webgl);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, unpackAlignment);

    const GLsizei depth = 1;
    GLenum error = DoTexOrSubImage(isSubImage, gl, target.get(), level, chosenDUI,
                                   xOffset, yOffset, zOffset, width, height, depth,
                                   map.GetData());
    if (error) {
        *out_glError = error;
        return false;
    }

    return true;
}

////////////////////

static bool
GetFormatForSurf(gfx::SourceSurface* surf, WebGLTexelFormat* const out_texelFormat)
{
    gfx::SurfaceFormat surfFormat = surf->GetFormat();

    switch (surfFormat) {
    case gfx::SurfaceFormat::B8G8R8A8:
        *out_texelFormat = WebGLTexelFormat::BGRA8;
        return true;

    case gfx::SurfaceFormat::B8G8R8X8:
        *out_texelFormat = WebGLTexelFormat::BGRX8;
        return true;

    case gfx::SurfaceFormat::R8G8B8A8:
        *out_texelFormat = WebGLTexelFormat::RGBA8;
        return true;

    case gfx::SurfaceFormat::R8G8B8X8:
        *out_texelFormat = WebGLTexelFormat::RGBX8;
        return true;

    case gfx::SurfaceFormat::R5G6B5_UINT16:
        *out_texelFormat = WebGLTexelFormat::RGB565;
        return true;

    case gfx::SurfaceFormat::A8:
        *out_texelFormat = WebGLTexelFormat::A8;
        return true;

    case gfx::SurfaceFormat::YUV:
        // Ugh...
        NS_ERROR("We don't handle uploads from YUV sources yet.");
        // When we want to, check out gfx/ycbcr/YCbCrUtils.h. (specifically
        // GetYCbCrToRGBDestFormatAndSize and ConvertYCbCrToRGB)
        return false;

    default:
        return false;
    }
}

static bool
GetFormatForPackingTuple(GLenum packingFormat, GLenum packingType,
                         WebGLTexelFormat* const out_texelFormat)
{
    switch (packingType) {
    case LOCAL_GL_UNSIGNED_BYTE:
        switch (packingFormat) {
        case LOCAL_GL_RED:
        case LOCAL_GL_LUMINANCE:
        case LOCAL_GL_RED_INTEGER:
            *out_texelFormat = WebGLTexelFormat::R8;
            return true;

        case LOCAL_GL_ALPHA:
            *out_texelFormat = WebGLTexelFormat::A8;
            return true;

        case LOCAL_GL_LUMINANCE_ALPHA:
            *out_texelFormat = WebGLTexelFormat::RA8;
            return true;

        case LOCAL_GL_RGB:
        case LOCAL_GL_RGB_INTEGER:
            *out_texelFormat = WebGLTexelFormat::RGB8;
            return true;

        case LOCAL_GL_RGBA:
        case LOCAL_GL_RGBA_INTEGER:
            *out_texelFormat = WebGLTexelFormat::RGBA8;
            return true;

        case LOCAL_GL_RG:
        case LOCAL_GL_RG_INTEGER:
            *out_texelFormat = WebGLTexelFormat::RG8;
            return true;

        default:
            break;
        }
        break;

    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
        switch (packingFormat) {
        case LOCAL_GL_RGB:
            *out_texelFormat = WebGLTexelFormat::RGB565;
            return true;

        default:
            break;
        }
        break;

    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
        switch (packingFormat) {
        case LOCAL_GL_RGBA:
            *out_texelFormat = WebGLTexelFormat::RGBA5551;
            return true;

        default:
            break;
        }
        break;

    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
        switch (packingFormat) {
        case LOCAL_GL_RGBA:
            *out_texelFormat = WebGLTexelFormat::RGBA4444;
            return true;

        default:
            break;
        }
        break;

    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
        switch (packingFormat) {
        case LOCAL_GL_RED:
        case LOCAL_GL_LUMINANCE:
            *out_texelFormat = WebGLTexelFormat::R16F;
            return true;

        case LOCAL_GL_ALPHA:
            *out_texelFormat = WebGLTexelFormat::A16F;
            return true;

        case LOCAL_GL_LUMINANCE_ALPHA:
            *out_texelFormat = WebGLTexelFormat::RA16F;
            return true;

        case LOCAL_GL_RGB:
            *out_texelFormat = WebGLTexelFormat::RGB16F;
            return true;

        case LOCAL_GL_RGBA:
            *out_texelFormat = WebGLTexelFormat::RGBA16F;
            return true;

        case LOCAL_GL_RG:
            *out_texelFormat = WebGLTexelFormat::RG16F;
            return true;

        default:
            break;
        }
        break;

    case LOCAL_GL_FLOAT:
        switch (packingFormat) {
        case LOCAL_GL_RED:
        case LOCAL_GL_LUMINANCE:
            *out_texelFormat = WebGLTexelFormat::R32F;
            return true;

        case LOCAL_GL_ALPHA:
            *out_texelFormat = WebGLTexelFormat::A32F;
            return true;

        case LOCAL_GL_LUMINANCE_ALPHA:
            *out_texelFormat = WebGLTexelFormat::RA32F;
            return true;

        case LOCAL_GL_RGB:
            *out_texelFormat = WebGLTexelFormat::RGB32F;
            return true;

        case LOCAL_GL_RGBA:
            *out_texelFormat = WebGLTexelFormat::RGBA32F;
            return true;

        case LOCAL_GL_RG:
            *out_texelFormat = WebGLTexelFormat::RG32F;
            return true;

        default:
            break;
        }
        break;

    case LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV:
        switch (packingFormat) {
        case LOCAL_GL_RGB:
            *out_texelFormat = WebGLTexelFormat::RGB11F11F10F;
            return true;

        default:
            break;
        }
        break;
    default:
        break;
    }

    NS_ERROR("Unsupported EffectiveFormat dest format for DOM element upload.");
    return false;
}

/*static*/ bool
TexUnpackSurface::ConvertSurface(WebGLContext* webgl, const webgl::DriverUnpackInfo* dui,
                                 gfx::DataSourceSurface* surf, bool isSurfAlphaPremult,
                                 UniqueBuffer* const out_convertedBuffer,
                                 uint8_t* const out_convertedAlignment,
                                 bool* const out_wasTrivial, bool* const out_outOfMemory)
{
    *out_outOfMemory = false;

    const size_t width = surf->GetSize().width;
    const size_t height = surf->GetSize().height;

    // Source args:

    // After we call this, on OSX, our GLContext will no longer be current.
    gfx::DataSourceSurface::ScopedMap srcMap(surf, gfx::DataSourceSurface::MapType::READ);
    if (!srcMap.IsMapped())
        return false;

    const void* const srcBegin = srcMap.GetData();
    const size_t srcStride = srcMap.GetStride();

    WebGLTexelFormat srcFormat;
    if (!GetFormatForSurf(surf, &srcFormat))
        return false;

    const bool srcPremultiplied = isSurfAlphaPremult;

    // Dest args:

    WebGLTexelFormat dstFormat;
    if (!GetFormatForPackingTuple(dui->unpackFormat, dui->unpackType, &dstFormat))
        return false;

    const auto bytesPerPixel = webgl::BytesPerPixel({dui->unpackFormat, dui->unpackType});
    const size_t dstRowBytes = bytesPerPixel * width;

    const size_t dstAlignment = 8; // Just use the max!
    const size_t dstStride = RoundUpToMultipleOf(dstRowBytes, dstAlignment);

    CheckedUint32 checkedDstSize = dstStride;
    checkedDstSize *= height;
    if (!checkedDstSize.isValid()) {
        *out_outOfMemory = true;
        return false;
    }

    const size_t dstSize = checkedDstSize.value();

    UniqueBuffer dstBuffer = malloc(dstSize);
    if (!dstBuffer) {
        *out_outOfMemory = true;
        return false;
    }
    void* const dstBegin = dstBuffer.get();

    gl::OriginPos srcOrigin, dstOrigin;
    OriginsForDOM(webgl, &srcOrigin, &dstOrigin);

    const bool dstPremultiplied = webgl->mPixelStore_PremultiplyAlpha;

    // And go!:
    bool wasTrivial;
    if (!ConvertImage(width, height,
                      srcBegin, srcStride, srcOrigin, srcFormat, srcPremultiplied,
                      dstBegin, dstStride, dstOrigin, dstFormat, dstPremultiplied,
                      &wasTrivial))
    {
        MOZ_ASSERT(false, "ConvertImage failed unexpectedly.");
        NS_ERROR("ConvertImage failed unexpectedly.");
        *out_outOfMemory = true;
        return false;
    }

    *out_convertedBuffer = Move(dstBuffer);
    *out_convertedAlignment = dstAlignment;
    *out_wasTrivial = wasTrivial;
    return true;
}


////////////////////

TexUnpackSurface::TexUnpackSurface(const RefPtr<gfx::SourceSurface>& surf,
                                   bool isAlphaPremult)
    : TexUnpackBlob(surf->GetSize().width, surf->GetSize().height, 1, true)
    , mSurf(surf)
    , mIsAlphaPremult(isAlphaPremult)
{ }

TexUnpackSurface::~TexUnpackSurface()
{ }

void
TexUnpackSurface::TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                                WebGLTexture* tex, TexImageTarget target, GLint level,
                                const webgl::DriverUnpackInfo* dui, GLint xOffset,
                                GLint yOffset, GLint zOffset, GLenum* const out_glError)
{
    *out_glError = 0;

    WebGLContext* webgl = tex->mContext;

    // MakeCurrent is a big mess in here, because mapping (and presumably unmapping) on
    // OSX can lose our MakeCurrent. Therefore it's easiest to MakeCurrent just before we
    // call into GL, instead of trying to keep MakeCurrent-ed.

    RefPtr<gfx::DataSourceSurface> dataSurf = mSurf->GetDataSurface();

    if (!dataSurf) {
        // Since GetDataSurface didn't return error code, assume system
        // is out of memory
        *out_glError = LOCAL_GL_OUT_OF_MEMORY;
        return;
    }

    GLenum error;
    if (UploadDataSurface(isSubImage, webgl, target, level, dui, xOffset, yOffset,
                          zOffset, mWidth, mHeight, dataSurf, mIsAlphaPremult, &error))
    {
        return;
    }
    if (error == LOCAL_GL_OUT_OF_MEMORY) {
        *out_glError = LOCAL_GL_OUT_OF_MEMORY;
        return;
    }

    // CPU conversion. (++numCopies)

    UniqueBuffer convertedBuffer;
    uint8_t convertedAlignment;
    bool wasTrivial;
    bool outOfMemory;
    if (!ConvertSurface(webgl, dui, dataSurf, mIsAlphaPremult, &convertedBuffer,
                        &convertedAlignment, &wasTrivial, &outOfMemory))
    {
        if (outOfMemory) {
            *out_glError = LOCAL_GL_OUT_OF_MEMORY;
        } else {
            NS_ERROR("Failed to convert surface.");
            *out_glError = LOCAL_GL_OUT_OF_MEMORY;
        }
        return;
    }

    if (!wasTrivial) {
        webgl->GenerateWarning("%s: Chosen format/type incured an expensive reformat:"
                               " 0x%04x/0x%04x",
                               funcName, dui->unpackFormat, dui->unpackType);
    }

    MOZ_ALWAYS_TRUE( webgl->gl->MakeCurrent() );
    ScopedUnpackReset scopedReset(webgl);
    webgl->gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, convertedAlignment);

    error = DoTexOrSubImage(isSubImage, webgl->gl, target.get(), level, dui, xOffset,
                            yOffset, zOffset, mWidth, mHeight, mDepth,
                            convertedBuffer.get());
    *out_glError = error;
}

} // namespace webgl
} // namespace mozilla
