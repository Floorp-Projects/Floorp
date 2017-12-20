/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TexUnpackBlob.h"

#include "GLBlitHelper.h"
#include "GLContext.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/RefPtr.h"
#include "nsLayoutUtils.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLTexelConversions.h"
#include "WebGLTexture.h"

namespace mozilla {
namespace webgl {

static bool
IsPIValidForDOM(const webgl::PackingInfo& pi)
{
    // https://www.khronos.org/registry/webgl/specs/latest/2.0/#TEXTURE_TYPES_FORMATS_FROM_DOM_ELEMENTS_TABLE

    // Just check for invalid individual formats and types, not combinations.
    switch (pi.format) {
    case LOCAL_GL_RGB:
    case LOCAL_GL_RGBA:
    case LOCAL_GL_LUMINANCE_ALPHA:
    case LOCAL_GL_LUMINANCE:
    case LOCAL_GL_ALPHA:
    case LOCAL_GL_RED:
    case LOCAL_GL_RED_INTEGER:
    case LOCAL_GL_RG:
    case LOCAL_GL_RG_INTEGER:
    case LOCAL_GL_RGB_INTEGER:
    case LOCAL_GL_RGBA_INTEGER:
        break;

    case LOCAL_GL_SRGB:
    case LOCAL_GL_SRGB_ALPHA:
        // Allowed in WebGL1+EXT_srgb
        break;

    default:
        return false;
    }

    switch (pi.type) {
    case LOCAL_GL_UNSIGNED_BYTE:
    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
    case LOCAL_GL_FLOAT:
    case LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV:
        break;

    default:
        return false;
    }

    return true;
}

static bool
ValidatePIForDOM(WebGLContext* webgl, const char* funcName,
                 const webgl::PackingInfo& pi)
{
    if (!IsPIValidForDOM(pi)) {
        webgl->ErrorInvalidOperation("%s: Format or type is invalid for DOM sources.",
                                     funcName);
        return false;
    }
    return true;
}

static WebGLTexelFormat
FormatForPackingInfo(const PackingInfo& pi)
{
    switch (pi.type) {
    case LOCAL_GL_UNSIGNED_BYTE:
        switch (pi.format) {
        case LOCAL_GL_RED:
        case LOCAL_GL_LUMINANCE:
        case LOCAL_GL_RED_INTEGER:
            return WebGLTexelFormat::R8;

        case LOCAL_GL_ALPHA:
            return WebGLTexelFormat::A8;

        case LOCAL_GL_LUMINANCE_ALPHA:
            return WebGLTexelFormat::RA8;

        case LOCAL_GL_RGB:
        case LOCAL_GL_RGB_INTEGER:
        case LOCAL_GL_SRGB:
            return WebGLTexelFormat::RGB8;

        case LOCAL_GL_RGBA:
        case LOCAL_GL_RGBA_INTEGER:
        case LOCAL_GL_SRGB_ALPHA:
            return WebGLTexelFormat::RGBA8;

        case LOCAL_GL_RG:
        case LOCAL_GL_RG_INTEGER:
            return WebGLTexelFormat::RG8;

        default:
            break;
        }
        break;

    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
        if (pi.format == LOCAL_GL_RGB)
            return WebGLTexelFormat::RGB565;
        break;

    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
        if (pi.format == LOCAL_GL_RGBA)
            return WebGLTexelFormat::RGBA5551;
        break;

    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
        if (pi.format == LOCAL_GL_RGBA)
            return WebGLTexelFormat::RGBA4444;
        break;

    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
        switch (pi.format) {
        case LOCAL_GL_RED:
        case LOCAL_GL_LUMINANCE:
            return WebGLTexelFormat::R16F;

        case LOCAL_GL_ALPHA:           return WebGLTexelFormat::A16F;
        case LOCAL_GL_LUMINANCE_ALPHA: return WebGLTexelFormat::RA16F;
        case LOCAL_GL_RG:              return WebGLTexelFormat::RG16F;
        case LOCAL_GL_RGB:             return WebGLTexelFormat::RGB16F;
        case LOCAL_GL_RGBA:            return WebGLTexelFormat::RGBA16F;

        default:
            break;
        }
        break;

    case LOCAL_GL_FLOAT:
        switch (pi.format) {
        case LOCAL_GL_RED:
        case LOCAL_GL_LUMINANCE:
            return WebGLTexelFormat::R32F;

        case LOCAL_GL_ALPHA:           return WebGLTexelFormat::A32F;
        case LOCAL_GL_LUMINANCE_ALPHA: return WebGLTexelFormat::RA32F;
        case LOCAL_GL_RG:              return WebGLTexelFormat::RG32F;
        case LOCAL_GL_RGB:             return WebGLTexelFormat::RGB32F;
        case LOCAL_GL_RGBA:            return WebGLTexelFormat::RGBA32F;

        default:
            break;
        }
        break;

    case LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV:
        if (pi.format == LOCAL_GL_RGB)
            return WebGLTexelFormat::RGB11F11F10F;
        break;

    default:
        break;
    }

    return WebGLTexelFormat::FormatNotSupportingAnyConversion;
}

////////////////////

static bool
ValidateUnpackPixels(WebGLContext* webgl, const char* funcName, uint32_t fullRows,
                     uint32_t tailPixels, webgl::TexUnpackBlob* blob)
{
    if (!blob->mWidth || !blob->mHeight || !blob->mDepth)
        return true;

    const auto usedPixelsPerRow = CheckedUint32(blob->mSkipPixels) + blob->mWidth;
    if (!usedPixelsPerRow.isValid() || usedPixelsPerRow.value() > blob->mRowLength) {
        webgl->ErrorInvalidOperation("%s: UNPACK_SKIP_PIXELS + width >"
                                     " UNPACK_ROW_LENGTH.",
                                     funcName);
        return false;
    }

    if (blob->mHeight > blob->mImageHeight) {
        webgl->ErrorInvalidOperation("%s: height > UNPACK_IMAGE_HEIGHT.", funcName);
        return false;
    }

    //////

    // The spec doesn't bound SKIP_ROWS + height <= IMAGE_HEIGHT, unfortunately.
    auto skipFullRows = CheckedUint32(blob->mSkipImages) * blob->mImageHeight;
    skipFullRows += blob->mSkipRows;

    MOZ_ASSERT(blob->mDepth >= 1);
    MOZ_ASSERT(blob->mHeight >= 1);
    auto usedFullRows = CheckedUint32(blob->mDepth - 1) * blob->mImageHeight;
    usedFullRows += blob->mHeight - 1; // Full rows in the final image, excluding the tail.

    const auto fullRowsNeeded = skipFullRows + usedFullRows;
    if (!fullRowsNeeded.isValid()) {
        webgl->ErrorOutOfMemory("%s: Invalid calculation for required row count.",
                                funcName);
        return false;
    }

    if (fullRows > fullRowsNeeded.value())
        return true;

    if (fullRows == fullRowsNeeded.value() && tailPixels >= usedPixelsPerRow.value()) {
        blob->mNeedsExactUpload = true;
        return true;
    }

    webgl->ErrorInvalidOperation("%s: Desired upload requires more data than is"
                                 " available: (%u rows plus %u pixels needed, %u rows"
                                 " plus %u pixels available)",
                                 funcName, fullRowsNeeded.value(),
                                 usedPixelsPerRow.value(), fullRows, tailPixels);
    return false;
}

static bool
ValidateUnpackBytes(WebGLContext* webgl, const char* funcName,
                    const webgl::PackingInfo& pi, size_t availByteCount,
                    webgl::TexUnpackBlob* blob)
{
    if (!blob->mWidth || !blob->mHeight || !blob->mDepth)
        return true;

    const auto bytesPerPixel = webgl::BytesPerPixel(pi);
    const auto bytesPerRow = CheckedUint32(blob->mRowLength) * bytesPerPixel;
    const auto rowStride = RoundUpToMultipleOf(bytesPerRow, blob->mAlignment);

    const auto fullRows = availByteCount / rowStride;
    if (!fullRows.isValid()) {
        webgl->ErrorOutOfMemory("%s: Unacceptable upload size calculated.", funcName);
        return false;
    }

    const auto bodyBytes = fullRows.value() * rowStride.value();
    const auto tailPixels = (availByteCount - bodyBytes) / bytesPerPixel;

    return ValidateUnpackPixels(webgl, funcName, fullRows.value(), tailPixels, blob);
}

////////////////////

static uint32_t
ZeroOn2D(TexImageTarget target, uint32_t val)
{
    return (IsTarget3D(target) ? val : 0);
}

static uint32_t
FallbackOnZero(uint32_t val, uint32_t fallback)
{
    return (val ? val : fallback);
}

TexUnpackBlob::TexUnpackBlob(const WebGLContext* webgl, TexImageTarget target,
                             uint32_t rowLength, uint32_t width, uint32_t height,
                             uint32_t depth, gfxAlphaType srcAlphaType)
    : mAlignment(webgl->mPixelStore_UnpackAlignment)
    , mRowLength(rowLength)
    , mImageHeight(FallbackOnZero(ZeroOn2D(target, webgl->mPixelStore_UnpackImageHeight),
                                  height))

