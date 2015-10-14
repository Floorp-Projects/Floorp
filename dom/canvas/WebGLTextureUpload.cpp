/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTexture.h"

#include <algorithm>
#include "CanvasUtils.h"
#include "GLBlitHelper.h"
#include "GLContext.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Scoped.h"
#include "ScopedGLHelpers.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLTexelConversions.h"

namespace mozilla {

bool
DoesTargetMatchDimensions(WebGLContext* webgl, TexImageTarget target, uint8_t funcDims,
                          const char* funcName)
{
    uint8_t targetDims;
    switch (target.get()) {
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
        targetDims = 3;
        break;

    default:
        MOZ_CRASH("Unhandled texImageTarget.");
    }

    if (targetDims != funcDims) {
        webgl->ErrorInvalidEnum("%s: `target` must match function dimensions.", funcName);
        return false;
    }

    return true;
}

void
WebGLTexture::CompressedTexImage2D(TexImageTarget texImageTarget,
                                   GLint level,
                                   GLenum internalFormat,
                                   GLsizei width, GLsizei height, GLint border,
                                   const dom::ArrayBufferViewOrSharedArrayBufferView& view)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::CompTexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    const char funcName[] = "compressedTexImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    if (!mContext->ValidateTexImage(texImageTarget.get(), level, internalFormat,
                          0, 0, 0, width, height, 0,
                          border, LOCAL_GL_NONE,
                          LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    size_t byteLength;
    void* data;
    js::Scalar::Type dataType;
    ComputeLengthAndData(view, &data, &byteLength, &dataType);

    if (!mContext->ValidateCompTexImageDataSize(level, internalFormat, width, height, byteLength, func, dims)) {
        return;
    }

    if (!mContext->ValidateCompTexImageSize(level, internalFormat, 0, 0, width, height, width, height, func, dims))
    {
        return;
    }

    if (mImmutable) {
        return mContext->ErrorInvalidOperation(
            "compressedTexImage2D: disallowed because the texture bound to "
            "this target has already been made immutable by texStorage2D");
    }

    mContext->MakeContextCurrent();
    gl::GLContext* gl = mContext->gl;
    gl->fCompressedTexImage2D(texImageTarget.get(), level, internalFormat, width, height, border, byteLength, data);

    SetImageInfo(texImageTarget, level, width, height, 1, internalFormat,
                      WebGLImageDataStatus::InitializedImageData);
}

void
WebGLTexture::CompressedTexSubImage2D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                                      GLint yOffset, GLsizei width, GLsizei height,
                                      GLenum internalFormat,
                                      const dom::ArrayBufferViewOrSharedArrayBufferView& view)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::CompTexSubImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    const char funcName[] = "compressedTexSubImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    if (!mContext->ValidateTexImage(texImageTarget.get(),
                          level, internalFormat,
                          xOffset, yOffset, 0,
                          width, height, 0,
                          0, LOCAL_GL_NONE, LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    WebGLTexture::ImageInfo& levelInfo = ImageInfoAt(texImageTarget, level);

    if (internalFormat != levelInfo.EffectiveInternalFormat()) {
        return mContext->ErrorInvalidOperation("compressedTexImage2D: internalFormat does not match the existing image");
    }

    size_t byteLength;
    void* data;
    js::Scalar::Type dataType;
    ComputeLengthAndData(view, &data, &byteLength, &dataType);

    if (!mContext->ValidateCompTexImageDataSize(level, internalFormat, width, height, byteLength, func, dims))
        return;

    if (!mContext->ValidateCompTexImageSize(level, internalFormat,
                                  xOffset, yOffset,
                                  width, height,
                                  levelInfo.Width(), levelInfo.Height(),
                                  func, dims))
    {
        return;
    }

    if (levelInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                width == levelInfo.Width() &&
                                height == levelInfo.Height();
        if (coversWholeImage) {
            SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }

    mContext->MakeContextCurrent();
    gl::GLContext* gl = mContext->gl;
    gl->fCompressedTexSubImage2D(texImageTarget.get(), level, xOffset, yOffset, width, height, internalFormat, byteLength, data);
}

