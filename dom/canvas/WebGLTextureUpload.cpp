/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTexture.h"

#include <algorithm>

#include "CanvasUtils.h"
#include "gfxPrefs.h"
#include "GLBlitHelper.h"
#include "GLContext.h"
#include "mozilla/Casting.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Scoped.h"
#include "mozilla/Unused.h"
#include "ScopedGLHelpers.h"
#include "TexUnpackBlob.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLTexelConversions.h"

namespace mozilla {

/* This file handles:
 * TexStorage2D(texTarget, levels, internalFormat, width, height)
 * TexStorage3D(texTarget, levels, intenralFormat, width, height, depth)
 *
 * TexImage2D(texImageTarget, level, internalFormat, width, height, border,
 *            unpackFormat, unpackType, data)
 * TexImage3D(texImageTarget, level, internalFormat, width, height, depth,
 *            border, unpackFormat, unpackType, data)
 * TexSubImage2D(texImageTarget, level, xOffset, yOffset, width, height,
 *               unpackFormat, unpackType, data)
 * TexSubImage3D(texImageTarget, level, xOffset, yOffset, zOffset, width,
 *               height, depth, unpackFormat, unpackType, data)
 *
 * CompressedTexImage2D(texImageTarget, level, internalFormat, width, height,
 *                      border, imageSize, data)
 * CompressedTexImage3D(texImageTarget, level, internalFormat, width, height,
 *                      depth, border, imageSize, data)
 * CompressedTexSubImage2D(texImageTarget, level, xOffset, yOffset, width,
 *                         height, sizedUnpackFormat, imageSize, data)
 * CompressedTexSubImage3D(texImageTarget, level, xOffset, yOffset, zOffset,
 *                         width, height, depth, sizedUnpackFormat, imageSize,
 *                         data)
 *
 * CopyTexImage2D(texImageTarget, level, internalFormat, x, y, width, height,
 *                border)
 * CopyTexImage3D - "Because the framebuffer is inhererntly two-dimensional,
 *                   there is no CopyTexImage3D command."
 * CopyTexSubImage2D(texImageTarget, level, xOffset, yOffset, x, y, width,
 *                   height)
 * CopyTexSubImage3D(texImageTarget, level, xOffset, yOffset, zOffset, x, y,
 *                   width, height)
 */

static bool ValidateExtents(WebGLContext* webgl, GLsizei width, GLsizei height,
                            GLsizei depth, GLint border,
                            uint32_t* const out_width,
                            uint32_t* const out_height,
                            uint32_t* const out_depth) {
  // Check border
  if (border != 0) {
    webgl->ErrorInvalidValue("`border` must be 0.");
    return false;
  }

  if (width < 0 || height < 0 || depth < 0) {
    /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
     *   "If wt and ht are the specified image width and height,
     *   and if either wt or ht are less than zero, then the error
     *   INVALID_VALUE is generated."
     */
    webgl->ErrorInvalidValue("`width`/`height`/`depth` must be >= 0.");
    return false;
  }

  *out_width = width;
  *out_height = height;
  *out_depth = depth;
  return true;
}

////////////////////////////////////////
// ArrayBufferView?

static inline bool DoesJSTypeMatchUnpackType(GLenum unpackType,
                                             js::Scalar::Type jsType) {
  switch (unpackType) {
    case LOCAL_GL_BYTE:
      return jsType == js::Scalar::Type::Int8;

    case LOCAL_GL_UNSIGNED_BYTE:
      return jsType == js::Scalar::Type::Uint8 ||
             jsType == js::Scalar::Type::Uint8Clamped;

    case LOCAL_GL_SHORT:
      return jsType == js::Scalar::Type::Int16;

    case LOCAL_GL_UNSIGNED_SHORT:
    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
      return jsType == js::Scalar::Type::Uint16;

    case LOCAL_GL_INT:
      return jsType == js::Scalar::Type::Int32;

    case LOCAL_GL_UNSIGNED_INT:
    case LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV:
    case LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV:
    case LOCAL_GL_UNSIGNED_INT_5_9_9_9_REV:
    case LOCAL_GL_UNSIGNED_INT_24_8:
      return jsType == js::Scalar::Type::Uint32;

    case LOCAL_GL_FLOAT:
      return jsType == js::Scalar::Type::Float32;

    default:
      return false;
  }
}

static bool ValidateViewType(WebGLContext* webgl, GLenum unpackType,
                             const TexImageSource& src) {
  if (!src.mView) return true;
  const auto& view = *(src.mView);

  const auto& jsType = view.Type();
  if (!DoesJSTypeMatchUnpackType(unpackType, jsType)) {
    webgl->ErrorInvalidOperation(
        "ArrayBufferView type not compatible with"
        " `type`.");
    return false;
  }

  return true;
}

static bool ValidateUnpackInfo(WebGLContext* webgl,
                               const webgl::PackingInfo& pi) {
  if (!webgl->mFormatUsage->AreUnpackEnumsValid(pi.format, pi.type)) {
    webgl->ErrorInvalidEnum("Invalid unpack format/type: 0x%04x/0x%04x",
                            pi.format, pi.type);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////

static UniquePtr<webgl::TexUnpackBytes> FromView(
    WebGLContext* webgl, TexImageTarget target, uint32_t width, uint32_t height,
    uint32_t depth, const dom::ArrayBufferView* view, GLuint viewElemOffset,
    GLuint viewElemLengthOverride, const GLenum errorVal) {
  const bool isClientData = true;
  const uint8_t* bytes = nullptr;
  size_t availByteCount = 0;
  if (view) {
    if (!webgl->ValidateArrayBufferView(
            *view, viewElemOffset, viewElemLengthOverride, errorVal,
            const_cast<uint8_t**>(&bytes), &availByteCount)) {
      return nullptr;
    }
  }
  return MakeUnique<webgl::TexUnpackBytes>(webgl, target, width, height, depth,
                                           isClientData, bytes, availByteCount);
}

static UniquePtr<webgl::TexUnpackBytes> FromPboOffset(
    WebGLContext* webgl, TexImageTarget target, uint32_t width, uint32_t height,
    uint32_t depth, WebGLsizeiptr pboOffset,
    const Maybe<GLsizei>& expectedImageSize) {
  if (pboOffset < 0) {
    webgl->ErrorInvalidValue("offset cannot be negative.");
    return nullptr;
  }

  const auto& buffer =
      webgl->ValidateBufferSelection(LOCAL_GL_PIXEL_UNPACK_BUFFER);
  if (!buffer) return nullptr;

  size_t availBufferBytes = buffer->ByteLength();
  if (size_t(pboOffset) > availBufferBytes) {
    webgl->ErrorInvalidOperation("Offset is passed end of buffer.");
    return nullptr;
  }
  availBufferBytes -= pboOffset;
  if (expectedImageSize.isSome()) {
    if (expectedImageSize.ref() < 0) {
      webgl->ErrorInvalidValue("ImageSize can't be less than 0.");
      return nullptr;
    }
    if (size_t(expectedImageSize.ref()) != availBufferBytes) {
      webgl->ErrorInvalidOperation(
          "ImageSize doesn't match the required upload byte size.");
      return nullptr;
    }
    availBufferBytes = size_t(expectedImageSize.ref());
  }
  const bool isClientData = false;
  const auto ptr = (const uint8_t*)pboOffset;
  return MakeUnique<webgl::TexUnpackBytes>(webgl, target, width, height, depth,
                                           isClientData, ptr, availBufferBytes);
}

static UniquePtr<webgl::TexUnpackBlob> FromImageBitmap(
    WebGLContext* webgl, TexImageTarget target, uint32_t width, uint32_t height,
    uint32_t depth, const dom::ImageBitmap& imageBitmap, ErrorResult* aRv) {
  if (imageBitmap.IsWriteOnly()) {
    aRv->Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  UniquePtr<dom::ImageBitmapCloneData> cloneData = imageBitmap.ToCloneData();
  if (!cloneData) {
    return nullptr;
  }

  const RefPtr<gfx::DataSourceSurface> surf = cloneData->mSurface;

  if (!width) {
    width = surf->GetSize().width;
  }

  if (!height) {
    height = surf->GetSize().height;
  }

  // WhatWG "HTML Living Standard" (30 October 2015):
  // "The getImageData(sx, sy, sw, sh) method [...] Pixels must be returned as
  // non-premultiplied alpha values."
  return MakeUnique<webgl::TexUnpackSurface>(
      webgl, target, width, height, depth, surf, cloneData->mAlphaType);
}

static UniquePtr<webgl::TexUnpackBlob> FromImageData(
    WebGLContext* webgl, TexImageTarget target, uint32_t width, uint32_t height,
    uint32_t depth, const dom::ImageData& imageData,
    dom::Uint8ClampedArray* scopedArr) {
  DebugOnly<bool> inited = scopedArr->Init(imageData.GetDataObject());
  MOZ_ASSERT(inited);

  scopedArr->ComputeLengthAndData();
  const DebugOnly<size_t> dataSize = scopedArr->Length();
  const void* const data = scopedArr->Data();

  const gfx::IntSize size(imageData.Width(), imageData.Height());
  const size_t stride = size.width * 4;
  const gfx::SurfaceFormat surfFormat = gfx::SurfaceFormat::R8G8B8A8;

  // WhatWG "HTML Living Standard" (30 October 2015):
  // "The getImageData(sx, sy, sw, sh) method [...] Pixels must be returned as
  // non-premultiplied alpha values."
  const auto alphaType = gfxAlphaType::NonPremult;

  MOZ_ASSERT(dataSize == stride * size.height);

  uint8_t* wrappableData = (uint8_t*)data;

  const RefPtr<gfx::DataSourceSurface> surf =
      gfx::Factory::CreateWrappingDataSourceSurface(wrappableData, stride, size,
                                                    surfFormat);
  if (!surf) {
    webgl->ErrorOutOfMemory("OOM in FromImageData.");
    return nullptr;
  }

  ////

  if (!width) {
    width = imageData.Width();
  }

  if (!height) {
    height = imageData.Height();
  }

  ////

  return MakeUnique<webgl::TexUnpackSurface>(webgl, target, width, height,
                                             depth, surf, alphaType);
}

UniquePtr<webgl::TexUnpackBlob> WebGLContext::FromDomElem(
    TexImageTarget target, uint32_t width, uint32_t height, uint32_t depth,
    const dom::Element& elem, ErrorResult* const out_error) {
  if (elem.IsHTMLElement(nsGkAtoms::canvas)) {
    const dom::HTMLCanvasElement* canvas =
        static_cast<const dom::HTMLCanvasElement*>(&elem);
    if (canvas->IsWriteOnly()) {
      out_error->Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }
  }

  // The canvas spec says that drawImage should draw the first frame of
  // animated images. The webgl spec doesn't mention the issue, so we do the
  // same as drawImage.
  uint32_t flags = nsLayoutUtils::SFE_WANT_FIRST_FRAME_IF_IMAGE |
                   nsLayoutUtils::SFE_WANT_IMAGE_SURFACE |
                   nsLayoutUtils::SFE_USE_ELEMENT_SIZE_IF_VECTOR;

  if (mPixelStore_ColorspaceConversion == LOCAL_GL_NONE)
    flags |= nsLayoutUtils::SFE_NO_COLORSPACE_CONVERSION;

  if (!mPixelStore_PremultiplyAlpha)
    flags |= nsLayoutUtils::SFE_PREFER_NO_PREMULTIPLY_ALPHA;

  RefPtr<gfx::DrawTarget> idealDrawTarget = nullptr;  // Don't care for now.
  auto sfer = nsLayoutUtils::SurfaceFromElement(
      const_cast<dom::Element*>(&elem), flags, idealDrawTarget);

  //////

  uint32_t elemWidth = 0;
  uint32_t elemHeight = 0;
  layers::Image* layersImage = nullptr;
  if (!gfxPrefs::WebGLDisableDOMBlitUploads() && sfer.mLayersImage) {
    layersImage = sfer.mLayersImage;
    elemWidth = layersImage->GetSize().width;
    elemHeight = layersImage->GetSize().height;
  }

  RefPtr<gfx::DataSourceSurface> dataSurf;
  if (!layersImage && sfer.GetSourceSurface()) {
    const auto surf = sfer.GetSourceSurface();
    elemWidth = surf->GetSize().width;
    elemHeight = surf->GetSize().height;

    // WARNING: OSX can lose our MakeCurrent here.
    dataSurf = surf->GetDataSurface();
  }

  //////

  if (!width) {
    width = elemWidth;
  }

  if (!height) {
    height = elemHeight;
  }

  ////

  if (!layersImage && !dataSurf) {
    const bool isClientData = true;
    return MakeUnique<webgl::TexUnpackBytes>(this, target, width, height, depth,
                                             isClientData, nullptr, 0);
  }

  //////

  // While it's counter-intuitive, the shape of the SFEResult API means that we
  // should try to pull out a surface first, and then, if we do pull out a
  // surface, check CORS/write-only/etc..

  if (!sfer.mCORSUsed) {
    auto& srcPrincipal = sfer.mPrincipal;
    nsIPrincipal* dstPrincipal = GetCanvas()->NodePrincipal();

    if (!dstPrincipal->Subsumes(srcPrincipal)) {
      GenerateWarning("Cross-origin elements require CORS.");
      out_error->Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }
  }

  if (sfer.mIsWriteOnly) {
    // mIsWriteOnly defaults to true, and so will be true even if SFE merely
    // failed. Thus we must test mIsWriteOnly after successfully retrieving an
    // Image or SourceSurface.
    GenerateWarning("Element is write-only, thus cannot be uploaded.");
    out_error->Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  //////
  // Ok, we're good!

  if (layersImage) {
    return MakeUnique<webgl::TexUnpackImage>(this, target, width, height, depth,
                                             layersImage, sfer.mAlphaType);
  }

  MOZ_ASSERT(dataSurf);
  return MakeUnique<webgl::TexUnpackSurface>(this, target, width, height, depth,
                                             dataSurf, sfer.mAlphaType);
}

////////////////////////////////////////

UniquePtr<webgl::TexUnpackBlob> WebGLContext::From(
    TexImageTarget target, GLsizei rawWidth, GLsizei rawHeight,
    GLsizei rawDepth, GLint border, const TexImageSource& src,
    dom::Uint8ClampedArray* const scopedArr) {
  uint32_t width, height, depth;
  if (!ValidateExtents(this, rawWidth, rawHeight, rawDepth, border, &width,
                       &height, &depth)) {
    return nullptr;
  }

  if (src.mPboOffset) {
    return FromPboOffset(this, target, width, height, depth, *(src.mPboOffset),
                         Nothing());
  }

  if (mBoundPixelUnpackBuffer) {
    ErrorInvalidOperation("PIXEL_UNPACK_BUFFER must be null.");
    return nullptr;
  }

  if (src.mImageBitmap) {
    return FromImageBitmap(this, target, width, height, depth,
                           *(src.mImageBitmap), src.mOut_error);
  }

  if (src.mImageData) {
    return FromImageData(this, target, width, height, depth, *(src.mImageData),
                         scopedArr);
  }

  if (src.mDomElem) {
    return FromDomElem(target, width, height, depth, *(src.mDomElem),
                       src.mOut_error);
  }

  return FromView(this, target, width, height, depth, src.mView,
                  src.mViewElemOffset, src.mViewElemLengthOverride,
                  LOCAL_GL_INVALID_OPERATION);
}

////////////////////////////////////////////////////////////////////////////////

static UniquePtr<webgl::TexUnpackBlob> ValidateTexOrSubImage(
    WebGLContext* webgl, TexImageTarget target, GLsizei rawWidth,
    GLsizei rawHeight, GLsizei rawDepth, GLint border,
    const webgl::PackingInfo& pi, const TexImageSource& src,
    dom::Uint8ClampedArray* const scopedArr) {
  if (!ValidateUnpackInfo(webgl, pi)) return nullptr;

  if (!ValidateViewType(webgl, pi.type, src)) return nullptr;

  auto blob = webgl->From(target, rawWidth, rawHeight, rawDepth, border, src,
                          scopedArr);
  if (!blob || !blob->Validate(webgl, pi)) return nullptr;

  return blob;
}

void WebGLTexture::TexImage(TexImageTarget target, GLint level,
                            GLenum internalFormat, GLsizei width,
                            GLsizei height, GLsizei depth, GLint border,
                            const webgl::PackingInfo& pi,
                            const TexImageSource& src) {
  dom::RootedSpiderMonkeyInterface<dom::Uint8ClampedArray> scopedArr(
      dom::RootingCx());
  const auto blob = ValidateTexOrSubImage(mContext, target, width, height,
                                          depth, border, pi, src, &scopedArr);
  if (!blob) return;

  TexImage(target, level, internalFormat, pi, blob.get());
}

void WebGLTexture::TexSubImage(TexImageTarget target, GLint level,
                               GLint xOffset, GLint yOffset, GLint zOffset,
                               GLsizei width, GLsizei height, GLsizei depth,
                               const webgl::PackingInfo& pi,
                               const TexImageSource& src) {
  const GLint border = 0;
  dom::RootedSpiderMonkeyInterface<dom::Uint8ClampedArray> scopedArr(
      dom::RootingCx());
  const auto blob = ValidateTexOrSubImage(mContext, target, width, height,
                                          depth, border, pi, src, &scopedArr);
  if (!blob) return;

  if (!blob->HasData()) {
    mContext->ErrorInvalidValue("Source must not be null.");
    return;
  }

  TexSubImage(target, level, xOffset, yOffset, zOffset, pi, blob.get());
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

static bool ValidateTexImage(WebGLContext* webgl, WebGLTexture* texture,
                             TexImageTarget target, GLint level,
                             webgl::ImageInfo** const out_imageInfo) {
  // Check level
  if (level < 0) {
    webgl->ErrorInvalidValue("`level` must be >= 0.");
    return false;
  }

  if (level >= WebGLTexture::kMaxLevelCount) {
    webgl->ErrorInvalidValue("`level` is too large.");
    return false;
  }

  auto& imageInfo = texture->ImageInfoAt(target, level);
  *out_imageInfo = &imageInfo;
  return true;
}

// For *TexImage*
bool WebGLTexture::ValidateTexImageSpecification(
    TexImageTarget target, GLint rawLevel, uint32_t width, uint32_t height,
    uint32_t depth, webgl::ImageInfo** const out_imageInfo) {
  if (mImmutable) {
    mContext->ErrorInvalidOperation("Specified texture is immutable.");
    return false;
  }

  // Do this early to validate `level`.
  webgl::ImageInfo* imageInfo;
  if (!ValidateTexImage(mContext, this, target, rawLevel, &imageInfo))
    return false;
  const uint32_t level(rawLevel);

  if (mTarget == LOCAL_GL_TEXTURE_CUBE_MAP && width != height) {
    mContext->ErrorInvalidValue("Cube map images must be square.");
    return false;
  }

  /* GLES 3.0.4, p133-134:
   * GL_MAX_TEXTURE_SIZE is *not* the max allowed texture size. Rather, it is
   * the max (width/height) size guaranteed not to generate an INVALID_VALUE for
   * too-large dimensions. Sizes larger than GL_MAX_TEXTURE_SIZE *may or may
   * not* result in an INVALID_VALUE, or possibly GL_OOM.
   *
   * However, we have needed to set our maximums lower in the past to prevent
   * resource corruption. Therefore we have mGLMaxTextureSize, which is neither
   * necessarily lower nor higher than MAX_TEXTURE_SIZE.
   *
   * Note that mGLMaxTextureSize must be >= than the advertized
   * MAX_TEXTURE_SIZE. For simplicity, we advertize MAX_TEXTURE_SIZE as
   * mGLMaxTextureSize.
   */

  uint32_t maxWidthHeight = 0;
  uint32_t maxDepth = 0;
  uint32_t maxLevel = 0;

  MOZ_ASSERT(level <= 31);
  switch (target.get()) {
    case LOCAL_GL_TEXTURE_2D:
      maxWidthHeight = mContext->mGLMaxTextureSize >> level;
      maxDepth = 1;
      maxLevel = CeilingLog2(mContext->mGLMaxTextureSize);
      break;

    case LOCAL_GL_TEXTURE_3D:
      maxWidthHeight = mContext->mGLMax3DTextureSize >> level;
      maxDepth = maxWidthHeight;
      maxLevel = CeilingLog2(mContext->mGLMax3DTextureSize);
      break;

    case LOCAL_GL_TEXTURE_2D_ARRAY:
      maxWidthHeight = mContext->mGLMaxTextureSize >> level;
      // "The maximum number of layers for two-dimensional array textures
      // (depth) must be at least MAX_ARRAY_TEXTURE_LAYERS for all levels."
      maxDepth = mContext->mGLMaxArrayTextureLayers;
      maxLevel = CeilingLog2(mContext->mGLMaxTextureSize);
      break;

    default:  // cube maps
      MOZ_ASSERT(IsCubeMap());
      maxWidthHeight = mContext->mGLMaxCubeMapTextureSize >> level;
      maxDepth = 1;
      maxLevel = CeilingLog2(mContext->mGLMaxCubeMapTextureSize);
      break;
  }

  if (level > maxLevel) {
    mContext->ErrorInvalidValue("Requested level is not supported for target.");
    return false;
  }

  if (width > maxWidthHeight || height > maxWidthHeight || depth > maxDepth) {
    mContext->ErrorInvalidValue("Requested size at this level is unsupported.");
    return false;
  }

  {
    /* GL ES Version 2.0.25 - 3.7.1 Texture Image Specification
     *   "If level is greater than zero, and either width or
     *   height is not a power-of-two, the error INVALID_VALUE is
     *   generated."
     *
     * This restriction does not apply to GL ES Version 3.0+.
     */
    bool requirePOT = (!mContext->IsWebGL2() && level != 0);

    if (requirePOT) {
      if (!IsPowerOfTwo(width) || !IsPowerOfTwo(height)) {
        mContext->ErrorInvalidValue(
            "For level > 0, width and height must be"
            " powers of two.");
        return false;
      }
    }
  }

  *out_imageInfo = imageInfo;
  return true;
}

// For *TexSubImage*
bool WebGLTexture::ValidateTexImageSelection(
    TexImageTarget target, GLint level, GLint xOffset, GLint yOffset,
    GLint zOffset, uint32_t width, uint32_t height, uint32_t depth,
    webgl::ImageInfo** const out_imageInfo) {
  // The conformance test wants bad arg checks before imageInfo checks.
  if (xOffset < 0 || yOffset < 0 || zOffset < 0) {
    mContext->ErrorInvalidValue("Offsets must be >=0.");
    return false;
  }

  webgl::ImageInfo* imageInfo;
  if (!ValidateTexImage(mContext, this, target, level, &imageInfo))
    return false;

  if (!imageInfo->IsDefined()) {
    mContext->ErrorInvalidOperation(
        "The specified TexImage has not yet been"
        " specified.");
    return false;
  }

  const auto totalX = CheckedUint32(xOffset) + width;
  const auto totalY = CheckedUint32(yOffset) + height;
  const auto totalZ = CheckedUint32(zOffset) + depth;

  if (!totalX.isValid() || totalX.value() > imageInfo->mWidth ||
      !totalY.isValid() || totalY.value() > imageInfo->mHeight ||
      !totalZ.isValid() || totalZ.value() > imageInfo->mDepth) {
    mContext->ErrorInvalidValue(
        "Offset+size must be <= the size of the existing"
        " specified image.");
    return false;
  }

  *out_imageInfo = imageInfo;
  return true;
}

static bool ValidateCompressedTexUnpack(WebGLContext* webgl, GLsizei width,
                                        GLsizei height, GLsizei depth,
                                        const webgl::FormatInfo* format,
                                        size_t dataSize) {
  auto compression = format->compression;

  auto bytesPerBlock = compression->bytesPerBlock;
  auto blockWidth = compression->blockWidth;
  auto blockHeight = compression->blockHeight;

  auto widthInBlocks = CheckedUint32(width) / blockWidth;
  auto heightInBlocks = CheckedUint32(height) / blockHeight;
  if (width % blockWidth) widthInBlocks += 1;
  if (height % blockHeight) heightInBlocks += 1;

  const CheckedUint32 blocksPerImage = widthInBlocks * heightInBlocks;
  const CheckedUint32 bytesPerImage = bytesPerBlock * blocksPerImage;
  const CheckedUint32 bytesNeeded = bytesPerImage * depth;

  if (!bytesNeeded.isValid()) {
    webgl->ErrorOutOfMemory("Overflow while computing the needed buffer size.");
    return false;
  }

  if (dataSize != bytesNeeded.value()) {
    webgl->ErrorInvalidValue(
        "Provided buffer's size must match expected size."
        " (needs %u, has %zu)",
        bytesNeeded.value(), dataSize);
    return false;
  }

  return true;
}

static bool DoChannelsMatchForCopyTexImage(const webgl::FormatInfo* srcFormat,
                                           const webgl::FormatInfo* dstFormat) {
  // GLES 3.0.4 p140 Table 3.16 "Valid CopyTexImage source
  // framebuffer/destination texture base internal format combinations."

  switch (srcFormat->unsizedFormat) {
    case webgl::UnsizedFormat::RGBA:
      switch (dstFormat->unsizedFormat) {
        case webgl::UnsizedFormat::A:
        case webgl::UnsizedFormat::L:
        case webgl::UnsizedFormat::LA:
        case webgl::UnsizedFormat::R:
        case webgl::UnsizedFormat::RG:
        case webgl::UnsizedFormat::RGB:
        case webgl::UnsizedFormat::RGBA:
          return true;
        default:
          return false;
      }

    case webgl::UnsizedFormat::RGB:
      switch (dstFormat->unsizedFormat) {
        case webgl::UnsizedFormat::L:
        case webgl::UnsizedFormat::R:
        case webgl::UnsizedFormat::RG:
        case webgl::UnsizedFormat::RGB:
          return true;
        default:
          return false;
      }

    case webgl::UnsizedFormat::RG:
      switch (dstFormat->unsizedFormat) {
        case webgl::UnsizedFormat::L:
        case webgl::UnsizedFormat::R:
        case webgl::UnsizedFormat::RG:
          return true;
        default:
          return false;
      }

    case webgl::UnsizedFormat::R:
      switch (dstFormat->unsizedFormat) {
        case webgl::UnsizedFormat::L:
        case webgl::UnsizedFormat::R:
          return true;
        default:
          return false;
      }

    default:
      return false;
  }
}

static bool EnsureImageDataInitializedForUpload(
    WebGLTexture* tex, TexImageTarget target, GLint level, GLint xOffset,
    GLint yOffset, GLint zOffset, uint32_t width, uint32_t height,
    uint32_t depth, webgl::ImageInfo* imageInfo) {
  if (!imageInfo->mHasData) {
    const bool isFullUpload =
        (!xOffset && !yOffset && !zOffset && width == imageInfo->mWidth &&
         height == imageInfo->mHeight && depth == imageInfo->mDepth);
    if (!isFullUpload) {
      WebGLContext* webgl = tex->mContext;
      webgl->GenerateWarning(
          "Texture has not been initialized prior to a"
          " partial upload, forcing the browser to clear it."
          " This may be slow.");
      if (!tex->EnsureImageDataInitialized(target, level)) {
        MOZ_ASSERT(false, "Unexpected failure to init image data.");
        return false;
      }
    }
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// Actual calls

static inline GLenum DoTexStorage(gl::GLContext* gl, TexTarget target,
                                  GLsizei levels, GLenum sizedFormat,
                                  GLsizei width, GLsizei height,
                                  GLsizei depth) {
  gl::GLContext::LocalErrorScope errorScope(*gl);

  switch (target.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP:
      MOZ_ASSERT(depth == 1);
      gl->fTexStorage2D(target.get(), levels, sizedFormat, width, height);
      break;

    case LOCAL_GL_TEXTURE_3D:
    case LOCAL_GL_TEXTURE_2D_ARRAY:
      gl->fTexStorage3D(target.get(), levels, sizedFormat, width, height,
                        depth);
      break;

    default:
      MOZ_CRASH("GFX: bad target");
  }

  return errorScope.GetError();
}

bool IsTarget3D(TexImageTarget target) {
  switch (target.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return false;

    case LOCAL_GL_TEXTURE_3D:
    case LOCAL_GL_TEXTURE_2D_ARRAY:
      return true;

    default:
      MOZ_CRASH("GFX: bad target");
  }
}

GLenum DoTexImage(gl::GLContext* gl, TexImageTarget target, GLint level,
                  const webgl::DriverUnpackInfo* dui, GLsizei width,
                  GLsizei height, GLsizei depth, const void* data) {
  const GLint border = 0;

  gl::GLContext::LocalErrorScope errorScope(*gl);

  if (IsTarget3D(target)) {
    gl->fTexImage3D(target.get(), level, dui->internalFormat, width, height,
                    depth, border, dui->unpackFormat, dui->unpackType, data);
  } else {
    MOZ_ASSERT(depth == 1);
    gl->fTexImage2D(target.get(), level, dui->internalFormat, width, height,
                    border, dui->unpackFormat, dui->unpackType, data);
  }

  return errorScope.GetError();
}

GLenum DoTexSubImage(gl::GLContext* gl, TexImageTarget target, GLint level,
                     GLint xOffset, GLint yOffset, GLint zOffset, GLsizei width,
                     GLsizei height, GLsizei depth,
                     const webgl::PackingInfo& pi, const void* data) {
  gl::GLContext::LocalErrorScope errorScope(*gl);

  if (IsTarget3D(target)) {
    gl->fTexSubImage3D(target.get(), level, xOffset, yOffset, zOffset, width,
                       height, depth, pi.format, pi.type, data);
  } else {
    MOZ_ASSERT(zOffset == 0);
    MOZ_ASSERT(depth == 1);
    gl->fTexSubImage2D(target.get(), level, xOffset, yOffset, width, height,
                       pi.format, pi.type, data);
  }

  return errorScope.GetError();
}

static inline GLenum DoCompressedTexImage(gl::GLContext* gl,
                                          TexImageTarget target, GLint level,
                                          GLenum internalFormat, GLsizei width,
                                          GLsizei height, GLsizei depth,
                                          GLsizei dataSize, const void* data) {
  const GLint border = 0;

  gl::GLContext::LocalErrorScope errorScope(*gl);

  if (IsTarget3D(target)) {
    gl->fCompressedTexImage3D(target.get(), level, internalFormat, width,
                              height, depth, border, dataSize, data);
  } else {
    MOZ_ASSERT(depth == 1);
    gl->fCompressedTexImage2D(target.get(), level, internalFormat, width,
                              height, border, dataSize, data);
  }

  return errorScope.GetError();
}

GLenum DoCompressedTexSubImage(gl::GLContext* gl, TexImageTarget target,
                               GLint level, GLint xOffset, GLint yOffset,
                               GLint zOffset, GLsizei width, GLsizei height,
                               GLsizei depth, GLenum sizedUnpackFormat,
                               GLsizei dataSize, const void* data) {
  gl::GLContext::LocalErrorScope errorScope(*gl);

  if (IsTarget3D(target)) {
    gl->fCompressedTexSubImage3D(target.get(), level, xOffset, yOffset, zOffset,
                                 width, height, depth, sizedUnpackFormat,
                                 dataSize, data);
  } else {
    MOZ_ASSERT(zOffset == 0);
    MOZ_ASSERT(depth == 1);
    gl->fCompressedTexSubImage2D(target.get(), level, xOffset, yOffset, width,
                                 height, sizedUnpackFormat, dataSize, data);
  }

  return errorScope.GetError();
}

static inline GLenum DoCopyTexSubImage(gl::GLContext* gl, TexImageTarget target,
                                       GLint level, GLint xOffset,
                                       GLint yOffset, GLint zOffset, GLint x,
                                       GLint y, GLsizei width, GLsizei height) {
  gl::GLContext::LocalErrorScope errorScope(*gl);

  if (IsTarget3D(target)) {
    gl->fCopyTexSubImage3D(target.get(), level, xOffset, yOffset, zOffset, x, y,
                           width, height);
  } else {
    MOZ_ASSERT(zOffset == 0);
    gl->fCopyTexSubImage2D(target.get(), level, xOffset, yOffset, x, y, width,
                           height);
  }

  return errorScope.GetError();
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// Actual (mostly generic) function implementations

static bool ValidateCompressedTexImageRestrictions(
    WebGLContext* webgl, TexImageTarget target, uint32_t level,
    const webgl::FormatInfo* format, uint32_t width, uint32_t height,
    uint32_t depth) {
  const auto fnIsDimValid_S3TC = [level](uint32_t size, uint32_t blockSize) {
    if (size % blockSize == 0) return true;

    if (level == 0) return false;

    return (size == 0 || size == 1 || size == 2);
  };

  switch (format->compression->family) {
    case webgl::CompressionFamily::ASTC:
      if (target == LOCAL_GL_TEXTURE_3D &&
          !webgl->gl->IsExtensionSupported(
              gl::GLContext::KHR_texture_compression_astc_hdr)) {
        webgl->ErrorInvalidOperation("TEXTURE_3D requires ASTC's hdr profile.");
        return false;
      }
      break;

    case webgl::CompressionFamily::PVRTC:
      if (!IsPowerOfTwo(width) || !IsPowerOfTwo(height)) {
        webgl->ErrorInvalidValue("%s requires power-of-two width and height.",
                                 format->name);
        return false;
      }

      break;

    case webgl::CompressionFamily::S3TC:
      if (!fnIsDimValid_S3TC(width, format->compression->blockWidth) ||
          !fnIsDimValid_S3TC(height, format->compression->blockHeight)) {
        webgl->ErrorInvalidOperation(
            "%s requires that width and height are"
            " block-aligned, or, if level>0, equal to 0, 1,"
            " or 2.",
            format->name);
        return false;
      }

      break;

    // Default: There are no restrictions on CompressedTexImage.
    default:  // ETC1, ES3
      break;
  }

  return true;
}

static bool ValidateTargetForFormat(WebGLContext* webgl, TexImageTarget target,
                                    const webgl::FormatInfo* format) {
  // GLES 3.0.4 p127:
  // "Textures with a base internal format of DEPTH_COMPONENT or DEPTH_STENCIL
  // are supported by texture image specification commands only if `target` is
  // TEXTURE_2D, TEXTURE_2D_ARRAY, or TEXTURE_CUBE_MAP. Using these formats in
  // conjunction with any other `target` will result in an INVALID_OPERATION
  // error."
  const bool ok = [&]() {
    if (bool(format->d) & (target == LOCAL_GL_TEXTURE_3D)) return false;

    if (format->compression) {
      switch (format->compression->family) {
        case webgl::CompressionFamily::ES3:
        case webgl::CompressionFamily::S3TC:
          if (target == LOCAL_GL_TEXTURE_3D) return false;
          break;

        case webgl::CompressionFamily::ETC1:
        case webgl::CompressionFamily::PVRTC:
        case webgl::CompressionFamily::RGTC:
          if (target == LOCAL_GL_TEXTURE_3D ||
              target == LOCAL_GL_TEXTURE_2D_ARRAY) {
            return false;
          }
          break;
        default:
          break;
      }
    }
    return true;
  }();
  if (!ok) {
    webgl->ErrorInvalidOperation("Format %s cannot be used with target %s.",
                                 format->name, GetEnumName(target.get()));
    return false;
  }

  return true;
}

void WebGLTexture::TexStorage(TexTarget target, GLsizei levels,
                              GLenum sizedFormat, GLsizei width, GLsizei height,
                              GLsizei depth) {
  // Check levels
  if (levels < 1) {
    mContext->ErrorInvalidValue("`levels` must be >= 1.");
    return;
  }

  if (!width || !height || !depth) {
    mContext->ErrorInvalidValue("Dimensions must be non-zero.");
    return;
  }

  const TexImageTarget testTarget =
      IsCubeMap() ? LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X : target.get();
  webgl::ImageInfo* baseImageInfo;
  if (!ValidateTexImageSpecification(testTarget, 0, width, height, depth,
                                     &baseImageInfo)) {
    return;
  }
  MOZ_ALWAYS_TRUE(baseImageInfo);

  auto dstUsage = mContext->mFormatUsage->GetSizedTexUsage(sizedFormat);
  if (!dstUsage) {
    mContext->ErrorInvalidEnumInfo("internalformat", sizedFormat);
    return;
  }
  auto dstFormat = dstUsage->format;

  if (!ValidateTargetForFormat(mContext, testTarget, dstFormat)) return;

  if (dstFormat->compression) {
    if (!ValidateCompressedTexImageRestrictions(
            mContext, testTarget, 0, dstFormat, width, height, depth)) {
      return;
    }
  }

  ////////////////////////////////////

  const bool levelsOk = [&]() {
    // Right-shift is only defined for bits-1, which is too large anyways.
    const auto lastLevel = uint32_t(levels - 1);
    if (lastLevel > 31) return false;

    const auto lastLevelWidth = uint32_t(width) >> lastLevel;
    const auto lastLevelHeight = uint32_t(height) >> lastLevel;

    // If these are all zero, then some earlier level was the final 1x1(x1)
    // level.
    bool ok = lastLevelWidth || lastLevelHeight;
    if (target == LOCAL_GL_TEXTURE_3D) {
      const auto lastLevelDepth = uint32_t(depth) >> lastLevel;
      ok |= bool(lastLevelDepth);
    }
    return ok;
  }();
  if (!levelsOk) {
    mContext->ErrorInvalidOperation(
        "Too many levels requested for the given"
        " dimensions. (levels: %u, width: %u, height: %u,"
        " depth: %u)",
        levels, width, height, depth);
    return;
  }

  ////////////////////////////////////
  // Do the thing!

  GLenum error = DoTexStorage(mContext->gl, target.get(), levels, sizedFormat,
                              width, height, depth);

  mContext->OnDataAllocCall();

  if (error == LOCAL_GL_OUT_OF_MEMORY) {
    mContext->ErrorOutOfMemory("Ran out of memory during texture allocation.");
    Truncate();
    return;
  }
  if (error) {
    mContext->GenerateError(error, "Unexpected error from driver.");
    const nsPrintfCString call(
        "DoTexStorage(0x%04x, %i, 0x%04x, %i,%i,%i) -> 0x%04x", target.get(),
        levels, sizedFormat, width, height, depth, error);
    gfxCriticalError() << "Unexpected error from driver: "
                       << call.BeginReading();
    return;
  }

  ////////////////////////////////////
  // Update our specification data.

  const bool isDataInitialized = false;
  const webgl::ImageInfo newInfo{dstUsage, uint32_t(width), uint32_t(height),
                                 uint32_t(depth), isDataInitialized};

  {
    const auto base_level = mBaseMipmapLevel;
    mBaseMipmapLevel = 0;

    ImageInfoAtFace(0, 0) = newInfo;
    PopulateMipChain(levels - 1);

    mBaseMipmapLevel = base_level;
  }

  mImmutable = true;
  mImmutableLevelCount = AutoAssertCast(levels);
  ClampLevelBaseAndMax();
}

////////////////////////////////////////
// Tex(Sub)Image

void WebGLTexture::TexImage(TexImageTarget target, GLint level,
                            GLenum internalFormat, const webgl::PackingInfo& pi,
                            const webgl::TexUnpackBlob* blob) {
  ////////////////////////////////////
  // Get dest info

  webgl::ImageInfo* imageInfo;
  if (!ValidateTexImageSpecification(target, level, blob->mWidth, blob->mHeight,
                                     blob->mDepth, &imageInfo)) {
    return;
  }
  MOZ_ASSERT(imageInfo);

  const auto& fua = mContext->mFormatUsage;
  if (!fua->IsInternalFormatEnumValid(internalFormat)) {
    mContext->ErrorInvalidValue("Invalid internalformat: 0x%04x",
                                internalFormat);
    return;
  }

  auto dstUsage = fua->GetSizedTexUsage(internalFormat);
  if (!dstUsage) {
    if (internalFormat != pi.format) {
      /* GL ES Version 3.0.4 - 3.8.3 Texture Image Specification
       *   "Specifying a combination of values for format, type, and
       *   internalformat that is not listed as a valid combination
       *   in tables 3.2 or 3.3 generates the error INVALID_OPERATION."
       */
      mContext->ErrorInvalidOperation(
          "Unsized internalFormat must match"
          " unpack format.");
      return;
    }

    dstUsage = fua->GetUnsizedTexUsage(pi);
  }

  if (!dstUsage) {
    mContext->ErrorInvalidOperation(
        "Invalid internalformat/format/type:"
        " 0x%04x/0x%04x/0x%04x",
        internalFormat, pi.format, pi.type);
    return;
  }

  const webgl::DriverUnpackInfo* driverUnpackInfo;
  if (!dstUsage->IsUnpackValid(pi, &driverUnpackInfo)) {
    mContext->ErrorInvalidOperation(
        "Mismatched internalFormat and format/type:"
        " 0x%04x and 0x%04x/0x%04x",
        internalFormat, pi.format, pi.type);
    return;
  }

  ////////////////////////////////////
  // Check that source and dest info are compatible
  auto dstFormat = dstUsage->format;

  if (!ValidateTargetForFormat(mContext, target, dstFormat)) return;

  if (!mContext->IsWebGL2() && dstFormat->d) {
    if (target != LOCAL_GL_TEXTURE_2D || blob->HasData() || level != 0) {
      mContext->ErrorInvalidOperation(
          "With format %s, this function may only"
          " be called with target=TEXTURE_2D,"
          " data=null, and level=0.",
          dstFormat->name);
      return;
    }
  }

  ////////////////////////////////////
  // Do the thing!

  // It's tempting to do allocation first, and TexSubImage second, but this is
  // generally slower.

  const webgl::ImageInfo newImageInfo{dstUsage, blob->mWidth, blob->mHeight,
                                      blob->mDepth, blob->HasData()};

  const bool isSubImage = false;
  const bool needsRespec = (imageInfo->mWidth != newImageInfo.mWidth ||
                            imageInfo->mHeight != newImageInfo.mHeight ||
                            imageInfo->mDepth != newImageInfo.mDepth ||
                            imageInfo->mFormat != newImageInfo.mFormat);
  const GLint xOffset = 0;
  const GLint yOffset = 0;
  const GLint zOffset = 0;

  GLenum glError;
  if (!blob->TexOrSubImage(isSubImage, needsRespec, this, target, level,
                           driverUnpackInfo, xOffset, yOffset, zOffset, pi,
                           &glError)) {
    return;
  }

  mContext->OnDataAllocCall();

  if (glError == LOCAL_GL_OUT_OF_MEMORY) {
    mContext->ErrorOutOfMemory("Driver ran out of memory during upload.");
    Truncate();
    return;
  }

  if (glError) {
    const auto enumStr = EnumString(glError);
    const nsPrintfCString dui(
        "Unexpected error %s during upload. (dui: %x/%x/%x)", enumStr.c_str(),
        driverUnpackInfo->internalFormat, driverUnpackInfo->unpackFormat,
        driverUnpackInfo->unpackType);
    mContext->ErrorInvalidOperation("%s", dui.BeginReading());
    gfxCriticalError() << mContext->FuncName() << ": " << dui.BeginReading();
    return;
  }

  ////////////////////////////////////
  // Update our specification data.

  *imageInfo = newImageInfo;
  InvalidateCaches();
}

void WebGLTexture::TexSubImage(TexImageTarget target, GLint level,
                               GLint xOffset, GLint yOffset, GLint zOffset,
                               const webgl::PackingInfo& pi,
                               const webgl::TexUnpackBlob* blob) {
  ////////////////////////////////////
  // Get dest info

  webgl::ImageInfo* imageInfo;
  if (!ValidateTexImageSelection(target, level, xOffset, yOffset, zOffset,
                                 blob->mWidth, blob->mHeight, blob->mDepth,
                                 &imageInfo)) {
    return;
  }
  MOZ_ASSERT(imageInfo);

  auto dstUsage = imageInfo->mFormat;
  auto dstFormat = dstUsage->format;

  if (!mContext->IsWebGL2() && dstFormat->d) {
    mContext->ErrorInvalidOperation(
        "Function may not be called on a texture of"
        " format %s.",
        dstFormat->name);
    return;
  }

  ////////////////////////////////////
  // Get source info

  const webgl::DriverUnpackInfo* driverUnpackInfo;
  if (!dstUsage->IsUnpackValid(pi, &driverUnpackInfo)) {
    mContext->ErrorInvalidOperation(
        "Mismatched internalFormat and format/type:"
        " %s and 0x%04x/0x%04x",
        dstFormat->name, pi.format, pi.type);
    return;
  }

  ////////////////////////////////////
  // Do the thing!

  if (!EnsureImageDataInitializedForUpload(
          this, target, level, xOffset, yOffset, zOffset, blob->mWidth,
          blob->mHeight, blob->mDepth, imageInfo)) {
    return;
  }

  const bool isSubImage = true;
  const bool needsRespec = false;

  GLenum glError;
  if (!blob->TexOrSubImage(isSubImage, needsRespec, this, target, level,
                           driverUnpackInfo, xOffset, yOffset, zOffset, pi,
                           &glError)) {
    return;
  }

  if (glError == LOCAL_GL_OUT_OF_MEMORY) {
    mContext->ErrorOutOfMemory("Driver ran out of memory during upload.");
    Truncate();
    return;
  }

  if (glError) {
    const auto enumStr = EnumString(glError);
    const nsPrintfCString dui(
        "Unexpected error %s during upload. (dui: %x/%x/%x)", enumStr.c_str(),
        driverUnpackInfo->internalFormat, driverUnpackInfo->unpackFormat,
        driverUnpackInfo->unpackType);
    mContext->ErrorInvalidOperation("%s", dui.BeginReading());
    gfxCriticalError() << mContext->FuncName() << ": " << dui.BeginReading();
    return;
  }

  ////////////////////////////////////
  // Update our specification data?

  imageInfo->mHasData = true;
}

////////////////////////////////////////
// CompressedTex(Sub)Image

UniquePtr<webgl::TexUnpackBytes> WebGLContext::FromCompressed(
    TexImageTarget target, GLsizei rawWidth, GLsizei rawHeight,
    GLsizei rawDepth, GLint border, const TexImageSource& src,
    const Maybe<GLsizei>& expectedImageSize) {
  uint32_t width, height, depth;
  if (!ValidateExtents(this, rawWidth, rawHeight, rawDepth, border, &width,
                       &height, &depth)) {
    return nullptr;
  }

  if (src.mPboOffset) {
    return FromPboOffset(this, target, width, height, depth, *(src.mPboOffset),
                         expectedImageSize);
  }

  if (mBoundPixelUnpackBuffer) {
    ErrorInvalidOperation("PIXEL_UNPACK_BUFFER must be null.");
    return nullptr;
  }

  return FromView(this, target, width, height, depth, src.mView,
                  src.mViewElemOffset, src.mViewElemLengthOverride,
                  LOCAL_GL_INVALID_VALUE);
}

void WebGLTexture::CompressedTexImage(TexImageTarget target, GLint level,
                                      GLenum internalFormat, GLsizei rawWidth,
                                      GLsizei rawHeight, GLsizei rawDepth,
                                      GLint border, const TexImageSource& src,
                                      const Maybe<GLsizei>& expectedImageSize) {
  const auto blob = mContext->FromCompressed(
      target, rawWidth, rawHeight, rawDepth, border, src, expectedImageSize);
  if (!blob) return;

  ////////////////////////////////////
  // Get dest info

  webgl::ImageInfo* imageInfo;
  if (!ValidateTexImageSpecification(target, level, blob->mWidth, blob->mHeight,
                                     blob->mDepth, &imageInfo)) {
    return;
  }
  MOZ_ASSERT(imageInfo);

  auto usage = mContext->mFormatUsage->GetSizedTexUsage(internalFormat);
  if (!usage || !usage->format->compression) {
    mContext->ErrorInvalidEnumArg("internalFormat", internalFormat);
    return;
  }
  auto format = usage->format;

  if (!ValidateTargetForFormat(mContext, target, format)) return;

  ////////////////////////////////////
  // Get source info

  if (!ValidateCompressedTexUnpack(mContext, blob->mWidth, blob->mHeight,
                                   blob->mDepth, format, blob->mAvailBytes)) {
    return;
  }

  ////////////////////////////////////
  // Check that source is compatible with dest

  if (!ValidateCompressedTexImageRestrictions(mContext, target, level, format,
                                              blob->mWidth, blob->mHeight,
                                              blob->mDepth)) {
    return;
  }

  ////////////////////////////////////
  // Do the thing!

  const ScopedLazyBind bindPBO(mContext->gl, LOCAL_GL_PIXEL_UNPACK_BUFFER,
                               mContext->mBoundPixelUnpackBuffer);

  // Warning: Possibly shared memory.  See bug 1225033.
  const auto error = DoCompressedTexImage(
      mContext->gl, target, level, internalFormat, blob->mWidth, blob->mHeight,
      blob->mDepth, blob->mAvailBytes, blob->mPtr);
  mContext->OnDataAllocCall();
  if (error == LOCAL_GL_OUT_OF_MEMORY) {
    mContext->ErrorOutOfMemory("Ran out of memory during upload.");
    Truncate();
    return;
  }
  if (error) {
    mContext->GenerateError(error, "Unexpected error from driver.");
    const nsPrintfCString call(
        "DoCompressedTexImage(0x%04x, %i, 0x%04x, %u,%u,%u, %u) -> 0x%04x",
        target.get(), level, internalFormat, blob->mWidth, blob->mHeight,
        blob->mDepth, uint32_t(blob->mAvailBytes), error);
    gfxCriticalError() << "Unexpected error from driver: "
                       << call.BeginReading();
    return;
  }

  ////////////////////////////////////
  // Update our specification data.

  const bool isDataInitialized = true;
  const webgl::ImageInfo newImageInfo{usage, blob->mWidth, blob->mHeight,
                                      blob->mDepth, isDataInitialized};
  *imageInfo = newImageInfo;
  InvalidateCaches();
}

static inline bool IsSubImageBlockAligned(
    const webgl::CompressedFormatInfo* compression,
    const webgl::ImageInfo* imageInfo, GLint xOffset, GLint yOffset,
    uint32_t width, uint32_t height) {
  if (xOffset % compression->blockWidth != 0 ||
      yOffset % compression->blockHeight != 0) {
    return false;
  }

  if (width % compression->blockWidth != 0 &&
      xOffset + width != imageInfo->mWidth)
    return false;

  if (height % compression->blockHeight != 0 &&
      yOffset + height != imageInfo->mHeight)
    return false;

  return true;
}

void WebGLTexture::CompressedTexSubImage(
    TexImageTarget target, GLint level, GLint xOffset, GLint yOffset,
    GLint zOffset, GLsizei rawWidth, GLsizei rawHeight, GLsizei rawDepth,
    GLenum sizedUnpackFormat, const TexImageSource& src,
    const Maybe<GLsizei>& expectedImageSize) {
  const GLint border = 0;
  const auto blob = mContext->FromCompressed(
      target, rawWidth, rawHeight, rawDepth, border, src, expectedImageSize);
  if (!blob) return;

  ////////////////////////////////////
  // Get dest info

  webgl::ImageInfo* imageInfo;
  if (!ValidateTexImageSelection(target, level, xOffset, yOffset, zOffset,
                                 blob->mWidth, blob->mHeight, blob->mDepth,
                                 &imageInfo)) {
    return;
  }
  MOZ_ASSERT(imageInfo);

  auto dstUsage = imageInfo->mFormat;
  auto dstFormat = dstUsage->format;

  ////////////////////////////////////
  // Get source info

  auto srcUsage = mContext->mFormatUsage->GetSizedTexUsage(sizedUnpackFormat);
  if (!srcUsage || !srcUsage->format->compression) {
    mContext->ErrorInvalidEnumArg("sizedUnpackFormat", sizedUnpackFormat);
    return;
  }
  if (srcUsage != dstUsage) {
    mContext->ErrorInvalidOperation(
        "`format` must match the format of the"
        " existing texture image.");
    return;
  }

  auto format = srcUsage->format;
  MOZ_ASSERT(format == dstFormat);
  if (!ValidateCompressedTexUnpack(mContext, blob->mWidth, blob->mHeight,
                                   blob->mDepth, format, blob->mAvailBytes)) {
    return;
  }

  ////////////////////////////////////
  // Check that source is compatible with dest

  switch (format->compression->family) {
    // Forbidden:
    case webgl::CompressionFamily::ETC1:
      mContext->ErrorInvalidOperation(
          "Format does not allow sub-image"
          " updates.");
      return;

    // Block-aligned:
    case webgl::CompressionFamily::ES3:  // Yes, the ES3 formats don't match the
                                         // ES3
    case webgl::CompressionFamily::S3TC:  // default behavior.
    case webgl::CompressionFamily::BPTC:
    case webgl::CompressionFamily::RGTC:
      if (!IsSubImageBlockAligned(dstFormat->compression, imageInfo, xOffset,
                                  yOffset, blob->mWidth, blob->mHeight)) {
        mContext->ErrorInvalidOperation(
            "Format requires block-aligned sub-image"
            " updates.");
        return;
      }
      break;

    // Full-only: (The ES3 default)
    default:  // PVRTC
      if (xOffset || yOffset || blob->mWidth != imageInfo->mWidth ||
          blob->mHeight != imageInfo->mHeight) {
        mContext->ErrorInvalidOperation(
            "Format does not allow partial sub-image"
            " updates.");
        return;
      }
      break;
  }

  ////////////////////////////////////
  // Do the thing!

  if (!EnsureImageDataInitializedForUpload(
          this, target, level, xOffset, yOffset, zOffset, blob->mWidth,
          blob->mHeight, blob->mDepth, imageInfo)) {
    return;
  }

  const ScopedLazyBind bindPBO(mContext->gl, LOCAL_GL_PIXEL_UNPACK_BUFFER,
                               mContext->mBoundPixelUnpackBuffer);

  // Warning: Possibly shared memory.  See bug 1225033.
  const auto error = DoCompressedTexSubImage(
      mContext->gl, target, level, xOffset, yOffset, zOffset, blob->mWidth,
      blob->mHeight, blob->mDepth, sizedUnpackFormat, blob->mAvailBytes,
      blob->mPtr);
  if (error == LOCAL_GL_OUT_OF_MEMORY) {
    mContext->ErrorOutOfMemory("Ran out of memory during upload.");
    Truncate();
    return;
  }
  if (error) {
    mContext->GenerateError(error, "Unexpected error from driver.");
    const nsPrintfCString call(
        "DoCompressedTexSubImage(0x%04x, %i, %i,%i,%i, %u,%u,%u, 0x%04x, %u) "
        "-> 0x%04x",
        target.get(), level, xOffset, yOffset, zOffset, blob->mWidth,
        blob->mHeight, blob->mDepth, sizedUnpackFormat,
        uint32_t(blob->mAvailBytes), error);
    gfxCriticalError() << "Unexpected error from driver: "
                       << call.BeginReading();
    return;
  }

  ////////////////////////////////////
  // Update our specification data?

  imageInfo->mHasData = true;
}

////////////////////////////////////////
// CopyTex(Sub)Image

static bool ValidateCopyTexImageFormats(WebGLContext* webgl,
                                        const webgl::FormatInfo* srcFormat,
                                        const webgl::FormatInfo* dstFormat) {
  MOZ_ASSERT(!srcFormat->compression);
  if (dstFormat->compression) {
    webgl->ErrorInvalidEnum(
        "Specified destination must not have a compressed"
        " format.");
    return false;
  }

  if (dstFormat->effectiveFormat == webgl::EffectiveFormat::RGB9_E5) {
    webgl->ErrorInvalidOperation(
        "RGB9_E5 is an invalid destination for"
        " CopyTex(Sub)Image. (GLES 3.0.4 p145)");
    return false;
  }

  if (!DoChannelsMatchForCopyTexImage(srcFormat, dstFormat)) {
    webgl->ErrorInvalidOperation(
        "Destination channels must be compatible with"
        " source channels. (GLES 3.0.4 p140 Table 3.16)");
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////

class ScopedCopyTexImageSource {
  WebGLContext* const mWebGL;
  GLuint mRB;
  GLuint mFB;

 public:
  ScopedCopyTexImageSource(WebGLContext* webgl, uint32_t srcWidth,
                           uint32_t srcHeight,
                           const webgl::FormatInfo* srcFormat,
                           const webgl::FormatUsageInfo* dstUsage);
  ~ScopedCopyTexImageSource();
};

ScopedCopyTexImageSource::ScopedCopyTexImageSource(
    WebGLContext* webgl, uint32_t srcWidth, uint32_t srcHeight,
    const webgl::FormatInfo* srcFormat, const webgl::FormatUsageInfo* dstUsage)
    : mWebGL(webgl), mRB(0), mFB(0) {
  switch (dstUsage->format->unsizedFormat) {
    case webgl::UnsizedFormat::L:
    case webgl::UnsizedFormat::A:
    case webgl::UnsizedFormat::LA:
      webgl->GenerateWarning(
          "Copying to a LUMINANCE, ALPHA, or LUMINANCE_ALPHA"
          " is deprecated, and has severely reduced performance"
          " on some platforms.");
      break;

    default:
      MOZ_ASSERT(!dstUsage->textureSwizzleRGBA);
      return;
  }

  if (!dstUsage->textureSwizzleRGBA) return;

  gl::GLContext* gl = webgl->gl;

  GLenum sizedFormat;

  switch (srcFormat->componentType) {
    case webgl::ComponentType::NormUInt:
      sizedFormat = LOCAL_GL_RGBA8;
      break;

    case webgl::ComponentType::Float:
      if (webgl->IsExtensionEnabled(
              WebGLExtensionID::WEBGL_color_buffer_float)) {
        sizedFormat = LOCAL_GL_RGBA32F;
        break;
      }

      if (webgl->IsExtensionEnabled(
              WebGLExtensionID::EXT_color_buffer_half_float)) {
        sizedFormat = LOCAL_GL_RGBA16F;
        break;
      }
      MOZ_CRASH("GFX: Should be able to request CopyTexImage from Float.");

    default:
      MOZ_CRASH("GFX: Should be able to request CopyTexImage from this type.");
  }

  gl::ScopedTexture scopedTex(gl);
  gl::ScopedBindTexture scopedBindTex(gl, scopedTex.Texture(),
                                      LOCAL_GL_TEXTURE_2D);

  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                     LOCAL_GL_NEAREST);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                     LOCAL_GL_NEAREST);

  GLint blitSwizzle[4] = {LOCAL_GL_ZERO};
  switch (dstUsage->format->unsizedFormat) {
    case webgl::UnsizedFormat::L:
      blitSwizzle[0] = LOCAL_GL_RED;
      break;

    case webgl::UnsizedFormat::A:
      blitSwizzle[0] = LOCAL_GL_ALPHA;
      break;

    case webgl::UnsizedFormat::LA:
      blitSwizzle[0] = LOCAL_GL_RED;
      blitSwizzle[1] = LOCAL_GL_ALPHA;
      break;

    default:
      MOZ_CRASH("GFX: Unhandled unsizedFormat.");
  }

  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_SWIZZLE_R,
                     blitSwizzle[0]);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_SWIZZLE_G,
                     blitSwizzle[1]);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_SWIZZLE_B,
                     blitSwizzle[2]);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_SWIZZLE_A,
                     blitSwizzle[3]);

  gl->fCopyTexImage2D(LOCAL_GL_TEXTURE_2D, 0, sizedFormat, 0, 0, srcWidth,
                      srcHeight, 0);

  // Now create the swizzled FB we'll be exposing.

  GLuint rgbaRB = 0;
  gl->fGenRenderbuffers(1, &rgbaRB);
  gl::ScopedBindRenderbuffer scopedRB(gl, rgbaRB);
  gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, sizedFormat, srcWidth,
                           srcHeight);

  GLuint rgbaFB = 0;
  gl->fGenFramebuffers(1, &rgbaFB);
  gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, rgbaFB);
  gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                               LOCAL_GL_RENDERBUFFER, rgbaRB);

  const GLenum status = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
  if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
    MOZ_CRASH("GFX: Temp framebuffer is not complete.");
  }

  // Restore RB binding.
  scopedRB.Unwrap();  // This function should really have a better name.

  // Draw-blit rgbaTex into rgbaFB.
  const gfx::IntSize srcSize(srcWidth, srcHeight);
  {
    const gl::ScopedBindFramebuffer bindFB(gl, rgbaFB);
    gl->BlitHelper()->DrawBlitTextureToFramebuffer(scopedTex.Texture(), srcSize,
                                                   srcSize);
  }

  // Restore Tex2D binding and destroy the temp tex.
  scopedBindTex.Unwrap();
  scopedTex.Unwrap();

  // Leave RB and FB alive, and FB bound.
  mRB = rgbaRB;
  mFB = rgbaFB;
}

template <typename T>
static inline GLenum ToGLHandle(const T& obj) {
  return (obj ? obj->mGLName : 0);
}

ScopedCopyTexImageSource::~ScopedCopyTexImageSource() {
  if (!mFB) {
    MOZ_ASSERT(!mRB);
    return;
  }
  MOZ_ASSERT(mRB);

  gl::GLContext* gl = mWebGL->gl;

  // If we're swizzling, it's because we're on a GL core (3.2+) profile, which
  // has split framebuffer support.
  gl->fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER,
                       ToGLHandle(mWebGL->mBoundDrawFramebuffer));
  gl->fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER,
                       ToGLHandle(mWebGL->mBoundReadFramebuffer));

  gl->fDeleteFramebuffers(1, &mFB);
  gl->fDeleteRenderbuffers(1, &mRB);
}

