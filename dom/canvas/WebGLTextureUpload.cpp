/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTextureUpload.h"
#include "WebGLTexture.h"

#include <algorithm>
#include <limits>

#include "CanvasUtils.h"
#include "ClientWebGLContext.h"
#include "GLBlitHelper.h"
#include "GLContext.h"
#include "mozilla/Casting.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/OffscreenCanvas.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Scoped.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "mozilla/Unused.h"
#include "nsLayoutUtils.h"
#include "ScopedGLHelpers.h"
#include "TexUnpackBlob.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLTexelConversions.h"

namespace mozilla {
namespace webgl {

// The canvas spec says that drawImage should draw the first frame of
// animated images. The webgl spec doesn't mention the issue, so we do the
// same as drawImage.
static constexpr uint32_t kDefaultSurfaceFromElementFlags =
    nsLayoutUtils::SFE_WANT_FIRST_FRAME_IF_IMAGE |
    nsLayoutUtils::SFE_USE_ELEMENT_SIZE_IF_VECTOR |
    nsLayoutUtils::SFE_EXACT_SIZE_SURFACE |
    nsLayoutUtils::SFE_ALLOW_NON_PREMULT;

Maybe<TexUnpackBlobDesc> FromImageBitmap(const GLenum target, Maybe<uvec3> size,
                                         const dom::ImageBitmap& imageBitmap,
                                         ErrorResult* const out_rv) {
  if (imageBitmap.IsWriteOnly()) {
    out_rv->Throw(NS_ERROR_DOM_SECURITY_ERR);
    return {};
  }

  const auto cloneData = imageBitmap.ToCloneData();
  if (!cloneData) {
    return {};
  }

  const RefPtr<gfx::DataSourceSurface> surf = cloneData->mSurface;
  const auto imageSize = *uvec2::FromSize(surf->GetSize());
  if (!size) {
    size.emplace(imageSize.x, imageSize.y, 1);
  }

  // WhatWG "HTML Living Standard" (30 October 2015):
  // "The getImageData(sx, sy, sw, sh) method [...] Pixels must be returned as
  // non-premultiplied alpha values."
  return Some(TexUnpackBlobDesc{target,
                                size.value(),
                                cloneData->mAlphaType,
                                {},
                                {},
                                Some(imageSize),
                                nullptr,
                                {},
                                surf,
                                {},
                                false});
}

static layers::SurfaceDescriptor Flatten(const layers::SurfaceDescriptor& sd) {
  const auto sdType = sd.type();
  if (sdType != layers::SurfaceDescriptor::TSurfaceDescriptorGPUVideo) {
    return sd;
  }
  const auto& sdv = sd.get_SurfaceDescriptorGPUVideo();
  const auto& sdvType = sdv.type();
  if (sdvType !=
      layers::SurfaceDescriptorGPUVideo::TSurfaceDescriptorRemoteDecoder) {
    return sd;
  }

  const auto& sdrd = sdv.get_SurfaceDescriptorRemoteDecoder();
  const auto& subdesc = sdrd.subdesc();
  const auto& subdescType = subdesc.type();
  switch (subdescType) {
    case layers::RemoteDecoderVideoSubDescriptor::T__None:
    case layers::RemoteDecoderVideoSubDescriptor::Tnull_t:
      return sd;

    case layers::RemoteDecoderVideoSubDescriptor::TSurfaceDescriptorD3D10:
      return subdesc.get_SurfaceDescriptorD3D10();
    case layers::RemoteDecoderVideoSubDescriptor::TSurfaceDescriptorDXGIYCbCr:
      return subdesc.get_SurfaceDescriptorDXGIYCbCr();
    case layers::RemoteDecoderVideoSubDescriptor::TSurfaceDescriptorDMABuf:
      return subdesc.get_SurfaceDescriptorDMABuf();
    case layers::RemoteDecoderVideoSubDescriptor::
        TSurfaceDescriptorMacIOSurface:
      return subdesc.get_SurfaceDescriptorMacIOSurface();
    case layers::RemoteDecoderVideoSubDescriptor::
        TSurfaceDescriptorDcompSurface:
      return subdesc.get_SurfaceDescriptorDcompSurface();
  }
  MOZ_CRASH("unreachable");
}

Maybe<webgl::TexUnpackBlobDesc> FromOffscreenCanvas(
    const ClientWebGLContext& webgl, const GLenum target, Maybe<uvec3> size,
    const dom::OffscreenCanvas& canvas, ErrorResult* const out_error) {
  if (canvas.IsWriteOnly()) {
    webgl.EnqueueWarning(
        "OffscreenCanvas is write-only, thus cannot be uploaded.");
    out_error->ThrowSecurityError(
        "OffscreenCanvas is write-only, thus cannot be uploaded.");
    return {};
  }

  auto sfer = nsLayoutUtils::SurfaceFromOffscreenCanvas(
      const_cast<dom::OffscreenCanvas*>(&canvas),
      kDefaultSurfaceFromElementFlags);
  return FromSurfaceFromElementResult(webgl, target, size, sfer, out_error);
}

Maybe<webgl::TexUnpackBlobDesc> FromVideoFrame(
    const ClientWebGLContext& webgl, const GLenum target, Maybe<uvec3> size,
    const dom::VideoFrame& videoFrame, ErrorResult* const out_error) {
  auto sfer = nsLayoutUtils::SurfaceFromVideoFrame(
      const_cast<dom::VideoFrame*>(&videoFrame),
      kDefaultSurfaceFromElementFlags);
  return FromSurfaceFromElementResult(webgl, target, size, sfer, out_error);
}

Maybe<webgl::TexUnpackBlobDesc> FromDomElem(const ClientWebGLContext& webgl,
                                            const GLenum target,
                                            Maybe<uvec3> size,
                                            const dom::Element& elem,
                                            ErrorResult* const out_error) {
  if (elem.IsHTMLElement(nsGkAtoms::canvas)) {
    const dom::HTMLCanvasElement* srcCanvas =
        static_cast<const dom::HTMLCanvasElement*>(&elem);
    if (srcCanvas->IsWriteOnly()) {
      out_error->Throw(NS_ERROR_DOM_SECURITY_ERR);
      return {};
    }
  }

  uint32_t flags = kDefaultSurfaceFromElementFlags;
  const auto& unpacking = webgl.State().mPixelUnpackState;
  if (unpacking.colorspaceConversion == LOCAL_GL_NONE) {
    flags |= nsLayoutUtils::SFE_NO_COLORSPACE_CONVERSION;
  }

  RefPtr<gfx::DrawTarget> idealDrawTarget = nullptr;  // Don't care for now.
  auto sfer = nsLayoutUtils::SurfaceFromElement(
      const_cast<dom::Element*>(&elem), flags, idealDrawTarget);
  return FromSurfaceFromElementResult(webgl, target, size, sfer, out_error);
}

Maybe<webgl::TexUnpackBlobDesc> FromSurfaceFromElementResult(
    const ClientWebGLContext& webgl, const GLenum target, Maybe<uvec3> size,
    SurfaceFromElementResult& sfer, ErrorResult* const out_error) {
  uvec2 elemSize;

  const auto& layersImage = sfer.mLayersImage;
  Maybe<layers::SurfaceDescriptor> sd;
  if (layersImage) {
    elemSize = *uvec2::FromSize(layersImage->GetSize());

    sd = layersImage->GetDesc();
    if (sd) {
      sd = Some(Flatten(*sd));
    }
    if (!sd) {
      NS_WARNING("No SurfaceDescriptor for layers::Image!");
    }
  }

  RefPtr<gfx::DataSourceSurface> dataSurf;
  if (!sd && sfer.GetSourceSurface()) {
    const auto surf = sfer.GetSourceSurface();
    elemSize = *uvec2::FromSize(surf->GetSize());

    // WARNING: OSX can lose our MakeCurrent here.
    dataSurf = surf->GetDataSurface();
  }

  //////

  if (!size) {
    size.emplace(elemSize.x, elemSize.y, 1);
  }

  ////

  if (!sd && !dataSurf) {
    webgl.EnqueueWarning("Resource has no data (yet?). Uploading zeros.");
    if (!size) {
      size.emplace(0, 0, 1);
    }
    return Some(
        TexUnpackBlobDesc{target, size.value(), gfxAlphaType::NonPremult});
  }

  //////

  // While it's counter-intuitive, the shape of the SFEResult API means that we
  // should try to pull out a surface first, and then, if we do pull out a
  // surface, check CORS/write-only/etc..

  if (!sfer.mCORSUsed) {
    auto& srcPrincipal = sfer.mPrincipal;
    nsIPrincipal* dstPrincipal = webgl.PrincipalOrNull();
    if (!dstPrincipal || !dstPrincipal->Subsumes(srcPrincipal)) {
      webgl.EnqueueWarning("Cross-origin elements require CORS.");
      out_error->Throw(NS_ERROR_DOM_SECURITY_ERR);
      return {};
    }
  }

  if (sfer.mIsWriteOnly) {
    // mIsWriteOnly defaults to true, and so will be true even if SFE merely
    // failed. Thus we must test mIsWriteOnly after successfully retrieving an
    // Image or SourceSurface.
    webgl.EnqueueWarning("Element is write-only, thus cannot be uploaded.");
    out_error->Throw(NS_ERROR_DOM_SECURITY_ERR);
    return {};
  }

  //////
  // Ok, we're good!

  return Some(TexUnpackBlobDesc{target,
                                size.value(),
                                sfer.mAlphaType,
                                {},
                                {},
                                Some(elemSize),
                                layersImage,
                                sd,
                                dataSurf});
}

}  // namespace webgl

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

static bool ValidateTexImage(WebGLContext* webgl, WebGLTexture* texture,
                             TexImageTarget target, uint32_t level,
                             webgl::ImageInfo** const out_imageInfo) {
  // Check level
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
    TexImageTarget target, uint32_t level, const uvec3& size,
    webgl::ImageInfo** const out_imageInfo) {
  if (mImmutable) {
    mContext->ErrorInvalidOperation("Specified texture is immutable.");
    return false;
  }

  // Do this early to validate `level`.
  webgl::ImageInfo* imageInfo;
  if (!ValidateTexImage(mContext, this, target, level, &imageInfo))
    return false;

  if (mTarget == LOCAL_GL_TEXTURE_CUBE_MAP && size.x != size.y) {
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
   * resource corruption. Therefore we have limits.maxTex2dSize, which is
   * neither necessarily lower nor higher than MAX_TEXTURE_SIZE.
   *
   * Note that limits.maxTex2dSize must be >= than the advertized
   * MAX_TEXTURE_SIZE. For simplicity, we advertize MAX_TEXTURE_SIZE as
   * limits.maxTex2dSize.
   */

  uint32_t maxWidthHeight = 0;
  uint32_t maxDepth = 0;
  uint32_t maxLevel = 0;

  const auto& limits = mContext->Limits();
  MOZ_ASSERT(level <= 31);
  switch (target.get()) {
    case LOCAL_GL_TEXTURE_2D:
      maxWidthHeight = limits.maxTex2dSize >> level;
      maxDepth = 1;
      maxLevel = CeilingLog2(limits.maxTex2dSize);
      break;

    case LOCAL_GL_TEXTURE_3D:
      maxWidthHeight = limits.maxTex3dSize >> level;
      maxDepth = maxWidthHeight;
      maxLevel = CeilingLog2(limits.maxTex3dSize);
      break;

    case LOCAL_GL_TEXTURE_2D_ARRAY:
      maxWidthHeight = limits.maxTex2dSize >> level;
      // "The maximum number of layers for two-dimensional array textures
      // (depth) must be at least MAX_ARRAY_TEXTURE_LAYERS for all levels."
      maxDepth = limits.maxTexArrayLayers;
      maxLevel = CeilingLog2(limits.maxTex2dSize);
      break;

    default:  // cube maps
      MOZ_ASSERT(IsCubeMap());
      maxWidthHeight = limits.maxTexCubeSize >> level;
      maxDepth = 1;
      maxLevel = CeilingLog2(limits.maxTexCubeSize);
      break;
  }

  if (level > maxLevel) {
    mContext->ErrorInvalidValue("Requested level is not supported for target.");
    return false;
  }

  if (size.x > maxWidthHeight || size.y > maxWidthHeight || size.z > maxDepth) {
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
      if (!IsPowerOfTwo(size.x) || !IsPowerOfTwo(size.y)) {
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
    TexImageTarget target, uint32_t level, const uvec3& offset,
    const uvec3& size, webgl::ImageInfo** const out_imageInfo) {
  webgl::ImageInfo* imageInfo;
  if (!ValidateTexImage(mContext, this, target, level, &imageInfo))
    return false;

  if (!imageInfo->IsDefined()) {
    mContext->ErrorInvalidOperation(
        "The specified TexImage has not yet been"
        " specified.");
    return false;
  }

  const auto totalX = CheckedUint32(offset.x) + size.x;
  const auto totalY = CheckedUint32(offset.y) + size.y;
  const auto totalZ = CheckedUint32(offset.z) + size.z;

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

static bool ValidateCompressedTexUnpack(WebGLContext* webgl, const uvec3& size,
                                        const webgl::FormatInfo* format,
                                        size_t dataSize) {
  auto compression = format->compression;

  auto bytesPerBlock = compression->bytesPerBlock;
  auto blockWidth = compression->blockWidth;
  auto blockHeight = compression->blockHeight;

  auto widthInBlocks = CheckedUint32(size.x) / blockWidth;
  auto heightInBlocks = CheckedUint32(size.y) / blockHeight;
  if (size.x % blockWidth) widthInBlocks += 1;
  if (size.y % blockHeight) heightInBlocks += 1;

  const CheckedUint32 blocksPerImage = widthInBlocks * heightInBlocks;
  const CheckedUint32 bytesPerImage = bytesPerBlock * blocksPerImage;
  const CheckedUint32 bytesNeeded = bytesPerImage * size.z;

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
    WebGLTexture* tex, TexImageTarget target, uint32_t level,
    const uvec3& offset, const uvec3& size, webgl::ImageInfo* imageInfo,
    bool* const out_expectsInit = nullptr) {
  if (out_expectsInit) {
    *out_expectsInit = false;
  }
  if (!imageInfo->mUninitializedSlices) return true;

  if (size.x == imageInfo->mWidth && size.y == imageInfo->mHeight) {
    bool expectsInit = false;
    auto& isSliceUninit = *imageInfo->mUninitializedSlices;
    for (const auto z : IntegerRange(offset.z, offset.z + size.z)) {
      if (!isSliceUninit[z]) continue;
      expectsInit = true;
      isSliceUninit[z] = false;
    }
    if (out_expectsInit) {
      *out_expectsInit = expectsInit;
    }

    if (!expectsInit) return true;

    bool hasUninitialized = false;
    for (const auto z : IntegerRange(imageInfo->mDepth)) {
      hasUninitialized |= isSliceUninit[z];
    }
    if (!hasUninitialized) {
      imageInfo->mUninitializedSlices = Nothing();
    }
    return true;
  }

  WebGLContext* webgl = tex->mContext;
  webgl->GenerateWarning(
      "Texture has not been initialized prior to a"
      " partial upload, forcing the browser to clear it."
      " This may be slow.");
  if (!tex->EnsureImageDataInitialized(target, level)) {
    MOZ_ASSERT(false, "Unexpected failure to init image data.");
    return false;
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
    const WebGLContext* webgl, TexImageTarget target, uint32_t level,
    const webgl::FormatInfo* format, const uvec3& size) {
  const auto fnIsDimValid_S3TC = [&](const char* const name, uint32_t levelSize,
                                     uint32_t blockSize) {
    const auto impliedBaseSize = levelSize << level;
    if (impliedBaseSize % blockSize == 0) return true;
    webgl->ErrorInvalidOperation(
        "%u is never a valid %s for level %u, because it implies a base mip %s "
        "of %u."
        " %s requires that base mip levels have a %s multiple of %u.",
        levelSize, name, level, name, impliedBaseSize, format->name, name,
        blockSize);
    return false;
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
      if (!IsPowerOfTwo(size.x) || !IsPowerOfTwo(size.y)) {
        webgl->ErrorInvalidValue("%s requires power-of-two width and height.",
                                 format->name);
        return false;
      }
      break;

    case webgl::CompressionFamily::BPTC:
    case webgl::CompressionFamily::RGTC:
    case webgl::CompressionFamily::S3TC:
      if (!fnIsDimValid_S3TC("width", size.x,
                             format->compression->blockWidth) ||
          !fnIsDimValid_S3TC("height", size.y,
                             format->compression->blockHeight)) {
        return false;
      }
      break;

    // Default: There are no restrictions on CompressedTexImage.
    case webgl::CompressionFamily::ES3:
    case webgl::CompressionFamily::ETC1:
      break;
  }

  return true;
}

static bool ValidateFormatAndSize(const WebGLContext* webgl,
                                  TexImageTarget target,
                                  const webgl::FormatInfo* format,
                                  const uvec3& size) {
  // Check if texture size will likely be rejected by the driver and give a more
  // meaningful error message.
  auto baseImageSize = CheckedInt<uint64_t>(format->estimatedBytesPerPixel) *
                       (uint32_t)size.x * (uint32_t)size.y * (uint32_t)size.z;
  if (target == LOCAL_GL_TEXTURE_CUBE_MAP) {
    baseImageSize *= 6;
  }
  if (!baseImageSize.isValid() ||
      baseImageSize.value() >
          (uint64_t)StaticPrefs::webgl_max_size_per_texture_mib() *
              (1024 * 1024)) {
    webgl->ErrorOutOfMemory(
        "Texture size too large; base image mebibytes > "
        "webgl.max-size-per-texture-mib");
    return false;
  }

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

void WebGLTexture::TexStorage(TexTarget target, uint32_t levels,
                              GLenum sizedFormat, const uvec3& size) {
  // Check levels
  if (levels < 1) {
    mContext->ErrorInvalidValue("`levels` must be >= 1.");
    return;
  }

  if (!size.x || !size.y || !size.z) {
    mContext->ErrorInvalidValue("Dimensions must be non-zero.");
    return;
  }

  const TexImageTarget testTarget =
      IsCubeMap() ? LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X : target.get();
  webgl::ImageInfo* baseImageInfo;
  if (!ValidateTexImageSpecification(testTarget, 0, size, &baseImageInfo)) {
    return;
  }
  MOZ_ALWAYS_TRUE(baseImageInfo);

  auto dstUsage = mContext->mFormatUsage->GetSizedTexUsage(sizedFormat);
  if (!dstUsage) {
    mContext->ErrorInvalidEnumInfo("internalformat", sizedFormat);
    return;
  }
  auto dstFormat = dstUsage->format;

  if (!ValidateFormatAndSize(mContext, testTarget, dstFormat, size)) return;

  if (dstFormat->compression) {
    if (!ValidateCompressedTexImageRestrictions(mContext, testTarget, 0,
                                                dstFormat, size)) {
      return;
    }
  }

  ////////////////////////////////////

  const bool levelsOk = [&]() {
    // Right-shift is only defined for bits-1, which is too large anyways.
    const auto lastLevel = uint32_t(levels - 1);
    if (lastLevel > 31) return false;

    const auto lastLevelWidth = uint32_t(size.x) >> lastLevel;
    const auto lastLevelHeight = uint32_t(size.y) >> lastLevel;

    // If these are all zero, then some earlier level was the final 1x1(x1)
    // level.
    bool ok = lastLevelWidth || lastLevelHeight;
    if (target == LOCAL_GL_TEXTURE_3D) {
      const auto lastLevelDepth = uint32_t(size.z) >> lastLevel;
      ok |= bool(lastLevelDepth);
    }
    return ok;
  }();
  if (!levelsOk) {
    mContext->ErrorInvalidOperation(
        "Too many levels requested for the given"
        " dimensions. (levels: %u, width: %u, height: %u,"
        " depth: %u)",
        levels, size.x, size.y, size.z);
    return;
  }

  ////////////////////////////////////
  // Do the thing!

  GLenum error = DoTexStorage(mContext->gl, target.get(), levels, sizedFormat,
                              size.x, size.y, size.z);

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
        levels, sizedFormat, size.x, size.y, size.z, error);
    gfxCriticalError() << "Unexpected error from driver: "
                       << call.BeginReading();
    return;
  }

  ////////////////////////////////////
  // Update our specification data.

  auto uninitializedSlices = Some(std::vector<bool>(size.z, true));
  const webgl::ImageInfo newInfo{dstUsage, size.x, size.y, size.z,
                                 std::move(uninitializedSlices)};

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

// TexSubImage iff `!respectFormat`
void WebGLTexture::TexImage(uint32_t level, GLenum respecFormat,
                            const uvec3& offset, const webgl::PackingInfo& pi,
                            const webgl::TexUnpackBlobDesc& src) {
  Maybe<RawBuffer<>> cpuDataView;
  if (src.cpuData) {
    cpuDataView = Some(RawBuffer<>{src.cpuData->Data()});
  }
  const auto srcViewDesc = webgl::TexUnpackBlobDesc{src.imageTarget,
                                                    src.size,
                                                    src.srcAlphaType,
                                                    std::move(cpuDataView),
                                                    src.pboOffset,
                                                    src.structuredSrcSize,
                                                    src.image,
                                                    src.sd,
                                                    src.dataSurf,
                                                    src.unpacking,
                                                    src.applyUnpackTransforms};

  const auto blob = webgl::TexUnpackBlob::Create(srcViewDesc);
  if (!blob) {
    MOZ_ASSERT(false);
    return;
  }

  const auto imageTarget = blob->mDesc.imageTarget;
  auto size = blob->mDesc.size;

  if (!IsTarget3D(imageTarget)) {
    size.z = 1;
  }

  ////////////////////////////////////
  // Get dest info

  const auto& fua = mContext->mFormatUsage;
  const auto fnValidateUnpackEnums = [&]() {
    if (!fua->AreUnpackEnumsValid(pi.format, pi.type)) {
      mContext->ErrorInvalidEnum("Invalid unpack format/type: %s/%s",
                                 EnumString(pi.format).c_str(),
                                 EnumString(pi.type).c_str());
      return false;
    }
    return true;
  };

  webgl::ImageInfo* imageInfo;
  const webgl::FormatUsageInfo* dstUsage;
  if (respecFormat) {
    if (!ValidateTexImageSpecification(imageTarget, level, size, &imageInfo))
      return;
    MOZ_ASSERT(imageInfo);

    if (!fua->IsInternalFormatEnumValid(respecFormat)) {
      mContext->ErrorInvalidValue("Invalid internalformat: 0x%04x",
                                  respecFormat);
      return;
    }

    dstUsage = fua->GetSizedTexUsage(respecFormat);
    if (!dstUsage) {
      if (respecFormat != pi.format) {
        /* GL ES Version 3.0.4 - 3.8.3 Texture Image Specification
         *   "Specifying a combination of values for format, type, and
         *   internalformat that is not listed as a valid combination
         *   in tables 3.2 or 3.3 generates the error INVALID_OPERATION."
         */
        if (!fnValidateUnpackEnums()) return;
        mContext->ErrorInvalidOperation(
            "Unsized internalFormat must match"
            " unpack format.");
        return;
      }

      dstUsage = fua->GetUnsizedTexUsage(pi);
    }

    if (!dstUsage) {
      if (!fnValidateUnpackEnums()) return;
      mContext->ErrorInvalidOperation(
          "Invalid internalformat/format/type:"
          " 0x%04x/0x%04x/0x%04x",
          respecFormat, pi.format, pi.type);
      return;
    }

    const auto& dstFormat = dstUsage->format;
    if (!ValidateFormatAndSize(mContext, imageTarget, dstFormat, size)) return;

    if (!mContext->IsWebGL2() && dstFormat->d) {
      if (imageTarget != LOCAL_GL_TEXTURE_2D || blob->HasData() || level != 0) {
        mContext->ErrorInvalidOperation(
            "With format %s, this function may only"
            " be called with target=TEXTURE_2D,"
            " data=null, and level=0.",
            dstFormat->name);
        return;
      }
    }
  } else {
    if (!ValidateTexImageSelection(imageTarget, level, offset, size,
                                   &imageInfo)) {
      return;
    }
    MOZ_ASSERT(imageInfo);
    dstUsage = imageInfo->mFormat;

    const auto& dstFormat = dstUsage->format;
    if (!mContext->IsWebGL2() && dstFormat->d) {
      mContext->ErrorInvalidOperation(
          "Function may not be called on a texture of"
          " format %s.",
          dstFormat->name);
      return;
    }
  }

  ////////////////////////////////////
  // Get source info

  const webgl::DriverUnpackInfo* driverUnpackInfo;
  if (!dstUsage->IsUnpackValid(pi, &driverUnpackInfo)) {
    if (!fnValidateUnpackEnums()) return;
    mContext->ErrorInvalidOperation(
        "Mismatched internalFormat and format/type:"
        " 0x%04x and 0x%04x/0x%04x",
        respecFormat, pi.format, pi.type);
    return;
  }

  if (!blob->Validate(mContext, pi)) return;

  ////////////////////////////////////
  // Do the thing!

  Maybe<webgl::ImageInfo> newImageInfo;
  bool isRespec = false;
  if (respecFormat) {
    // It's tempting to do allocation first, and TexSubImage second, but this is
    // generally slower.
    newImageInfo = Some(webgl::ImageInfo{dstUsage, size.x, size.y, size.z});
    if (!blob->HasData()) {
      newImageInfo->mUninitializedSlices =
          Some(std::vector<bool>(size.z, true));
    }

    isRespec = (imageInfo->mWidth != newImageInfo->mWidth ||
                imageInfo->mHeight != newImageInfo->mHeight ||
                imageInfo->mDepth != newImageInfo->mDepth ||
                imageInfo->mFormat != newImageInfo->mFormat);
  } else {
    if (!blob->HasData()) {
      mContext->ErrorInvalidValue("`source` cannot be null.");
      return;
    }
    if (!EnsureImageDataInitializedForUpload(this, imageTarget, level, offset,
                                             size, imageInfo)) {
      return;
    }
  }

  webgl::PixelPackingState{}.AssertCurrentUnpack(*mContext->gl,
                                                 mContext->IsWebGL2());

  blob->mDesc.unpacking.ApplyUnpack(*mContext->gl, mContext->IsWebGL2(), size);
  const auto revertUnpacking = MakeScopeExit([&]() {
    webgl::PixelPackingState{}.ApplyUnpack(*mContext->gl, mContext->IsWebGL2(),
                                           size);
  });

  const bool isSubImage = !respecFormat;
  GLenum glError = 0;
  if (!blob->TexOrSubImage(isSubImage, isRespec, this, level, driverUnpackInfo,
                           offset.x, offset.y, offset.z, pi, &glError)) {
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

  if (respecFormat) {
    mContext->OnDataAllocCall();
    *imageInfo = *newImageInfo;
    InvalidateCaches();
  }
}

////////////////////////////////////////
// CompressedTex(Sub)Image

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

// CompressedTexSubImage iff `sub`
void WebGLTexture::CompressedTexImage(bool sub, GLenum imageTarget,
                                      uint32_t level, GLenum formatEnum,
                                      const uvec3& offset, const uvec3& size,
                                      const Range<const uint8_t>& src,
                                      const uint32_t pboImageSize,
                                      const Maybe<uint64_t>& pboOffset) {
  auto imageSize = pboImageSize;
  if (pboOffset) {
    const auto& buffer =
        mContext->ValidateBufferSelection(LOCAL_GL_PIXEL_UNPACK_BUFFER);
    if (!buffer) return;
    auto availBytes = buffer->ByteLength();
    if (*pboOffset > availBytes) {
      mContext->GenerateError(
          LOCAL_GL_INVALID_OPERATION,
          "`offset` (%llu) must be <= PIXEL_UNPACK_BUFFER size (%llu).",
          *pboOffset, availBytes);
      return;
    }
    availBytes -= *pboOffset;
    if (availBytes < pboImageSize) {
      mContext->GenerateError(
          LOCAL_GL_INVALID_OPERATION,
          "PIXEL_UNPACK_BUFFER size minus `offset` (%llu) too small for"
          " `pboImageSize` (%u).",
          availBytes, pboImageSize);
      return;
    }
  } else {
    if (mContext->mBoundPixelUnpackBuffer) {
      mContext->GenerateError(LOCAL_GL_INVALID_OPERATION,
                              "PIXEL_UNPACK_BUFFER is non-null.");
      return;
    }
    imageSize = src.length();
  }

  // -

  const auto usage = mContext->mFormatUsage->GetSizedTexUsage(formatEnum);
  if (!usage || !usage->format->compression) {
    mContext->ErrorInvalidEnumArg("format", formatEnum);
    return;
  }

  webgl::ImageInfo* imageInfo;
  if (!sub) {
    if (!ValidateTexImageSpecification(imageTarget, level, size, &imageInfo)) {
      return;
    }
    MOZ_ASSERT(imageInfo);

    if (!ValidateFormatAndSize(mContext, imageTarget, usage->format, size))
      return;
    if (!ValidateCompressedTexImageRestrictions(mContext, imageTarget, level,
                                                usage->format, size)) {
      return;
    }
  } else {
    if (!ValidateTexImageSelection(imageTarget, level, offset, size,
                                   &imageInfo))
      return;
    MOZ_ASSERT(imageInfo);

    const auto dstUsage = imageInfo->mFormat;
    if (usage != dstUsage) {
      mContext->ErrorInvalidOperation(
          "`format` must match the format of the"
          " existing texture image.");
      return;
    }

    const auto& format = usage->format;
    switch (format->compression->family) {
      // Forbidden:
      case webgl::CompressionFamily::ETC1:
        mContext->ErrorInvalidOperation(
            "Format does not allow sub-image"
            " updates.");
        return;

      // Block-aligned:
      case webgl::CompressionFamily::ES3:   // Yes, the ES3 formats don't match
                                            // the ES3
      case webgl::CompressionFamily::S3TC:  // default behavior.
      case webgl::CompressionFamily::BPTC:
      case webgl::CompressionFamily::RGTC:
        if (!IsSubImageBlockAligned(format->compression, imageInfo, offset.x,
                                    offset.y, size.x, size.y)) {
          mContext->ErrorInvalidOperation(
              "Format requires block-aligned sub-image"
              " updates.");
          return;
        }
        break;

      // Full-only: (The ES3 default)
      case webgl::CompressionFamily::ASTC:
      case webgl::CompressionFamily::PVRTC:
        if (offset.x || offset.y || size.x != imageInfo->mWidth ||
            size.y != imageInfo->mHeight) {
          mContext->ErrorInvalidOperation(
              "Format does not allow partial sub-image"
              " updates.");
          return;
        }
        break;
    }
  }

  switch (usage->format->compression->family) {
    case webgl::CompressionFamily::BPTC:
    case webgl::CompressionFamily::RGTC:
      if (level == 0) {
        if (size.x % 4 != 0 || size.y % 4 != 0) {
          mContext->ErrorInvalidOperation(
              "For level == 0, width and height must be multiples of 4.");
          return;
        }
      }
      break;

    default:
      break;
  }

  if (!ValidateCompressedTexUnpack(mContext, size, usage->format, imageSize))
    return;

  ////////////////////////////////////
  // Do the thing!

  if (sub) {
    if (!EnsureImageDataInitializedForUpload(this, imageTarget, level, offset,
                                             size, imageInfo)) {
      return;
    }
  }

  const ScopedLazyBind bindPBO(mContext->gl, LOCAL_GL_PIXEL_UNPACK_BUFFER,
                               mContext->mBoundPixelUnpackBuffer);
  GLenum error;
  const void* ptr;
  if (pboOffset) {
    ptr = reinterpret_cast<const void*>(*pboOffset);
  } else {
    ptr = reinterpret_cast<const void*>(src.begin().get());
  }

  if (!sub) {
    error = DoCompressedTexImage(mContext->gl, imageTarget, level, formatEnum,
                                 size.x, size.y, size.z, imageSize, ptr);
  } else {
    error = DoCompressedTexSubImage(mContext->gl, imageTarget, level, offset.x,
                                    offset.y, offset.z, size.x, size.y, size.z,
                                    formatEnum, imageSize, ptr);
  }
  if (error == LOCAL_GL_OUT_OF_MEMORY) {
    mContext->ErrorOutOfMemory("Ran out of memory during upload.");
    Truncate();
    return;
  }
  if (error) {
    mContext->GenerateError(error, "Unexpected error from driver.");
    nsCString call;
    if (!sub) {
      call = nsPrintfCString(
          "DoCompressedTexImage(0x%04x, %u, 0x%04x, %u,%u,%u, %u, %p)",
          imageTarget, level, formatEnum, size.x, size.y, size.z, imageSize,
          ptr);
    } else {
      call = nsPrintfCString(
          "DoCompressedTexSubImage(0x%04x, %u, %u,%u,%u, %u,%u,%u, 0x%04x, %u, "
          "%p)",
          imageTarget, level, offset.x, offset.y, offset.z, size.x, size.y,
          size.z, formatEnum, imageSize, ptr);
    }
    gfxCriticalError() << "Unexpected error " << gfx::hexa(error)
                       << " from driver: " << call.BeginReading();
    return;
  }

  ////////////////////////////////////
  // Update our specification data?

  if (!sub) {
    const auto uninitializedSlices = Nothing();
    const webgl::ImageInfo newImageInfo{usage, size.x, size.y, size.z,
                                        uninitializedSlices};
    *imageInfo = newImageInfo;
    InvalidateCaches();
  }
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
        webgl->WarnIfImplicit(WebGLExtensionID::WEBGL_color_buffer_float);
        break;
      }

      if (webgl->IsExtensionEnabled(
              WebGLExtensionID::EXT_color_buffer_half_float)) {
        sizedFormat = LOCAL_GL_RGBA16F;
        webgl->WarnIfImplicit(WebGLExtensionID::EXT_color_buffer_half_float);
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
  GLuint rgbaFB = 0;
  {
    gl->fGenRenderbuffers(1, &rgbaRB);
    gl::ScopedBindRenderbuffer scopedRB(gl, rgbaRB);
    gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, sizedFormat, srcWidth,
                             srcHeight);

    gl->fGenFramebuffers(1, &rgbaFB);
    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, rgbaFB);
    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_COLOR_ATTACHMENT0,
                                 LOCAL_GL_RENDERBUFFER, rgbaRB);

    const GLenum status = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
      MOZ_CRASH("GFX: Temp framebuffer is not complete.");
    }
  }

  // Draw-blit rgbaTex into rgbaFB.
  const gfx::IntSize srcSize(srcWidth, srcHeight);
  {
    const gl::ScopedBindFramebuffer bindFB(gl, rgbaFB);
    gl->BlitHelper()->DrawBlitTextureToFramebuffer(scopedTex.Texture(), srcSize,
                                                   srcSize);
  }

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

static bool ValidateCopyTexImageForFeedback(const WebGLContext& webgl,
                                            const WebGLTexture& tex,
                                            const uint32_t mipLevel,
                                            const uint32_t zLayer) {
  const auto& fb = webgl.BoundReadFb();
  if (fb) {
    MOZ_ASSERT(fb->ColorReadBuffer());
    const auto& attach = *fb->ColorReadBuffer();
    MOZ_ASSERT(attach.ZLayerCount() ==
               1);  // Multiview invalid for copyTexImage.

    if (attach.Texture() == &tex && attach.Layer() == zLayer &&
        attach.MipLevel() == mipLevel) {
      // Note that the TexImageTargets *don't* have to match for this to be
      // undefined per GLES 3.0.4 p211, thus an INVALID_OP in WebGL.
      webgl.ErrorInvalidOperation(
          "Feedback loop detected, as this texture"
          " is already attached to READ_FRAMEBUFFER's"
          " READ_BUFFER-selected COLOR_ATTACHMENT%u.",
          attach.mAttachmentPoint);
      return false;
    }
  }
  return true;
}

static bool DoCopyTexOrSubImage(WebGLContext* webgl, bool isSubImage,
                                bool needsInit, WebGLTexture* const tex,
                                const TexImageTarget target, GLint level,
                                GLint xWithinSrc, GLint yWithinSrc,
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
    const auto& pi = idealUnpack->ToPacking();

    UniqueBuffer zeros;
    const bool fullOverwrite =
        (uint32_t(rwWidth) == dstWidth && uint32_t(rwHeight) == dstHeight);
    if (needsInit && !fullOverwrite) {
      CheckedInt<size_t> byteCount = BytesPerPixel(pi);
      byteCount *= dstWidth;
      byteCount *= dstHeight;

      if (byteCount.isValid()) {
        zeros = UniqueBuffer::Take(calloc(1u, byteCount.value()));
      }

      if (!zeros.get()) {
        webgl->ErrorOutOfMemory("Ran out of memory allocating zeros.");
        return false;
      }
    }

    if (!isSubImage || zeros) {
      webgl::PixelPackingState{}.AssertCurrentUnpack(*gl, webgl->IsWebGL2());

      gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 1);
      const auto revert = MakeScopeExit(
          [&]() { gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4); });
      if (!isSubImage) {
        error = DoTexImage(gl, target, level, idealUnpack, dstWidth, dstHeight,
                           1, nullptr);
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
      if (zeros) {
        error = DoTexSubImage(gl, target, level, xOffset, yOffset, zOffset,
                              dstWidth, dstHeight, 1, pi, zeros.get());
        if (error) {
          errorText = nsPrintfCString(
              "DoTexSubImage(0x%04x, %i, %i,%i,%i, %u,%u,1, {0x%04x, 0x%04x}) "
              "-> "
              "0x%04x",
              target.get(), level, xOffset, yOffset, zOffset, dstWidth,
              dstHeight, idealUnpack->unpackFormat, idealUnpack->unpackType,
              error);
          break;
        }
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

// CopyTexSubImage if `!respecFormat`
void WebGLTexture::CopyTexImage(GLenum imageTarget, uint32_t level,
                                GLenum respecFormat, const uvec3& dstOffset,
                                const ivec2& srcOffset, const uvec2& size2) {
  ////////////////////////////////////
  // Get source info

  const webgl::FormatUsageInfo* srcUsage;
  uint32_t srcTotalWidth;
  uint32_t srcTotalHeight;
  if (!mContext->BindCurFBForColorRead(&srcUsage, &srcTotalWidth,
                                       &srcTotalHeight)) {
    return;
  }
  const auto& srcFormat = srcUsage->format;

  if (!ValidateCopyTexImageForFeedback(*mContext, *this, level, dstOffset.z))
    return;

  const auto size = uvec3{size2.x, size2.y, 1};

  ////////////////////////////////////
  // Get dest info

  webgl::ImageInfo* imageInfo;
  const webgl::FormatUsageInfo* dstUsage;
  if (respecFormat) {
    if (!ValidateTexImageSpecification(imageTarget, level, size, &imageInfo))
      return;
    MOZ_ASSERT(imageInfo);

    dstUsage = ValidateCopyDestUsage(mContext, srcFormat, respecFormat);
    if (!dstUsage) return;

    if (!ValidateFormatAndSize(mContext, imageTarget, dstUsage->format, size))
      return;
  } else {
    if (!ValidateTexImageSelection(imageTarget, level, dstOffset, size,
                                   &imageInfo)) {
      return;
    }
    MOZ_ASSERT(imageInfo);

    dstUsage = imageInfo->mFormat;
    MOZ_ASSERT(dstUsage);
  }

  const auto& dstFormat = dstUsage->format;
  if (!mContext->IsWebGL2() && dstFormat->d) {
    mContext->ErrorInvalidOperation(
        "Function may not be called with format %s.", dstFormat->name);
    return;
  }

  ////////////////////////////////////
  // Check that source and dest info are compatible

  if (!ValidateCopyTexImageFormats(mContext, srcFormat, dstFormat)) return;

  ////////////////////////////////////
  // Do the thing!

  const bool isSubImage = !respecFormat;
  bool expectsInit = true;
  if (isSubImage) {
    if (!EnsureImageDataInitializedForUpload(this, imageTarget, level,
                                             dstOffset, size, imageInfo,
                                             &expectsInit)) {
      return;
    }
  }

  if (!DoCopyTexOrSubImage(mContext, isSubImage, expectsInit, this, imageTarget,
                           level, srcOffset.x, srcOffset.y, srcTotalWidth,
                           srcTotalHeight, srcUsage, dstOffset.x, dstOffset.y,
                           dstOffset.z, size.x, size.y, dstUsage)) {
    Truncate();
    return;
  }

  mContext->OnDataAllocCall();

  ////////////////////////////////////
  // Update our specification data?

  if (respecFormat) {
    const auto uninitializedSlices = Nothing();
    const webgl::ImageInfo newImageInfo{dstUsage, size.x, size.y, size.z,
                                        uninitializedSlices};
    *imageInfo = newImageInfo;
    InvalidateCaches();
  }
}

}  // namespace mozilla