void
WebGLTexture::CopyTexSubImage2D_base(TexImageTarget texImageTarget, GLint level,
                                     TexInternalFormat internalFormat,
                                     GLint xOffset, GLint yOffset, GLint x,
                                     GLint y, GLsizei width, GLsizei height,
                                     bool sub)
{
    const WebGLRectangleObject* framebufferRect = mContext->CurValidReadFBRectObject();
    GLsizei framebufferWidth = framebufferRect ? framebufferRect->Width() : 0;
    GLsizei framebufferHeight = framebufferRect ? framebufferRect->Height() : 0;

    WebGLTexImageFunc func = sub
                             ? WebGLTexImageFunc::CopyTexSubImage
                             : WebGLTexImageFunc::CopyTexImage;
    WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;
    const char* info = InfoFrom(func, dims);

    // TODO: This changes with color_buffer_float. Reassess when the
    // patch lands.
    if (!mContext->ValidateTexImage(texImageTarget, level, internalFormat.get(),
                          xOffset, yOffset, 0,
                          width, height, 0,
                          0,
                          LOCAL_GL_NONE, LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    if (!mContext->ValidateCopyTexImage(internalFormat.get(), func, dims))
        return;

    if (!mContext->mBoundReadFramebuffer)
        mContext->ClearBackbufferIfNeeded();

    mContext->MakeContextCurrent();
    gl::GLContext* gl = mContext->gl;

    if (mImmutable) {
        if (!sub) {
            return mContext->ErrorInvalidOperation("copyTexImage2D: disallowed because the texture bound to this target has already been made immutable by texStorage2D");
        }
    }

    TexType framebuffertype = LOCAL_GL_NONE;
    if (mContext->mBoundReadFramebuffer) {
        TexInternalFormat framebuffereffectiveformat = mContext->mBoundReadFramebuffer->ColorAttachment(0).EffectiveInternalFormat();
        framebuffertype = TypeFromInternalFormat(framebuffereffectiveformat);
    } else {
        // FIXME - here we're assuming that the default framebuffer is backed by UNSIGNED_BYTE
        // that might not always be true, say if we had a 16bpp default framebuffer.
        framebuffertype = LOCAL_GL_UNSIGNED_BYTE;
    }

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromUnsizedInternalFormatAndType(internalFormat, framebuffertype);

    // this should never fail, validation happened earlier.
    MOZ_ASSERT(effectiveInternalFormat != LOCAL_GL_NONE);

    const bool widthOrHeightIsZero = (width == 0 || height == 0);
    if (gl->WorkAroundDriverBugs() &&
        sub && widthOrHeightIsZero)
    {
        // NV driver on Linux complains that CopyTexSubImage2D(level=0,
        // xOffset=0, yOffset=2, x=0, y=0, width=0, height=0) from a 300x150 FB
        // to a 0x2 texture. This a useless thing to do, but technically legal.
        // NV331.38 generates INVALID_VALUE.
        return mContext->DummyFramebufferOperation(info);
    }

    // check if the memory size of this texture may change with this call
    bool sizeMayChange = !sub;
    if (!sub && HasImageInfoAt(texImageTarget, level)) {
        const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
        sizeMayChange = width != imageInfo.Width() ||
                        height != imageInfo.Height() ||
                        effectiveInternalFormat != imageInfo.EffectiveInternalFormat();
    }

    if (sizeMayChange)
        mContext->GetAndFlushUnderlyingGLErrors();

    if (CanvasUtils::CheckSaneSubrectSize(x, y, width, height, framebufferWidth, framebufferHeight)) {
        if (sub)
            gl->fCopyTexSubImage2D(texImageTarget.get(), level, xOffset, yOffset, x, y, width, height);
        else
            gl->fCopyTexImage2D(texImageTarget.get(), level, internalFormat.get(), x, y, width, height, 0);
    } else {

        // the rect doesn't fit in the framebuffer

        // first, we initialize the texture as black
        if (!sub) {
            SetImageInfo(texImageTarget, level, width, height, 1,
                      effectiveInternalFormat,
                      WebGLImageDataStatus::UninitializedImageData);
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }

        // if we are completely outside of the framebuffer, we can exit now with our black texture
        if (   x >= framebufferWidth
            || x+width <= 0
            || y >= framebufferHeight
            || y+height <= 0)
        {
            // we are completely outside of range, can exit now with buffer filled with zeros
            return mContext->DummyFramebufferOperation(info);
        }

        GLint   actual_x             = clamped(x, 0, framebufferWidth);
        GLint   actual_x_plus_width  = clamped(x + width, 0, framebufferWidth);
        GLsizei actual_width   = actual_x_plus_width  - actual_x;
        GLint   actual_xOffset = xOffset + actual_x - x;

        GLint   actual_y             = clamped(y, 0, framebufferHeight);
        GLint   actual_y_plus_height = clamped(y + height, 0, framebufferHeight);
        GLsizei actual_height  = actual_y_plus_height - actual_y;
        GLint   actual_yOffset = yOffset + actual_y - y;

        gl->fCopyTexSubImage2D(texImageTarget.get(), level, actual_xOffset, actual_yOffset, actual_x, actual_y, actual_width, actual_height);
    }

    if (sizeMayChange) {
        GLenum error = mContext->GetAndFlushUnderlyingGLErrors();
        if (error) {
            mContext->GenerateWarning("copyTexImage2D generated error %s", mContext->ErrorName(error));
            return;
        }
    }

    if (!sub) {
        SetImageInfo(texImageTarget, level, width, height, 1,
                          effectiveInternalFormat,
                          WebGLImageDataStatus::InitializedImageData);
    }
}

void
WebGLTexture::CopyTexImage2D(TexImageTarget texImageTarget,
                             GLint level,
                             GLenum internalFormat,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             GLint border)
{
    // copyTexImage2D only generates textures with type = UNSIGNED_BYTE
    const WebGLTexImageFunc func = WebGLTexImageFunc::CopyTexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    const char funcName[] = "copyTexImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    if (!mContext->ValidateTexImage(texImageTarget.get(), level, internalFormat,
                          0, 0, 0,
                          width, height, 0,
                          border, LOCAL_GL_NONE, LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    if (!mContext->ValidateCopyTexImage(internalFormat, func, dims))
        return;

    if (!mContext->mBoundReadFramebuffer)
        mContext->ClearBackbufferIfNeeded();

    CopyTexSubImage2D_base(texImageTarget, level, internalFormat, 0, 0, x, y, width, height, false);
}

void
WebGLTexture::CopyTexSubImage2D(TexImageTarget texImageTarget,
                                GLint level,
                                GLint xOffset,
                                GLint yOffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height)
{
    switch (texImageTarget.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        break;
    default:
        return mContext->ErrorInvalidEnumInfo("copyTexSubImage2D: target", texImageTarget.get());
    }

    if (level < 0)
        return mContext->ErrorInvalidValue("copyTexSubImage2D: level may not be negative");

    GLsizei maxTextureSize = mContext->MaxTextureSizeForTarget(TexImageTargetToTexTarget(texImageTarget));
    if (!(maxTextureSize >> level))
        return mContext->ErrorInvalidValue("copyTexSubImage2D: 2^level exceeds maximum texture size");

    if (width < 0 || height < 0)
        return mContext->ErrorInvalidValue("copyTexSubImage2D: width and height may not be negative");

    if (xOffset < 0 || yOffset < 0)
        return mContext->ErrorInvalidValue("copyTexSubImage2D: xOffset and yOffset may not be negative");

    if (!HasImageInfoAt(texImageTarget, level))
        return mContext->ErrorInvalidOperation("copyTexSubImage2D: no texture image previously defined for this level and face");

    const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    GLsizei texWidth = imageInfo.Width();
    GLsizei texHeight = imageInfo.Height();

    if (xOffset + width > texWidth || xOffset + width < 0)
      return mContext->ErrorInvalidValue("copyTexSubImage2D: xOffset+width is too large");

    if (yOffset + height > texHeight || yOffset + height < 0)
      return mContext->ErrorInvalidValue("copyTexSubImage2D: yOffset+height is too large");

    if (!mContext->mBoundReadFramebuffer)
        mContext->ClearBackbufferIfNeeded();

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                width == texWidth &&
                                height == texHeight;
        if (coversWholeImage) {
            SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }

    TexInternalFormat internalFormat;
    TexType type;
    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(imageInfo.EffectiveInternalFormat(),
                                             &internalFormat, &type);
    return CopyTexSubImage2D_base(texImageTarget, level, internalFormat, xOffset, yOffset, x, y, width, height, true);
}


GLenum
WebGLTexture::CheckedTexImage2D(TexImageTarget texImageTarget,
                                       GLint level,
                                       TexInternalFormat internalFormat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border,
                                       TexFormat unpackFormat,
                                       TexType unpackType,
                                       const GLvoid* data)
{
    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalFormat, unpackType);
    bool sizeMayChange = true;

    if (HasImageInfoAt(texImageTarget, level)) {
        const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
        sizeMayChange = width != imageInfo.Width() ||
                        height != imageInfo.Height() ||
                        effectiveInternalFormat != imageInfo.EffectiveInternalFormat();
    }

    gl::GLContext* gl = mContext->gl;

    // Convert to format and type required by OpenGL 'driver'.
    GLenum driverType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverFormat = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl,
                                             effectiveInternalFormat,
                                             &driverInternalFormat,
                                             &driverFormat,
                                             &driverType);

    if (sizeMayChange) {
        mContext->GetAndFlushUnderlyingGLErrors();
    }

    gl->fTexImage2D(texImageTarget.get(), level, driverInternalFormat, width, height, border, driverFormat, driverType, data);

    if (effectiveInternalFormat != driverInternalFormat)
        SetLegacyTextureSwizzle(gl, texImageTarget.get(), internalFormat.get());

    GLenum error = LOCAL_GL_NO_ERROR;
    if (sizeMayChange) {
        error = mContext->GetAndFlushUnderlyingGLErrors();
    }

    return error;
}