////////////////////////////////////////////////////////////////////////////////

static bool GetUnsizedFormatForCopy(GLenum internalFormat,
                                    webgl::UnsizedFormat* const out) {
  switch (internalFormat) {
    case LOCAL_GL_RED:
      *out = webgl::UnsizedFormat::R;
      break;
    case LOCAL_GL_RG:
      *out = webgl::UnsizedFormat::RG;
      break;
    case LOCAL_GL_RGB:
      *out = webgl::UnsizedFormat::RGB;
      break;
    case LOCAL_GL_RGBA:
      *out = webgl::UnsizedFormat::RGBA;
      break;
    case LOCAL_GL_LUMINANCE:
      *out = webgl::UnsizedFormat::L;
      break;
    case LOCAL_GL_ALPHA:
      *out = webgl::UnsizedFormat::A;
      break;
    case LOCAL_GL_LUMINANCE_ALPHA:
      *out = webgl::UnsizedFormat::LA;
      break;

    default:
      return false;
  }

  return true;
}

static const webgl::FormatUsageInfo* ValidateCopyDestUsage(
    WebGLContext* webgl, const webgl::FormatInfo* srcFormat,
    GLenum internalFormat) {
  const auto& fua = webgl->mFormatUsage;

  switch (internalFormat) {
    case LOCAL_GL_R8_SNORM:
    case LOCAL_GL_RG8_SNORM:
    case LOCAL_GL_RGB8_SNORM:
    case LOCAL_GL_RGBA8_SNORM:
      webgl->ErrorInvalidEnum("SNORM formats are invalid for CopyTexImage.");
      return nullptr;
  }

  auto dstUsage = fua->GetSizedTexUsage(internalFormat);
  if (!dstUsage) {
    // Ok, maybe it's unsized.
    webgl::UnsizedFormat unsizedFormat;
    if (!GetUnsizedFormatForCopy(internalFormat, &unsizedFormat)) {
      webgl->ErrorInvalidEnumInfo("internalFormat", internalFormat);
      return nullptr;
    }

    const auto dstFormat = srcFormat->GetCopyDecayFormat(unsizedFormat);
    if (dstFormat) {
      dstUsage = fua->GetUsage(dstFormat->effectiveFormat);
    }
    if (!dstUsage) {
      webgl->ErrorInvalidOperation(
          "0x%04x is not a valid unsized format for"
          " source format %s.",
          internalFormat, srcFormat->name);
      return nullptr;
    }

    return dstUsage;
  }
  // Alright, it's sized.

  const auto dstFormat = dstUsage->format;

  if (dstFormat->componentType != srcFormat->componentType) {
    webgl->ErrorInvalidOperation(
        "For sized internalFormats, source and dest"
        " component types must match. (source: %s, dest:"
        " %s)",
        srcFormat->name, dstFormat->name);
    return nullptr;
  }

  bool componentSizesMatch = true;
  if (dstFormat->r) {
    componentSizesMatch &= (dstFormat->r == srcFormat->r);
  }
  if (dstFormat->g) {
    componentSizesMatch &= (dstFormat->g == srcFormat->g);
  }
  if (dstFormat->b) {
    componentSizesMatch &= (dstFormat->b == srcFormat->b);
  }
  if (dstFormat->a) {
    componentSizesMatch &= (dstFormat->a == srcFormat->a);
  }

  if (!componentSizesMatch) {
    webgl->ErrorInvalidOperation(
        "For sized internalFormats, source and dest"
        " component sizes must match exactly. (source: %s,"
        " dest: %s)",
        srcFormat->name, dstFormat->name);
    return nullptr;
  }

  return dstUsage;
}