    , mSkipPixels(webgl->mPixelStore_UnpackSkipPixels)
    , mSkipRows(webgl->mPixelStore_UnpackSkipRows)
    , mSkipImages(ZeroOn2D(target, webgl->mPixelStore_UnpackSkipImages))

    , mWidth(width)
    , mHeight(height)
    , mDepth(depth)

    , mSrcAlphaType(srcAlphaType)

    , mNeedsExactUpload(false)
{
    MOZ_ASSERT_IF(!IsTarget3D(target), mDepth == 1);
}

static bool
HasColorAndAlpha(const WebGLTexelFormat format)
{
    switch (format) {
    case WebGLTexelFormat::RA8:
    case WebGLTexelFormat::RA16F:
    case WebGLTexelFormat::RA32F:
    case WebGLTexelFormat::RGBA8:
    case WebGLTexelFormat::RGBA5551:
    case WebGLTexelFormat::RGBA4444:
    case WebGLTexelFormat::RGBA16F:
    case WebGLTexelFormat::RGBA32F:
    case WebGLTexelFormat::BGRA8:
        return true;
    default:
        return false;
    }
}

bool
TexUnpackBlob::ConvertIfNeeded(WebGLContext* webgl, const char* funcName,
                               const uint32_t rowLength, const uint32_t rowCount,
                               WebGLTexelFormat srcFormat,
                               const uint8_t* const srcBegin, const ptrdiff_t srcStride,
                               WebGLTexelFormat dstFormat, const ptrdiff_t dstStride,
                               const uint8_t** const out_begin,
                               UniqueBuffer* const out_anchoredBuffer) const
{
    MOZ_ASSERT(srcFormat != WebGLTexelFormat::FormatNotSupportingAnyConversion);
    MOZ_ASSERT(dstFormat != WebGLTexelFormat::FormatNotSupportingAnyConversion);

    *out_begin = srcBegin;

    if (!rowLength || !rowCount)
        return true;

    const auto srcIsPremult = (mSrcAlphaType == gfxAlphaType::Premult);
    const auto& dstIsPremult = webgl->mPixelStore_PremultiplyAlpha;
    const auto fnHasPremultMismatch = [&]() {
        if (mSrcAlphaType == gfxAlphaType::Opaque)
            return false;

        if (!HasColorAndAlpha(srcFormat))
            return false;

        return srcIsPremult != dstIsPremult;
    };

    const auto srcOrigin = (webgl->mPixelStore_FlipY ? gl::OriginPos::TopLeft
                                                     : gl::OriginPos::BottomLeft);
    const auto dstOrigin = gl::OriginPos::BottomLeft;

    if (srcFormat != dstFormat) {
        webgl->GeneratePerfWarning("%s: Conversion requires pixel reformatting. (%u->%u)",
                                   funcName, uint32_t(srcFormat),
                                   uint32_t(dstFormat));
    } else if (fnHasPremultMismatch()) {
        webgl->GeneratePerfWarning("%s: Conversion requires change in"
                                   " alpha-premultiplication.",
                                   funcName);
    } else if (srcOrigin != dstOrigin) {
        webgl->GeneratePerfWarning("%s: Conversion requires y-flip.", funcName);
    } else if (srcStride != dstStride) {
        webgl->GeneratePerfWarning("%s: Conversion requires change in stride. (%u->%u)",
                                   funcName, uint32_t(srcStride), uint32_t(dstStride));
    } else {
        return true;
    }

    ////

    const auto dstTotalBytes = CheckedUint32(rowCount) * dstStride;
    if (!dstTotalBytes.isValid()) {
        webgl->ErrorOutOfMemory("%s: Calculation failed.", funcName);
        return false;
    }

    UniqueBuffer dstBuffer = calloc(1, dstTotalBytes.value());
    if (!dstBuffer.get()) {
        webgl->ErrorOutOfMemory("%s: Failed to allocate dest buffer.", funcName);
        return false;
    }
    const auto dstBegin = static_cast<uint8_t*>(dstBuffer.get());

    ////

    // And go!:
    bool wasTrivial;
    if (!ConvertImage(rowLength, rowCount,
                      srcBegin, srcStride, srcOrigin, srcFormat, srcIsPremult,
                      dstBegin, dstStride, dstOrigin, dstFormat, dstIsPremult,
                      &wasTrivial))
    {
        webgl->ErrorImplementationBug("%s: ConvertImage failed.", funcName);
        return false;
    }

    *out_begin = dstBegin;
    *out_anchoredBuffer = Move(dstBuffer);
    return true;
}

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

//////////////////////////////////////////////////////////////////////////////////////////
// TexUnpackBytes

TexUnpackBytes::TexUnpackBytes(const WebGLContext* webgl, TexImageTarget target,
                               uint32_t width, uint32_t height, uint32_t depth,
                               bool isClientData, const uint8_t* ptr, size_t availBytes)
    : TexUnpackBlob(webgl, target,
                    FallbackOnZero(webgl->mPixelStore_UnpackRowLength, width),
                    width, height, depth, gfxAlphaType::NonPremult)
    , mIsClientData(isClientData)
    , mPtr(ptr)
    , mAvailBytes(availBytes)
{ }

bool
TexUnpackBytes::Validate(WebGLContext* webgl, const char* funcName,
                         const webgl::PackingInfo& pi)
{
    if (mIsClientData && !mPtr)
        return true;

    return ValidateUnpackBytes(webgl, funcName, pi, mAvailBytes, this);
}

bool
TexUnpackBytes::TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                              WebGLTexture* tex, TexImageTarget target, GLint level,
                              const webgl::DriverUnpackInfo* dui, GLint xOffset,
                              GLint yOffset, GLint zOffset, const webgl::PackingInfo& pi,
                              GLenum* const out_error) const
{
    WebGLContext* webgl = tex->mContext;

    const auto format = FormatForPackingInfo(pi);
    const auto bytesPerPixel = webgl::BytesPerPixel(pi);

    const uint8_t* uploadPtr = mPtr;
    UniqueBuffer tempBuffer;

    do {
        if (!mIsClientData || !mPtr)
            break;

        if (!webgl->mPixelStore_FlipY &&
            !webgl->mPixelStore_PremultiplyAlpha)
        {
            break;
        }

        if (webgl->mPixelStore_UnpackImageHeight ||
            webgl->mPixelStore_UnpackSkipImages ||
            webgl->mPixelStore_UnpackRowLength ||
            webgl->mPixelStore_UnpackSkipRows ||
            webgl->mPixelStore_UnpackSkipPixels)
        {
            webgl->ErrorInvalidOperation("%s: Non-DOM-Element uploads with alpha-premult"
                                         " or y-flip do not support subrect selection.",
                                         funcName);
            return false;
        }

        webgl->GenerateWarning("%s: Alpha-premult and y-flip are deprecated for"
                               " non-DOM-Element uploads.",
                               funcName);

        const uint32_t rowLength = mWidth;
        const uint32_t rowCount = mHeight * mDepth;
        const auto stride = RoundUpToMultipleOf(rowLength * bytesPerPixel, mAlignment);
        if (!ConvertIfNeeded(webgl, funcName, rowLength, rowCount, format, mPtr, stride,
                             format, stride, &uploadPtr, &tempBuffer))
        {
            return false;
        }
    } while (false);

    //////

    const auto& gl = webgl->gl;

    bool useParanoidHandling = false;
    if (mNeedsExactUpload && webgl->mBoundPixelUnpackBuffer) {
        webgl->GenerateWarning("%s: Uploads from a buffer with a final row with a byte"
                               " count smaller than the row stride can incur extra"
                               " overhead.",
                               funcName);

        if (gl->WorkAroundDriverBugs()) {
            useParanoidHandling |= (gl->Vendor() == gl::GLVendor::NVIDIA);
        }
    }

    if (!useParanoidHandling) {
        if (webgl->mBoundPixelUnpackBuffer) {
            gl->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER,
                            webgl->mBoundPixelUnpackBuffer->mGLName);
        }

        *out_error = DoTexOrSubImage(isSubImage, gl, target, level, dui, xOffset, yOffset,
                                     zOffset, mWidth, mHeight, mDepth, uploadPtr);

        if (webgl->mBoundPixelUnpackBuffer) {
            gl->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);
        }
        return true;
    }

    //////

    MOZ_ASSERT(webgl->mBoundPixelUnpackBuffer);

    if (!isSubImage) {
        // Alloc first to catch OOMs.
        AssertUintParamCorrect(gl, LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);
        *out_error = DoTexOrSubImage(false, gl, target, level, dui, xOffset, yOffset,
                                     zOffset, mWidth, mHeight, mDepth, nullptr);
        if (*out_error)
            return true;
    }

    const ScopedLazyBind bindPBO(gl, LOCAL_GL_PIXEL_UNPACK_BUFFER,
                                 webgl->mBoundPixelUnpackBuffer);

    //////

    // Make our sometimes-implicit values explicit. Also this keeps them constant when we
    // ask for height=mHeight-1 and such.
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, mRowLength);
    gl->fPixelStorei(LOCAL_GL_UNPACK_IMAGE_HEIGHT, mImageHeight);

    if (mDepth > 1) {
        *out_error = DoTexOrSubImage(true, gl, target, level, dui, xOffset, yOffset,
                                     zOffset, mWidth, mHeight, mDepth-1, uploadPtr);
    }

    // Skip the images we uploaded.
    gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_IMAGES, mSkipImages + mDepth - 1);

    if (mHeight > 1) {
        *out_error = DoTexOrSubImage(true, gl, target, level, dui, xOffset, yOffset,
                                     zOffset+mDepth-1, mWidth, mHeight-1, 1, uploadPtr);
    }

    const auto totalSkipRows = CheckedUint32(mSkipImages) * mImageHeight + mSkipRows;
    const auto totalFullRows = CheckedUint32(mDepth - 1) * mImageHeight + mHeight - 1;
    const auto tailOffsetRows = totalSkipRows + totalFullRows;

    const auto bytesPerRow = CheckedUint32(mRowLength) * bytesPerPixel;
    const auto rowStride = RoundUpToMultipleOf(bytesPerRow, mAlignment);
    if (!rowStride.isValid()) {
        MOZ_CRASH("Should be checked earlier.");
    }
    const auto tailOffsetBytes = tailOffsetRows * rowStride;

    uploadPtr += tailOffsetBytes.value();

    //////

    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 1);   // No stride padding.
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, 0);  // No padding in general.
    gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_IMAGES, 0); // Don't skip images,
    gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_ROWS, 0);   // or rows.
                                                      // Keep skipping pixels though!

    *out_error = DoTexOrSubImage(true, gl, target, level, dui, xOffset,
                                 yOffset+mHeight-1, zOffset+mDepth-1, mWidth, 1, 1,
                                 uploadPtr);

    // Reset all our modified state.
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, webgl->mPixelStore_UnpackAlignment);
    gl->fPixelStorei(LOCAL_GL_UNPACK_IMAGE_HEIGHT, webgl->mPixelStore_UnpackImageHeight);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, webgl->mPixelStore_UnpackRowLength);
    gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_IMAGES, webgl->mPixelStore_UnpackSkipImages);
    gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_ROWS, webgl->mPixelStore_UnpackSkipRows);

    return true;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TexUnpackImage