void
WebGLTexture::TexImage2D_base(TexImageTarget texImageTarget, GLint level,
                              GLenum internalFormat,
                              GLsizei width, GLsizei height, GLsizei srcStrideOrZero,
                              GLint border,
                              GLenum unpackFormat,
                              GLenum unpackType,
                              void* data, uint32_t byteLength,
                              js::Scalar::Type jsArrayType,
                              WebGLTexelFormat srcFormat, bool srcPremultiplied)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::TexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (unpackType == LOCAL_GL_HALF_FLOAT_OES) {
        unpackType = LOCAL_GL_HALF_FLOAT;
    }

    if (!mContext->ValidateTexImage(texImageTarget, level, internalFormat,
                          0, 0, 0,
                          width, height, 0,
                          border, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    const bool isDepthTexture = unpackFormat == LOCAL_GL_DEPTH_COMPONENT ||
                                unpackFormat == LOCAL_GL_DEPTH_STENCIL;

    if (isDepthTexture && !mContext->IsWebGL2()) {
        if (data != nullptr || level != 0)
            return mContext->ErrorInvalidOperation("texImage2D: "
                                         "with format of DEPTH_COMPONENT or DEPTH_STENCIL, "
                                         "data must be nullptr, "
                                         "level must be zero");
    }

    if (!mContext->ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalFormat, unpackType);

    if (effectiveInternalFormat == LOCAL_GL_NONE) {
        return mContext->ErrorInvalidOperation("texImage2D: bad combination of internalFormat and type");
    }

    size_t srcTexelSize = size_t(-1);
    if (srcFormat == WebGLTexelFormat::Auto) {
        // we need to find the exact sized format of the source data. Slightly abusing
        // EffectiveInternalFormatFromInternalFormatAndType for that purpose. Really, an unsized source format
        // is the same thing as an unsized internalFormat.
        TexInternalFormat effectiveSourceFormat =
            EffectiveInternalFormatFromInternalFormatAndType(unpackFormat, unpackType);
        MOZ_ASSERT(effectiveSourceFormat != LOCAL_GL_NONE); // should have validated format/type combo earlier
        const size_t srcbitsPerTexel = GetBitsPerTexel(effectiveSourceFormat);
        MOZ_ASSERT((srcbitsPerTexel % 8) == 0); // should not have compressed formats here.
        srcTexelSize = srcbitsPerTexel / 8;
    } else {
        srcTexelSize = WebGLTexelConversions::TexelBytesForFormat(srcFormat);
    }

    CheckedUint32 checked_neededByteLength =
        mContext->GetImageSize(height, width, 1, srcTexelSize, mContext->mPixelStoreUnpackAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * srcTexelSize;
    CheckedUint32 checked_alignedRowSize =
        RoundedToNextMultipleOf(checked_plainRowSize.value(), mContext->mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return mContext->ErrorInvalidOperation("texImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (byteLength && byteLength < bytesNeeded)
        return mContext->ErrorInvalidOperation("texImage2D: not enough data for operation (need %d, have %d)",
                                 bytesNeeded, byteLength);

    if (mImmutable) {
        return mContext->ErrorInvalidOperation(
            "texImage2D: disallowed because the texture "
            "bound to this target has already been made immutable by texStorage2D");
    }
    mContext->MakeContextCurrent();

    nsAutoArrayPtr<uint8_t> convertedData;
    void* pixels = nullptr;
    WebGLImageDataStatus imageInfoStatusIfSuccess = WebGLImageDataStatus::UninitializedImageData;

    WebGLTexelFormat dstFormat = GetWebGLTexelFormat(effectiveInternalFormat);
    WebGLTexelFormat actualSrcFormat = srcFormat == WebGLTexelFormat::Auto ? dstFormat : srcFormat;

    if (byteLength) {
        size_t   bitsPerTexel = GetBitsPerTexel(effectiveInternalFormat);
        MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed formats here.
        size_t   dstTexelSize = bitsPerTexel / 8;
        size_t   srcStride = srcStrideOrZero ? srcStrideOrZero : checked_alignedRowSize.value();
        size_t   dstPlainRowSize = dstTexelSize * width;
        size_t   unpackAlignment = mContext->mPixelStoreUnpackAlignment;
        size_t   dstStride = ((dstPlainRowSize + unpackAlignment-1) / unpackAlignment) * unpackAlignment;

        if (actualSrcFormat == dstFormat &&
            srcPremultiplied == mContext->mPixelStorePremultiplyAlpha &&
            srcStride == dstStride &&
            !mContext->mPixelStoreFlipY)
        {
            // no conversion, no flipping, so we avoid copying anything and just pass the source pointer
            pixels = data;
        }
        else
        {
            size_t convertedDataSize = height * dstStride;
            convertedData = new (fallible) uint8_t[convertedDataSize];
            if (!convertedData) {
                mContext->ErrorOutOfMemory("texImage2D: Ran out of memory when allocating"
                                 " a buffer for doing format conversion.");
                return;
            }
            if (!mContext->ConvertImage(width, height, srcStride, dstStride,
                              static_cast<uint8_t*>(data), convertedData,
                              actualSrcFormat, srcPremultiplied,
                              dstFormat, mContext->mPixelStorePremultiplyAlpha, dstTexelSize))
            {
                return mContext->ErrorInvalidOperation("texImage2D: Unsupported texture format conversion");
            }
            pixels = reinterpret_cast<void*>(convertedData.get());
        }
        imageInfoStatusIfSuccess = WebGLImageDataStatus::InitializedImageData;
    }

    GLenum error = CheckedTexImage2D(texImageTarget, level, internalFormat, width,
                                     height, border, unpackFormat, unpackType, pixels);

    if (error) {
        mContext->GenerateWarning("texImage2D generated error %s", mContext->ErrorName(error));
        return;
    }

    // in all of the code paths above, we should have either initialized data,
    // or allocated data and left it uninitialized, but in any case we shouldn't
    // have NoImageData at this point.
    MOZ_ASSERT(imageInfoStatusIfSuccess != WebGLImageDataStatus::NoImageData);

    SetImageInfo(texImageTarget, level, width, height, 1,
                      effectiveInternalFormat, imageInfoStatusIfSuccess);
}

void
WebGLTexture::TexImage2D(TexImageTarget texImageTarget, GLint level,
                         GLenum internalFormat, GLsizei width,
                         GLsizei height, GLint border, GLenum unpackFormat,
                         GLenum unpackType, const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                         ErrorResult* const out_rv)
{
    void* data;
    size_t length;
    js::Scalar::Type jsArrayType;
    if (maybeView.IsNull()) {
        data = nullptr;
        length = 0;
        jsArrayType = js::Scalar::MaxTypedArrayViewType;
    } else {
        const auto& view = maybeView.Value();
        ComputeLengthAndData(view, &data, &length, &jsArrayType);
    }

    const char funcName[] = "texImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    return TexImage2D_base(texImageTarget, level, internalFormat, width, height, 0, border, unpackFormat, unpackType,
                           data, length, jsArrayType,
                           WebGLTexelFormat::Auto, false);
}

void
WebGLTexture::TexImage2D(TexImageTarget texImageTarget, GLint level,
                         GLenum internalFormat, GLenum unpackFormat,
                         GLenum unpackType, dom::ImageData* imageData, ErrorResult* const out_rv)
{
    if (!imageData) {
        // Spec says to generate an INVALID_VALUE error
        return mContext->ErrorInvalidValue("texImage2D: null ImageData");
    }

    dom::Uint8ClampedArray arr;
    DebugOnly<bool> inited = arr.Init(imageData->GetDataObject());
    MOZ_ASSERT(inited);
    arr.ComputeLengthAndData();

    void* pixelData = arr.Data();
    const uint32_t pixelDataLength = arr.Length();

    const char funcName[] = "texImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    return TexImage2D_base(texImageTarget, level, internalFormat, imageData->Width(),
                           imageData->Height(), 4*imageData->Width(), 0,
                           unpackFormat, unpackType, pixelData, pixelDataLength, js::Scalar::MaxTypedArrayViewType,
                           WebGLTexelFormat::RGBA8, false);
}

void
WebGLTexture::TexImage2D(TexImageTarget texImageTarget, GLint level,
                GLenum internalFormat, GLenum unpackFormat, GLenum unpackType,
                dom::Element* elem, ErrorResult* out_rv)
{
    const char funcName[] = "texImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    if (level < 0)
        return mContext->ErrorInvalidValue("texImage2D: level is negative");

    const int32_t maxLevel = mContext->MaxTextureLevelForTexImageTarget(texImageTarget);
    if (level > maxLevel) {
        mContext->ErrorInvalidValue("texImage2D: level %d is too large, max is %d",
                          level, maxLevel);
        return;
    }

    // Trying to handle the video by GPU directly first
    if (TexImageFromVideoElement(texImageTarget, level, internalFormat,
                                 unpackFormat, unpackType, elem))
    {
        return;
    }

    RefPtr<gfx::DataSourceSurface> data;
    WebGLTexelFormat srcFormat;
    nsLayoutUtils::SurfaceFromElementResult res = mContext->SurfaceFromElement(elem);
    *out_rv = mContext->SurfaceFromElementResultToImageSurface(res, data, &srcFormat);
    if (out_rv->Failed() || !data)
        return;

    gfx::IntSize size = data->GetSize();
    uint32_t byteLength = data->Stride() * size.height;
    return TexImage2D_base(texImageTarget, level, internalFormat,
                           size.width, size.height, data->Stride(), 0,
                           unpackFormat, unpackType, data->GetData(), byteLength,
                           js::Scalar::MaxTypedArrayViewType, srcFormat,
                           res.mIsPremultiplied);
}


void
WebGLTexture::TexSubImage2D_base(TexImageTarget texImageTarget, GLint level,
                                 GLint xOffset, GLint yOffset,
                                 GLsizei width, GLsizei height, GLsizei srcStrideOrZero,
                                 GLenum unpackFormat, GLenum unpackType,
                                 void* data, uint32_t byteLength,
                                 js::Scalar::Type jsArrayType,
                                 WebGLTexelFormat srcFormat, bool srcPremultiplied)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::TexSubImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (unpackType == LOCAL_GL_HALF_FLOAT_OES)
        unpackType = LOCAL_GL_HALF_FLOAT;

    const char funcName[] = "texSubImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    if (!HasImageInfoAt(texImageTarget, level))
        return mContext->ErrorInvalidOperation("texSubImage2D: no previously defined texture image");

    const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    const TexInternalFormat existingEffectiveInternalFormat = imageInfo.EffectiveInternalFormat();

    if (!mContext->ValidateTexImage(texImageTarget, level,
                          existingEffectiveInternalFormat.get(),
                          xOffset, yOffset, 0,
                          width, height, 0,
                          0, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (!mContext->ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    if (unpackType != TypeFromInternalFormat(existingEffectiveInternalFormat)) {
        return mContext->ErrorInvalidOperation("texSubImage2D: type differs from that of the existing image");
    }

    size_t srcTexelSize = size_t(-1);
    if (srcFormat == WebGLTexelFormat::Auto) {
        const size_t bitsPerTexel = GetBitsPerTexel(existingEffectiveInternalFormat);
        MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed formats here.
        srcTexelSize = bitsPerTexel / 8;
    } else {
        srcTexelSize = WebGLTexelConversions::TexelBytesForFormat(srcFormat);
    }

    if (width == 0 || height == 0)
        return; // ES 2.0 says it has no effect, we better return right now

    CheckedUint32 checked_neededByteLength =
        mContext->GetImageSize(height, width, 1, srcTexelSize, mContext->mPixelStoreUnpackAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * srcTexelSize;

    CheckedUint32 checked_alignedRowSize =
        RoundedToNextMultipleOf(checked_plainRowSize.value(), mContext->mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return mContext->ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (byteLength < bytesNeeded)
        return mContext->ErrorInvalidOperation("texSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, byteLength);

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                width == imageInfo.Width() &&
                                height == imageInfo.Height();
        if (coversWholeImage) {
            SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }
    mContext->MakeContextCurrent();
    gl::GLContext* gl = mContext->gl;

    size_t   srcStride = srcStrideOrZero ? srcStrideOrZero : checked_alignedRowSize.value();
    uint32_t dstTexelSize = GetBitsPerTexel(existingEffectiveInternalFormat) / 8;
    size_t   dstPlainRowSize = dstTexelSize * width;
    // There are checks above to ensure that this won't overflow.
    size_t   dstStride = RoundedToNextMultipleOf(dstPlainRowSize, mContext->mPixelStoreUnpackAlignment).value();

    void* pixels = data;
    nsAutoArrayPtr<uint8_t> convertedData;

    WebGLTexelFormat dstFormat = GetWebGLTexelFormat(existingEffectiveInternalFormat);
    WebGLTexelFormat actualSrcFormat = srcFormat == WebGLTexelFormat::Auto ? dstFormat : srcFormat;

    // no conversion, no flipping, so we avoid copying anything and just pass the source pointer
    bool noConversion = (actualSrcFormat == dstFormat &&
                         srcPremultiplied == mContext->mPixelStorePremultiplyAlpha &&
                         srcStride == dstStride &&
                         !mContext->mPixelStoreFlipY);

    if (!noConversion) {
        size_t convertedDataSize = height * dstStride;
        convertedData = new (fallible) uint8_t[convertedDataSize];
        if (!convertedData) {
            mContext->ErrorOutOfMemory("texImage2D: Ran out of memory when allocating"
                             " a buffer for doing format conversion.");
            return;
        }
        if (!mContext->ConvertImage(width, height, srcStride, dstStride,
                          static_cast<const uint8_t*>(data), convertedData,
                          actualSrcFormat, srcPremultiplied,
                          dstFormat, mContext->mPixelStorePremultiplyAlpha, dstTexelSize))
        {
            return mContext->ErrorInvalidOperation("texSubImage2D: Unsupported texture format conversion");
        }
        pixels = reinterpret_cast<void*>(convertedData.get());
    }

    GLenum driverType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverFormat = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl,
                                             existingEffectiveInternalFormat,
                                             &driverInternalFormat,
                                             &driverFormat,
                                             &driverType);

    gl->fTexSubImage2D(texImageTarget.get(), level, xOffset, yOffset, width, height, driverFormat, driverType, pixels);
}

void
WebGLTexture::TexSubImage2D(TexImageTarget texImageTarget, GLint level,
                            GLint xOffset, GLint yOffset,
                            GLsizei width, GLsizei height,
                            GLenum unpackFormat, GLenum unpackType,
                            const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                            ErrorResult* const out_rv)
{
    if (maybeView.IsNull())
        return mContext->ErrorInvalidValue("texSubImage2D: pixels must not be null!");

    const auto& view = maybeView.Value();
    size_t length;
    void* data;
    js::Scalar::Type jsArrayType;
    ComputeLengthAndData(view, &data, &length, &jsArrayType);

    const char funcName[] = "texSubImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    return TexSubImage2D_base(texImageTarget, level, xOffset, yOffset,
                              width, height, 0, unpackFormat, unpackType,
                              data, length, jsArrayType,
                              WebGLTexelFormat::Auto, false);
}

void
WebGLTexture::TexSubImage2D(TexImageTarget texImageTarget, GLint level,
                            GLint xOffset, GLint yOffset,
                            GLenum unpackFormat, GLenum unpackType, dom::ImageData* imageData,
                            ErrorResult* const out_rv)
{
    if (!imageData)
        return mContext->ErrorInvalidValue("texSubImage2D: pixels must not be null!");

    dom::Uint8ClampedArray arr;
    DebugOnly<bool> inited = arr.Init(imageData->GetDataObject());
    MOZ_ASSERT(inited);
    arr.ComputeLengthAndData();

    return TexSubImage2D_base(texImageTarget, level, xOffset, yOffset,
                              imageData->Width(), imageData->Height(),
                              4*imageData->Width(), unpackFormat, unpackType,
                              arr.Data(), arr.Length(),
                              js::Scalar::MaxTypedArrayViewType,
                              WebGLTexelFormat::RGBA8, false);
}



bool
WebGLTexture::TexImageFromVideoElement(TexImageTarget texImageTarget,
                                       GLint level, GLenum internalFormat,
                                       GLenum unpackFormat, GLenum unpackType,
                                       mozilla::dom::Element* elem)
{
    if (unpackType == LOCAL_GL_HALF_FLOAT_OES &&
        !mContext->gl->IsExtensionSupported(gl::GLContext::OES_texture_half_float))
    {
        unpackType = LOCAL_GL_HALF_FLOAT;
    }

    if (!mContext->ValidateTexImageFormatAndType(unpackFormat, unpackType,
                                       WebGLTexImageFunc::TexImage,
                                       WebGLTexDimensions::Tex2D))
    {
        return false;
    }

    dom::HTMLVideoElement* video = dom::HTMLVideoElement::FromContentOrNull(elem);
    if (!video)
        return false;

    uint16_t readyState;
    if (NS_SUCCEEDED(video->GetReadyState(&readyState)) &&
        readyState < nsIDOMHTMLMediaElement::HAVE_CURRENT_DATA)
    {
        //No frame inside, just return
        return false;
    }

    // If it doesn't have a principal, just bail
    nsCOMPtr<nsIPrincipal> principal = video->GetCurrentPrincipal();
    if (!principal)
        return false;

    mozilla::layers::ImageContainer* container = video->GetImageContainer();
    if (!container)
        return false;

    if (video->GetCORSMode() == CORS_NONE) {
        bool subsumes;
        nsresult rv = mContext->mCanvasElement->NodePrincipal()->Subsumes(principal, &subsumes);
        if (NS_FAILED(rv) || !subsumes) {
            mContext->GenerateWarning("It is forbidden to load a WebGL texture from a cross-domain element that has not been validated with CORS. "
                                "See https://developer.mozilla.org/en/WebGL/Cross-Domain_Textures");
            return false;
        }
    }

    layers::AutoLockImage lockedImage(container);
    layers::Image* srcImage = lockedImage.GetImage();
    if (!srcImage) {
      return false;
    }

    gl::GLContext* gl = mContext->gl;
    gl->MakeCurrent();

    const WebGLTexture::ImageInfo& info = ImageInfoAt(texImageTarget, 0);
    bool dimensionsMatch = info.Width() == srcImage->GetSize().width &&
                           info.Height() == srcImage->GetSize().height;
    if (!dimensionsMatch) {
        // we need to allocation
        gl->fTexImage2D(texImageTarget.get(), level, internalFormat,
                        srcImage->GetSize().width, srcImage->GetSize().height,
                        0, unpackFormat, unpackType, nullptr);
    }

    const gl::OriginPos destOrigin = mContext->mPixelStoreFlipY ? gl::OriginPos::BottomLeft
                                                      : gl::OriginPos::TopLeft;
    bool ok = gl->BlitHelper()->BlitImageToTexture(srcImage,
                                                   srcImage->GetSize(),
                                                   mGLName,
                                                   texImageTarget.get(),
                                                   destOrigin);
    if (ok) {
        TexInternalFormat effectiveInternalFormat =
            EffectiveInternalFormatFromInternalFormatAndType(internalFormat,
                                                             unpackType);
        MOZ_ASSERT(effectiveInternalFormat != LOCAL_GL_NONE);
        SetImageInfo(texImageTarget, level, srcImage->GetSize().width,
                          srcImage->GetSize().height, 1,
                          effectiveInternalFormat,
                          WebGLImageDataStatus::InitializedImageData);
        Bind(TexImageTargetToTexTarget(texImageTarget));
    }
    return ok;
}

void
WebGLTexture::TexSubImage2D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                            GLint yOffset, GLenum unpackFormat, GLenum unpackType,
                            dom::Element* elem, ErrorResult* const out_rv)
{
    const char funcName[] = "texSubImage2D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 2, funcName))
        return;

    if (level < 0)
        return mContext->ErrorInvalidValue("texSubImage2D: level is negative");

    const int32_t maxLevel = mContext->MaxTextureLevelForTexImageTarget(texImageTarget);
    if (level > maxLevel) {
        mContext->ErrorInvalidValue("texSubImage2D: level %d is too large, max is %d",
                          level, maxLevel);
        return;
    }

    const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    const TexInternalFormat internalFormat = imageInfo.EffectiveInternalFormat();

    // Trying to handle the video by GPU directly first
    if (TexImageFromVideoElement(texImageTarget, level, internalFormat.get(), unpackFormat, unpackType, elem))
    {
        return;
    }

    RefPtr<gfx::DataSourceSurface> data;
    WebGLTexelFormat srcFormat;
    nsLayoutUtils::SurfaceFromElementResult res = mContext->SurfaceFromElement(elem);
    *out_rv = mContext->SurfaceFromElementResultToImageSurface(res, data, &srcFormat);
    if (out_rv->Failed() || !data)
        return;

    gfx::IntSize size = data->GetSize();
    uint32_t byteLength = data->Stride() * size.height;
    TexSubImage2D_base(texImageTarget, level, xOffset, yOffset, size.width,
                       size.height, data->Stride(), unpackFormat, unpackType, data->GetData(),
                       byteLength, js::Scalar::MaxTypedArrayViewType, srcFormat,
                       res.mIsPremultiplied);
}

bool
WebGLTexture::ValidateSizedInternalFormat(GLenum internalFormat, const char* info)
{
    switch (internalFormat) {
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

    if (IsCompressedTextureFormat(internalFormat))
        return true;

    nsCString name;
    mContext->EnumName(internalFormat, &name);
    mContext->ErrorInvalidEnum("%s: invalid internal format %s", info, name.get());

    return false;
}


/** Validates parameters to texStorage{2D,3D} */
bool
WebGLTexture::ValidateTexStorage(TexTarget texTarget, GLsizei levels, GLenum internalFormat,
                                      GLsizei width, GLsizei height, GLsizei depth,
                                      const char* info)
{
    // GL_INVALID_OPERATION is generated if the texture object currently bound to target already has
    // GL_TEXTURE_IMMUTABLE_FORMAT set to GL_TRUE.
    if (mImmutable) {
        mContext->ErrorInvalidOperation("%s: texture bound to target %s is already immutable",
                                        info, mContext->EnumName(texTarget.get()));
        return false;
    }

    // GL_INVALID_ENUM is generated if internalFormat is not a valid sized internal format.
    if (!ValidateSizedInternalFormat(internalFormat, info))
        return false;

    // GL_INVALID_VALUE is generated if width, height or levels are less than 1.
    if (width < 1)  { mContext->ErrorInvalidValue("%s: width is < 1", info);  return false; }
    if (height < 1) { mContext->ErrorInvalidValue("%s: height is < 1", info); return false; }
    if (depth < 1)  { mContext->ErrorInvalidValue("%s: depth is < 1", info);  return false; }
    if (levels < 1) { mContext->ErrorInvalidValue("%s: levels is < 1", info); return false; }

    // GL_INVALID_OPERATION is generated if levels is greater than floor(log2(max(width, height, depth)))+1.
    if (FloorLog2(std::max(std::max(width, height), depth)) + 1 < levels) {
        mContext->ErrorInvalidOperation("%s: too many levels for given texture dimensions", info);
        return false;
    }

    return true;
}

void
WebGLTexture::TexStorage2D(TexTarget texTarget, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height)
{
    // GL_INVALID_ENUM is generated if target is not one of the accepted target enumerants.
    if (texTarget != LOCAL_GL_TEXTURE_2D && texTarget != LOCAL_GL_TEXTURE_CUBE_MAP)
        return mContext->ErrorInvalidEnum("texStorage2D: target is not TEXTURE_2D or TEXTURE_CUBE_MAP");

    if (!ValidateTexStorage(texTarget, levels, internalFormat, width, height, 1, "texStorage2D"))
        return;

    gl::GLContext* gl = mContext->gl;
    gl->MakeCurrent();

    mContext->GetAndFlushUnderlyingGLErrors();
    gl->fTexStorage2D(texTarget.get(), levels, internalFormat, width, height);
    GLenum error = mContext->GetAndFlushUnderlyingGLErrors();
    if (error) {
        return mContext->GenerateWarning("texStorage2D generated error %s", mContext->ErrorName(error));
    }

    SetImmutable();

    const size_t facesCount = (texTarget == LOCAL_GL_TEXTURE_2D) ? 1 : 6;
    GLsizei w = width;
    GLsizei h = height;
    for (size_t l = 0; l < size_t(levels); l++) {
        for (size_t f = 0; f < facesCount; f++) {
            SetImageInfo(TexImageTargetForTargetAndFace(texTarget, f),
                              l, w, h, 1,
                              internalFormat,
                              WebGLImageDataStatus::UninitializedImageData);
        }
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }
}

void
WebGLTexture::TexStorage3D(TexTarget texTarget, GLsizei levels, GLenum internalFormat,
                            GLsizei width, GLsizei height, GLsizei depth)
{
    // GL_INVALID_ENUM is generated if target is not one of the accepted target enumerants.
    if (texTarget != LOCAL_GL_TEXTURE_3D)
        return mContext->ErrorInvalidEnum("texStorage3D: target is not TEXTURE_3D");

    if (!ValidateTexStorage(texTarget, levels, internalFormat, width, height, depth, "texStorage3D"))
        return;

    gl::GLContext* gl = mContext->gl;
    gl->MakeCurrent();

    mContext->GetAndFlushUnderlyingGLErrors();
    gl->fTexStorage3D(texTarget.get(), levels, internalFormat, width, height, depth);
    GLenum error = mContext->GetAndFlushUnderlyingGLErrors();
    if (error) {
        return mContext->GenerateWarning("texStorage3D generated error %s", mContext->ErrorName(error));
    }

    SetImmutable();

    GLsizei w = width;
    GLsizei h = height;
    GLsizei d = depth;
    for (size_t l = 0; l < size_t(levels); l++) {
        SetImageInfo(TexImageTargetForTargetAndFace(texTarget, 0),
                          l, w, h, d,
                          internalFormat,
                          WebGLImageDataStatus::UninitializedImageData);
        w = std::max(1, w >> 1);
        h = std::max(1, h >> 1);
        d = std::max(1, d >> 1);
    }
}

void
WebGLTexture::TexImage3D(TexImageTarget texImageTarget, GLint level, GLenum internalFormat,
                          GLsizei width, GLsizei height, GLsizei depth,
                          GLint border, GLenum unpackFormat, GLenum unpackType,
                          const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                          ErrorResult* const out_rv)
{
    void* data;
    size_t dataLength;
    js::Scalar::Type jsArrayType;
    if (maybeView.IsNull()) {
        data = nullptr;
        dataLength = 0;
        jsArrayType = js::Scalar::MaxTypedArrayViewType;
    } else {
        const auto& view = maybeView.Value();
        ComputeLengthAndData(view, &data, &dataLength, &jsArrayType);
    }

    const char funcName[] = "texImage3D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 3, funcName))
        return;

    const WebGLTexImageFunc func = WebGLTexImageFunc::TexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex3D;

    if (!mContext->ValidateTexImage(texImageTarget, level, internalFormat,
                          0, 0, 0,
                          width, height, depth,
                          border, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (!mContext->ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalFormat, unpackType);

    if (effectiveInternalFormat == LOCAL_GL_NONE) {
        return mContext->ErrorInvalidOperation("texImage3D: bad combination of internalFormat and unpackType");
    }

    // we need to find the exact sized format of the source data. Slightly abusing
    // EffectiveInternalFormatFromInternalFormatAndType for that purpose. Really, an unsized source format
    // is the same thing as an unsized internalFormat.
    TexInternalFormat effectiveSourceFormat =
        EffectiveInternalFormatFromInternalFormatAndType(unpackFormat, unpackType);
    MOZ_ASSERT(effectiveSourceFormat != LOCAL_GL_NONE); // should have validated unpack format/type combo earlier
    const size_t srcbitsPerTexel = GetBitsPerTexel(effectiveSourceFormat);
    MOZ_ASSERT((srcbitsPerTexel % 8) == 0); // should not have compressed formats here.
    size_t srcTexelSize = srcbitsPerTexel / 8;

    CheckedUint32 checked_neededByteLength =
        mContext->GetImageSize(height, width, depth, srcTexelSize, mContext->mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return mContext->ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (dataLength && dataLength < bytesNeeded)
        return mContext->ErrorInvalidOperation("texImage3D: not enough data for operation (need %d, have %d)",
                                 bytesNeeded, dataLength);

    if (IsImmutable()) {
        return mContext->ErrorInvalidOperation(
            "texImage3D: disallowed because the texture "
            "bound to this target has already been made immutable by texStorage3D");
    }

    gl::GLContext* gl = mContext->gl;
    gl->MakeCurrent();

    GLenum driverUnpackType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverUnpackFormat = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl,
                                             effectiveInternalFormat,
                                             &driverInternalFormat,
                                             &driverUnpackFormat,
                                             &driverUnpackType);

    mContext->GetAndFlushUnderlyingGLErrors();
    gl->fTexImage3D(texImageTarget.get(), level,
                    driverInternalFormat,
                    width, height, depth,
                    0, driverUnpackFormat, driverUnpackType,
                    data);
    GLenum error = mContext->GetAndFlushUnderlyingGLErrors();
    if (error) {
        return mContext->GenerateWarning("texImage3D generated error %s", mContext->ErrorName(error));
    }

    SetImageInfo(texImageTarget, level,
                      width, height, depth,
                      effectiveInternalFormat,
                      data ? WebGLImageDataStatus::InitializedImageData
                           : WebGLImageDataStatus::UninitializedImageData);
}

void
WebGLTexture::TexSubImage3D(TexImageTarget texImageTarget, GLint level,
                             GLint xOffset, GLint yOffset, GLint zOffset,
                             GLsizei width, GLsizei height, GLsizei depth,
                             GLenum unpackFormat, GLenum unpackType,
                             const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                             ErrorResult* const out_rv)
{
    if (maybeView.IsNull())
        return mContext->ErrorInvalidValue("texSubImage3D: pixels must not be null!");

    const auto& view = maybeView.Value();
    void* data;
    size_t dataLength;
    js::Scalar::Type jsArrayType;
    ComputeLengthAndData(view, &data, &dataLength, &jsArrayType);

    const char funcName[] = "texSubImage3D";
    if (!DoesTargetMatchDimensions(mContext, texImageTarget, 3, funcName))
        return;

    const WebGLTexImageFunc func = WebGLTexImageFunc::TexSubImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex3D;

    if (!HasImageInfoAt(texImageTarget, level)) {
        return mContext->ErrorInvalidOperation("texSubImage3D: no previously defined texture image");
    }

    const WebGLTexture::ImageInfo& imageInfo = ImageInfoAt(texImageTarget, level);
    const TexInternalFormat existingEffectiveInternalFormat = imageInfo.EffectiveInternalFormat();
    TexInternalFormat existingUnsizedInternalFormat = LOCAL_GL_NONE;
    TexType existingType = LOCAL_GL_NONE;
    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(existingEffectiveInternalFormat,
                                                            &existingUnsizedInternalFormat,
                                                            &existingType);

    if (!mContext->ValidateTexImage(texImageTarget, level, existingEffectiveInternalFormat.get(),
                          xOffset, yOffset, zOffset,
                          width, height, depth,
                          0, unpackFormat, unpackType, func, dims))
    {
        return;
    }

    if (unpackType != existingType) {
        return mContext->ErrorInvalidOperation("texSubImage3D: type differs from that of the existing image");
    }

    if (!mContext->ValidateTexInputData(unpackType, jsArrayType, func, dims))
        return;

    const size_t bitsPerTexel = GetBitsPerTexel(existingEffectiveInternalFormat);
    MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed formats here.
    size_t srcTexelSize = bitsPerTexel / 8;

    if (width == 0 || height == 0 || depth == 0)
        return; // no effect, we better return right now

    CheckedUint32 checked_neededByteLength =
        mContext->GetImageSize(height, width, depth, srcTexelSize, mContext->mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return mContext->ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (dataLength < bytesNeeded)
        return mContext->ErrorInvalidOperation("texSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, dataLength);

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xOffset == 0 &&
                                yOffset == 0 &&
                                zOffset == 0 &&
                                width == imageInfo.Width() &&
                                height == imageInfo.Height() &&
                                depth == imageInfo.Depth();
        if (coversWholeImage) {
            SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            if (!EnsureInitializedImageData(texImageTarget, level))
                return;
        }
    }

    GLenum driverUnpackType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverUnpackFormat = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(mContext->gl,
                                             existingEffectiveInternalFormat,
                                             &driverInternalFormat,
                                             &driverUnpackFormat,
                                             &driverUnpackType);

    mContext->MakeContextCurrent();
    mContext->gl->fTexSubImage3D(texImageTarget.get(), level,
                       xOffset, yOffset, zOffset,
                       width, height, depth,
                       driverUnpackFormat, driverUnpackType, data);

}


} // namespace mozilla