bool WebGLTexture::ValidateCopyTexImageForFeedback(uint32_t level,
                                                   GLint layer) const {
  const auto& fb = mContext->mBoundReadFramebuffer;
  if (fb) {
    const auto& attach = fb->ColorReadBuffer();
    MOZ_ASSERT(attach);

    if (attach->Texture() == this && attach->Layer() == layer &&
        uint32_t(attach->MipLevel()) == level) {
      // Note that the TexImageTargets *don't* have to match for this to be
      // undefined per GLES 3.0.4 p211, thus an INVALID_OP in WebGL.
      mContext->ErrorInvalidOperation(
          "Feedback loop detected, as this texture"
          " is already attached to READ_FRAMEBUFFER's"
          " READ_BUFFER-selected COLOR_ATTACHMENT%u.",
          attach->mAttachmentPoint);
      return false;
    }
  }
  return true;
}

static bool DoCopyTexOrSubImage(WebGLContext* webgl, bool isSubImage,
                                WebGLTexture* const tex, const TexImageTarget target,
                                GLint level, GLint xWithinSrc, GLint yWithinSrc,
                                uint32_t srcTotalWidth, uint32_t srcTotalHeight,
                                const webgl::FormatUsageInfo* srcUsage,
                                GLint xOffset, GLint yOffset, GLint zOffset,
                                uint32_t dstWidth, uint32_t dstHeight,
                                const webgl::FormatUsageInfo* dstUsage) {
  const auto& gl = webgl->gl;

  ////

  int32_t readX, readY;
  int32_t writeX, writeY;
  int32_t rwWidth, rwHeight;
  if (!Intersect(srcTotalWidth, xWithinSrc, dstWidth, &readX, &writeX,
                 &rwWidth) ||
      !Intersect(srcTotalHeight, yWithinSrc, dstHeight, &readY, &writeY,
                 &rwHeight)) {
    webgl->ErrorOutOfMemory("Bad subrect selection.");
    return false;
  }

  writeX += xOffset;
  writeY += yOffset;

  ////

  GLenum error = 0;
  nsCString errorText;
  do {
    const auto& idealUnpack = dstUsage->idealUnpack;
    if (!isSubImage) {
      UniqueBuffer buffer;

      if (uint32_t(rwWidth) != dstWidth || uint32_t(rwHeight) != dstHeight) {
        const auto& pi = idealUnpack->ToPacking();
        CheckedInt<size_t> byteCount = BytesPerPixel(pi);
        byteCount *= dstWidth;
        byteCount *= dstHeight;

        if (byteCount.isValid()) {
          buffer = calloc(1u, byteCount.value());
        }

        if (!buffer.get()) {
          webgl->ErrorOutOfMemory("Ran out of memory allocating zeros.");
          return false;
        }
      }

      const ScopedUnpackReset unpackReset(webgl);
      gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 1);
      error = DoTexImage(gl, target, level, idealUnpack, dstWidth, dstHeight, 1,
                         buffer.get());
      if (error) {
        errorText = nsPrintfCString(
            "DoTexImage(0x%04x, %i, {0x%04x, 0x%04x, 0x%04x}, %u,%u,1) -> "
            "0x%04x",
            target.get(), level, idealUnpack->internalFormat,
            idealUnpack->unpackFormat, idealUnpack->unpackType, dstWidth,
            dstHeight, error);
        break;
      }
    }

    if (!rwWidth || !rwHeight) {
      // There aren't any pixels to copy, so we're 'done'.
      return true;
    }

    const auto& srcFormat = srcUsage->format;
    ScopedCopyTexImageSource maybeSwizzle(webgl, srcTotalWidth, srcTotalHeight,
                                          srcFormat, dstUsage);

    error = DoCopyTexSubImage(gl, target, level, writeX, writeY, zOffset, readX,
                              readY, rwWidth, rwHeight);
    if (error) {
      errorText = nsPrintfCString(
          "DoCopyTexSubImage(0x%04x, %i, %i,%i,%i, %i,%i, %u,%u) -> 0x%04x",
          target.get(), level, writeX, writeY, zOffset, readX, readY, rwWidth,
          rwHeight, error);
      break;
    }

    return true;
  } while (false);

  if (error == LOCAL_GL_OUT_OF_MEMORY) {
    webgl->ErrorOutOfMemory("Ran out of memory during texture copy.");
    tex->Truncate();
    return false;
  }

  if (gl->IsANGLE() && error == LOCAL_GL_INVALID_OPERATION) {
    webgl->ErrorImplementationBug(
        "ANGLE is particular about CopyTexSubImage"
        " formats matching exactly.");
    return false;
  }

  webgl->GenerateError(error, "Unexpected error from driver.");
  gfxCriticalError() << "Unexpected error from driver: "
                     << errorText.BeginReading();
  return false;
}