TexUnpackImage::TexUnpackImage(const WebGLContext* webgl, TexImageTarget target,
                               uint32_t width, uint32_t height, uint32_t depth,
                               layers::Image* image, gfxAlphaType srcAlphaType)
    : TexUnpackBlob(webgl, target, image->GetSize().width, width, height, depth,
                    srcAlphaType)
    , mImage(image)
{ }

TexUnpackImage::~TexUnpackImage()
{ }

bool
TexUnpackImage::Validate(WebGLContext* webgl, const char* funcName,
                         const webgl::PackingInfo& pi)
{
    if (!ValidatePIForDOM(webgl, funcName, pi))
        return false;

    const auto fullRows = mImage->GetSize().height;
    return ValidateUnpackPixels(webgl, funcName, fullRows, 0, this);
}

bool
TexUnpackImage::TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                              WebGLTexture* tex, TexImageTarget target, GLint level,
                              const webgl::DriverUnpackInfo* dui, GLint xOffset,
                              GLint yOffset, GLint zOffset, const webgl::PackingInfo& pi,
                              GLenum* const out_error) const
{
    MOZ_ASSERT_IF(needsRespec, !isSubImage);

    WebGLContext* webgl = tex->mContext;

    gl::GLContext* gl = webgl->GL();

    if (needsRespec) {
        *out_error = DoTexOrSubImage(isSubImage, gl, target.get(), level, dui, xOffset,
                                     yOffset, zOffset, mWidth, mHeight, mDepth,
                                     nullptr);
        if (*out_error)
            return true;
    }

    const char* fallbackReason;
    do {
        if (mDepth != 1) {
            fallbackReason = "depth is not 1";
            break;
        }
        if (xOffset != 0 || yOffset != 0 || zOffset != 0) {
            fallbackReason = "x/y/zOffset is not 0";
            break;
        }

        if (webgl->mPixelStore_UnpackSkipPixels ||
            webgl->mPixelStore_UnpackSkipRows ||
            webgl->mPixelStore_UnpackSkipImages)
        {
            fallbackReason = "non-zero UNPACK_SKIP_* not yet supported";
            break;
        }

        const auto fnHasPremultMismatch = [&]() {
            if (mSrcAlphaType == gfxAlphaType::Opaque)
                return false;

            const bool srcIsPremult = (mSrcAlphaType == gfxAlphaType::Premult);
            const auto& dstIsPremult = webgl->mPixelStore_PremultiplyAlpha;
            if (srcIsPremult == dstIsPremult)
                return false;

            if (dstIsPremult) {
                fallbackReason = "UNPACK_PREMULTIPLY_ALPHA_WEBGL is not true";
            } else {
                fallbackReason = "UNPACK_PREMULTIPLY_ALPHA_WEBGL is not false";
            }
            return true;
        };
        if (fnHasPremultMismatch())
            break;

        if (dui->unpackFormat != LOCAL_GL_RGB && dui->unpackFormat != LOCAL_GL_RGBA) {
            fallbackReason = "`format` is not RGB or RGBA";
            break;
        }

        if (dui->unpackType != LOCAL_GL_UNSIGNED_BYTE) {
            fallbackReason = "`type` is not UNSIGNED_BYTE";
            break;
        }

        gl::ScopedFramebuffer scopedFB(gl);
        gl::ScopedBindFramebuffer bindFB(gl, scopedFB.FB());

        {
            gl::GLContext::LocalErrorScope errorScope(*gl);

            gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                      target.get(), tex->mGLName, level);

            if (errorScope.GetError()) {
                fallbackReason = "bug: failed to attach to FB for blit";
                break;
            }
        }

        const GLenum status = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
        if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
            fallbackReason = "bug: failed to confirm FB for blit";
            break;
        }

        const gfx::IntSize dstSize(mWidth, mHeight);
        const auto dstOrigin = (webgl->mPixelStore_FlipY ? gl::OriginPos::TopLeft
                                                         : gl::OriginPos::BottomLeft);
        if (!gl->BlitHelper()->BlitImageToFramebuffer(mImage, dstSize, dstOrigin)) {
            fallbackReason = "likely bug: failed to blit";
            break;
        }

        // Blitting was successful, so we're done!
        *out_error = 0;
        return true;
    } while (false);

    const nsPrintfCString perfMsg("%s: Failed to hit GPU-copy fast-path: %s (src type %u)",
                                  funcName, fallbackReason, uint32_t(mImage->GetFormat()));

    if (webgl->mPixelStore_RequireFastPath) {
        webgl->ErrorInvalidOperation("%s", perfMsg.BeginReading());
        return false;
    }

    webgl->GeneratePerfWarning("%s Falling back to CPU upload.",
                               perfMsg.BeginReading());

    const RefPtr<gfx::SourceSurface> surf = mImage->GetAsSourceSurface();

    RefPtr<gfx::DataSourceSurface> dataSurf;
    if (surf) {
        // WARNING: OSX can lose our MakeCurrent here.
        dataSurf = surf->GetDataSurface();
    }
    if (!dataSurf) {
        webgl->ErrorOutOfMemory("%s: GetAsSourceSurface or GetDataSurface failed after"
                                " blit failed for TexUnpackImage.",
                                funcName);
        return false;
    }

    const TexUnpackSurface surfBlob(webgl, target, mWidth, mHeight, mDepth, dataSurf,
                                    mSrcAlphaType);

    return surfBlob.TexOrSubImage(isSubImage, needsRespec, funcName, tex, target, level,
                                  dui, xOffset, yOffset, zOffset, pi, out_error);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TexUnpackSurface