// There is no CopyTexImage3D.
void WebGLTexture::CopyTexImage2D(TexImageTarget target, GLint level,
                                  GLenum internalFormat, GLint x, GLint y,
                                  GLsizei rawWidth, GLsizei rawHeight,
                                  GLint border) {
  ////////////////////////////////////
  // Get dest info

  uint32_t width, height, depth;
  if (!ValidateExtents(mContext, rawWidth, rawHeight, 1, border, &width,
                       &height, &depth)) {
    return;
  }

  webgl::ImageInfo* imageInfo;
  if (!ValidateTexImageSpecification(target, level, width, height, depth,
                                     &imageInfo))
    return;
  MOZ_ASSERT(imageInfo);

  ////////////////////////////////////
  // Get source info

  const webgl::FormatUsageInfo* srcUsage;
  uint32_t srcTotalWidth;
  uint32_t srcTotalHeight;
  if (!mContext->BindCurFBForColorRead(&srcUsage, &srcTotalWidth,
                                       &srcTotalHeight)) {
    return;
  }

  if (!ValidateCopyTexImageForFeedback(level)) return;

  ////////////////////////////////////
  // Check that source and dest info are compatible

  const auto& srcFormat = srcUsage->format;
  const auto dstUsage =
      ValidateCopyDestUsage(mContext, srcFormat, internalFormat);
  if (!dstUsage) return;

  const auto& dstFormat = dstUsage->format;
  if (!ValidateTargetForFormat(mContext, target, dstFormat)) return;

  if (!mContext->IsWebGL2() && dstFormat->d) {
    mContext->ErrorInvalidOperation(
        "Function may not be called with format %s.", dstFormat->name);
    return;
  }

  if (!ValidateCopyTexImageFormats(mContext, srcFormat, dstFormat)) return;

  ////////////////////////////////////
  // Do the thing!

  const bool isSubImage = false;
  if (!DoCopyTexOrSubImage(mContext, isSubImage, this, target, level, x, y,
                           srcTotalWidth, srcTotalHeight, srcUsage, 0, 0, 0,
                           width, height, dstUsage)) {
    return;
  }

  mContext->OnDataAllocCall();

  ////////////////////////////////////
  // Update our specification data.

  const bool isDataInitialized = true;
  const webgl::ImageInfo newImageInfo{dstUsage, width, height, depth,
                                      isDataInitialized};
  *imageInfo = newImageInfo;
  InvalidateCaches();
}