TexUnpackSurface::TexUnpackSurface(const WebGLContext* webgl, TexImageTarget target,
                                   uint32_t width, uint32_t height, uint32_t depth,
                                   gfx::DataSourceSurface* surf,
                                   gfxAlphaType srcAlphaType)
    : TexUnpackBlob(webgl, target, surf->GetSize().width, width, height, depth,
                    srcAlphaType)
    , mSurf(surf)
{ }

//////////

static bool
GetFormatForSurf(gfx::SourceSurface* surf, WebGLTexelFormat* const out_texelFormat,
                 uint8_t* const out_bpp)
{
    const auto surfFormat = surf->GetFormat();
    switch (surfFormat) {
    case gfx::SurfaceFormat::B8G8R8A8:
        *out_texelFormat = WebGLTexelFormat::BGRA8;
        *out_bpp = 4;
        return true;

    case gfx::SurfaceFormat::B8G8R8X8:
        *out_texelFormat = WebGLTexelFormat::BGRX8;
        *out_bpp = 4;
        return true;

    case gfx::SurfaceFormat::R8G8B8A8:
        *out_texelFormat = WebGLTexelFormat::RGBA8;
        *out_bpp = 4;
        return true;

    case gfx::SurfaceFormat::R8G8B8X8:
        *out_texelFormat = WebGLTexelFormat::RGBX8;
        *out_bpp = 4;
        return true;

    case gfx::SurfaceFormat::R5G6B5_UINT16:
        *out_texelFormat = WebGLTexelFormat::RGB565;
        *out_bpp = 2;
        return true;

    case gfx::SurfaceFormat::A8:
        *out_texelFormat = WebGLTexelFormat::A8;
        *out_bpp = 1;
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

//////////

bool
TexUnpackSurface::Validate(WebGLContext* webgl, const char* funcName,
                           const webgl::PackingInfo& pi)
{
    if (!ValidatePIForDOM(webgl, funcName, pi))
        return false;

    const auto fullRows = mSurf->GetSize().height;
    return ValidateUnpackPixels(webgl, funcName, fullRows, 0, this);
}

bool
TexUnpackSurface::TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                                WebGLTexture* tex, TexImageTarget target, GLint level,
                                const webgl::DriverUnpackInfo* dui, GLint xOffset,
                                GLint yOffset, GLint zOffset, const webgl::PackingInfo& dstPI,
                                GLenum* const out_error) const
{
    const auto& webgl = tex->mContext;

    ////

    const auto rowLength = mSurf->GetSize().width;
    const auto rowCount = mSurf->GetSize().height;

    const auto& dstBPP = webgl::BytesPerPixel(dstPI);
    const auto dstFormat = FormatForPackingInfo(dstPI);

    ////

    WebGLTexelFormat srcFormat;
    uint8_t srcBPP;
    if (!GetFormatForSurf(mSurf, &srcFormat, &srcBPP)) {
        webgl->ErrorImplementationBug("%s: GetFormatForSurf failed for"
                                      " WebGLTexelFormat::%u.",
                                      funcName, uint32_t(mSurf->GetFormat()));
        return false;
    }

    gfx::DataSourceSurface::ScopedMap map(mSurf, gfx::DataSourceSurface::MapType::READ);
    if (!map.IsMapped()) {
        webgl->ErrorOutOfMemory("%s: Failed to map source surface for upload.", funcName);
        return false;
    }

    const auto& srcBegin = map.GetData();
    const auto& srcStride = map.GetStride();

    ////

    const auto srcRowLengthBytes = rowLength * srcBPP;

    const uint8_t maxGLAlignment = 8;
    uint8_t srcAlignment = 1;
    for (; srcAlignment <= maxGLAlignment; srcAlignment *= 2) {
        const auto strideGuess = RoundUpToMultipleOf(srcRowLengthBytes, srcAlignment);
        if (strideGuess == srcStride)
            break;
    }
    const uint32_t dstAlignment = (srcAlignment > maxGLAlignment) ? 1 : srcAlignment;

    const auto dstRowLengthBytes = rowLength * dstBPP;
    const auto dstStride = RoundUpToMultipleOf(dstRowLengthBytes, dstAlignment);

    ////

    const uint8_t* dstBegin = srcBegin;
    UniqueBuffer tempBuffer;
    if (!ConvertIfNeeded(webgl, funcName, rowLength, rowCount, srcFormat, srcBegin,
                         srcStride, dstFormat, dstStride, &dstBegin, &tempBuffer))
    {
        return false;
    }

    ////

    const auto& gl = webgl->gl;
    if (!gl->MakeCurrent()) {
        *out_error = LOCAL_GL_CONTEXT_LOST;
        return true;
    }

    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, dstAlignment);
    if (webgl->IsWebGL2()) {
        gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, rowLength);
    }

    *out_error = DoTexOrSubImage(isSubImage, gl, target.get(), level, dui, xOffset,
                                 yOffset, zOffset, mWidth, mHeight, mDepth, dstBegin);

    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, webgl->mPixelStore_UnpackAlignment);
    if (webgl->IsWebGL2()) {
        gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, webgl->mPixelStore_UnpackRowLength);
    }

    return true;
}

} // namespace webgl
} // namespace mozilla