void WebGLTexture::CopyTexSubImage(TexImageTarget target, GLint level,
                                   GLint xOffset, GLint yOffset, GLint zOffset,
                                   GLint x, GLint y, GLsizei rawWidth,
                                   GLsizei rawHeight) {
  uint32_t width, height, depth;
  if (!ValidateExtents(mContext, rawWidth, rawHeight, 1, 0, &width, &height,
                       &depth)) {
    return;
  }

  ////////////////////////////////////
  // Get dest info

  webgl::ImageInfo* imageInfo;
  if (!ValidateTexImageSelection(target, level, xOffset, yOffset, zOffset,
                                 width, height, depth, &imageInfo)) {
    return;
  }
  MOZ_ASSERT(imageInfo);

  auto dstUsage = imageInfo->mFormat;
  MOZ_ASSERT(dstUsage);

  auto dstFormat = dstUsage->format;
  if (!mContext->IsWebGL2() && dstFormat->d) {
    mContext->ErrorInvalidOperation(
        "Function may not be called on a texture of"
        " format %s.",
        dstFormat->name);
    return;
  }

  ////////////////////////////////////
  // Get source info

  const webgl::FormatUsageInfo* srcUsage;
  uint32_t srcTotalWidth;
  uint32_t srcTotalHeight;
  if (!mContext->BindCurFBForColorRead(&srcUsage, &srcTotalWidth,
                                       &srcTotalHeight)) {
    return;
  }

  if (!ValidateCopyTexImageForFeedback(level, zOffset)) return;

  ////////////////////////////////////
  // Check that source and dest info are compatible

  auto srcFormat = srcUsage->format;
  if (!ValidateCopyTexImageFormats(mContext, srcFormat, dstFormat)) return;

  ////////////////////////////////////
  // Do the thing!

  if (!EnsureImageDataInitializedForUpload(this, target, level, xOffset,
                                           yOffset, zOffset, width, height,
                                           depth, imageInfo)) {
    return;
  }

  const bool isSubImage = true;
  if (!DoCopyTexOrSubImage(mContext, isSubImage, this, target, level, x, y,
                           srcTotalWidth, srcTotalHeight, srcUsage, xOffset,
                           yOffset, zOffset, width, height, dstUsage)) {
    return;
  }

  ////////////////////////////////////
  // Update our specification data?

  imageInfo->mHasData = true;
}

}  // namespace mozilla
