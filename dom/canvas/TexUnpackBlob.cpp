/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TexUnpackBlob.h"

#include "GLBlitHelper.h"
#include "GLContext.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/RefPtr.h"
#include "nsLayoutUtils.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"
#include "WebGLTexelConversions.h"
#include "WebGLTexture.h"

namespace mozilla {

bool WebGLPixelStore::AssertCurrent(gl::GLContext& gl,
                                    const bool isWebgl2) const {
  WebGLPixelStore actual;
  gl.GetInt(LOCAL_GL_UNPACK_ALIGNMENT, &actual.mUnpackAlignment);
  if (isWebgl2) {
    gl.GetInt(LOCAL_GL_UNPACK_ROW_LENGTH, &actual.mUnpackRowLength);
    gl.GetInt(LOCAL_GL_UNPACK_IMAGE_HEIGHT, &actual.mUnpackImageHeight);

    gl.GetInt(LOCAL_GL_UNPACK_SKIP_PIXELS, &actual.mUnpackSkipPixels);
    gl.GetInt(LOCAL_GL_UNPACK_SKIP_ROWS, &actual.mUnpackSkipRows);
    gl.GetInt(LOCAL_GL_UNPACK_SKIP_IMAGES, &actual.mUnpackSkipImages);
  }

  bool ok = true;
  ok &= (mUnpackAlignment == actual.mUnpackAlignment);
  ok &= (mUnpackRowLength == actual.mUnpackRowLength);
  ok &= (mUnpackImageHeight == actual.mUnpackImageHeight);
  ok &= (mUnpackSkipPixels == actual.mUnpackSkipPixels);
  ok &= (mUnpackSkipRows == actual.mUnpackSkipRows);
  ok &= (mUnpackSkipImages == actual.mUnpackSkipImages);
  if (ok) return ok;

  const auto fnToStr = [](const WebGLPixelStore& x) {
    const auto text = nsPrintfCString("%u,%u,%u;%u,%u,%u", x.mUnpackAlignment,
                                      x.mUnpackRowLength, x.mUnpackImageHeight,
                                      x.mUnpackSkipPixels, x.mUnpackSkipRows,
                                      x.mUnpackSkipImages);
    return ToString(text);
  };

  const auto was = fnToStr(actual);
  const auto expected = fnToStr(*this);
  gfxCriticalError() << "WebGLPixelStore not current. Was " << was
                     << ". Expected << " << expected << ".";
  return ok;
}

void WebGLPixelStore::Apply(gl::GLContext& gl, const bool isWebgl2,
                            const uvec3& uploadSize) const {
  gl.fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, mUnpackAlignment);
  if (!isWebgl2) return;

  // Re-simplify. (ANGLE seems to have an issue with imageHeight ==
  // uploadSize.y)
  auto rowLength = mUnpackRowLength;
  auto imageHeight = mUnpackImageHeight;
  if (rowLength == uploadSize.x) {
    rowLength = 0;
  }
  if (imageHeight == uploadSize.y) {
    imageHeight = 0;
  }

  gl.fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, rowLength);
  gl.fPixelStorei(LOCAL_GL_UNPACK_IMAGE_HEIGHT, imageHeight);

  gl.fPixelStorei(LOCAL_GL_UNPACK_SKIP_PIXELS, mUnpackSkipPixels);
  gl.fPixelStorei(LOCAL_GL_UNPACK_SKIP_ROWS, mUnpackSkipRows);
  gl.fPixelStorei(LOCAL_GL_UNPACK_SKIP_IMAGES, mUnpackSkipImages);
}

namespace webgl {

static bool IsPIValidForDOM(const webgl::PackingInfo& pi) {
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

static bool ValidatePIForDOM(const WebGLContext* const webgl,
                             const webgl::PackingInfo& pi) {
  if (!IsPIValidForDOM(pi)) {
    webgl->ErrorInvalidValue("Format or type is invalid for DOM sources.");
    return false;
  }
  return true;
}

static WebGLTexelFormat FormatForPackingInfo(const PackingInfo& pi) {
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
      if (pi.format == LOCAL_GL_RGB) return WebGLTexelFormat::RGB565;
      break;

    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
      if (pi.format == LOCAL_GL_RGBA) return WebGLTexelFormat::RGBA5551;
      break;

    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
      if (pi.format == LOCAL_GL_RGBA) return WebGLTexelFormat::RGBA4444;
      break;

    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
      switch (pi.format) {
        case LOCAL_GL_RED:
        case LOCAL_GL_LUMINANCE:
          return WebGLTexelFormat::R16F;

        case LOCAL_GL_ALPHA:
          return WebGLTexelFormat::A16F;
        case LOCAL_GL_LUMINANCE_ALPHA:
          return WebGLTexelFormat::RA16F;
        case LOCAL_GL_RG:
          return WebGLTexelFormat::RG16F;
        case LOCAL_GL_RGB:
          return WebGLTexelFormat::RGB16F;
        case LOCAL_GL_RGBA:
          return WebGLTexelFormat::RGBA16F;

        default:
          break;
      }
      break;

    case LOCAL_GL_FLOAT:
      switch (pi.format) {
        case LOCAL_GL_RED:
        case LOCAL_GL_LUMINANCE:
          return WebGLTexelFormat::R32F;

        case LOCAL_GL_ALPHA:
          return WebGLTexelFormat::A32F;
        case LOCAL_GL_LUMINANCE_ALPHA:
          return WebGLTexelFormat::RA32F;
        case LOCAL_GL_RG:
          return WebGLTexelFormat::RG32F;
        case LOCAL_GL_RGB:
          return WebGLTexelFormat::RGB32F;
        case LOCAL_GL_RGBA:
          return WebGLTexelFormat::RGBA32F;

        default:
          break;
      }
      break;

    case LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV:
      if (pi.format == LOCAL_GL_RGB) return WebGLTexelFormat::RGB11F11F10F;
      break;

    default:
      break;
  }

  return WebGLTexelFormat::FormatNotSupportingAnyConversion;
}

////////////////////

static uint32_t ZeroOn2D(const GLenum target, const uint32_t val) {
  const bool is2d = !IsTexTarget3D(target);
  if (is2d) return 0;
  return val;
}

static bool ValidateUnpackPixels(const WebGLContext* webgl, uint32_t fullRows,
                                 uint32_t tailPixels,
                                 webgl::TexUnpackBlob* const blob) {
  const auto& size = blob->mDesc.size;
  if (!size.x || !size.y || !size.z) return true;

  const auto& unpacking = blob->mDesc.unpacking;

  // -

  const auto usedPixelsPerRow =
      CheckedUint32(unpacking.mUnpackSkipPixels) + size.x;
  if (!usedPixelsPerRow.isValid() ||
      usedPixelsPerRow.value() > unpacking.mUnpackRowLength) {
    webgl->ErrorInvalidOperation(
        "UNPACK_SKIP_PIXELS + width >"
        " UNPACK_ROW_LENGTH.");
    return false;
  }

  if (size.y > unpacking.mUnpackImageHeight) {
    webgl->ErrorInvalidOperation("height > UNPACK_IMAGE_HEIGHT.");
    return false;
  }

  //////

  // The spec doesn't bound SKIP_ROWS + height <= IMAGE_HEIGHT, unfortunately.
  auto skipFullRows =
      CheckedUint32(unpacking.mUnpackSkipImages) * unpacking.mUnpackImageHeight;
  skipFullRows += unpacking.mUnpackSkipRows;

  // Full rows in the final image, excluding the tail.
  MOZ_ASSERT(size.y >= 1);
  MOZ_ASSERT(size.z >= 1);
  auto usedFullRows = CheckedUint32(size.z - 1) * unpacking.mUnpackImageHeight;
  usedFullRows += size.y - 1;

  const auto fullRowsNeeded = skipFullRows + usedFullRows;
  if (!fullRowsNeeded.isValid()) {
    webgl->ErrorOutOfMemory("Invalid calculation for required row count.");
    return false;
  }

  if (fullRows > fullRowsNeeded.value()) {
    blob->mNeedsExactUpload = false;
    return true;
  }

  if (fullRows == fullRowsNeeded.value() &&
      tailPixels >= usedPixelsPerRow.value()) {
    MOZ_ASSERT(blob->mNeedsExactUpload);
    return true;
  }

  webgl->ErrorInvalidOperation(
      "Desired upload requires more data than is"
      " available: (%u rows plus %u pixels needed, %u rows"
      " plus %u pixels available)",
      fullRowsNeeded.value(), usedPixelsPerRow.value(), fullRows, tailPixels);
  return false;
}

static bool ValidateUnpackBytes(const WebGLContext* const webgl,
                                const webgl::PackingInfo& pi,
                                size_t availByteCount,
                                webgl::TexUnpackBlob* const blob) {
  const auto& size = blob->mDesc.size;
  if (!size.x || !size.y || !size.z) return true;
  const auto& unpacking = blob->mDesc.unpacking;

  const auto bytesPerPixel = webgl::BytesPerPixel(pi);
  const auto bytesPerRow =
      CheckedUint32(unpacking.mUnpackRowLength) * bytesPerPixel;
  const auto rowStride =
      RoundUpToMultipleOf(bytesPerRow, unpacking.mUnpackAlignment);

  const auto fullRows = availByteCount / rowStride;
  if (!fullRows.isValid()) {
    webgl->ErrorOutOfMemory("Unacceptable upload size calculated.");
    return false;
  }

  const auto bodyBytes = fullRows.value() * rowStride.value();
  const auto tailPixels = (availByteCount - bodyBytes) / bytesPerPixel;

  return ValidateUnpackPixels(webgl, fullRows.value(), tailPixels, blob);
}

////////////////////

// static
std::unique_ptr<TexUnpackBlob> TexUnpackBlob::Create(
    const TexUnpackBlobDesc& desc) {
  return std::unique_ptr<TexUnpackBlob>{[&]() -> TexUnpackBlob* {
    if (!IsTarget3D(desc.imageTarget) && desc.size.z != 1) {
      MOZ_ASSERT(false);
      return nullptr;
    }

    switch (desc.unpacking.mUnpackAlignment) {
      case 1:
      case 2:
      case 4:
      case 8:
        break;
      default:
        MOZ_ASSERT(false);
        return nullptr;
    }

    if (desc.sd) {
      return new TexUnpackImage(desc);
    }
    if (desc.dataSurf) {
      return new TexUnpackSurface(desc);
    }

    if (desc.srcAlphaType != gfxAlphaType::NonPremult) {
      MOZ_ASSERT(false);
      return nullptr;
    }
    return new TexUnpackBytes(desc);
  }()};
}

static bool HasColorAndAlpha(const WebGLTexelFormat format) {
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

bool TexUnpackBlob::ConvertIfNeeded(
    const WebGLContext* const webgl, const uint32_t rowLength,
    const uint32_t rowCount, WebGLTexelFormat srcFormat,
    const uint8_t* const srcBegin, const ptrdiff_t srcStride,
    WebGLTexelFormat dstFormat, const ptrdiff_t dstStride,
    const uint8_t** const out_begin,
    UniqueBuffer* const out_anchoredBuffer) const {
  MOZ_ASSERT(srcFormat != WebGLTexelFormat::FormatNotSupportingAnyConversion);
  MOZ_ASSERT(dstFormat != WebGLTexelFormat::FormatNotSupportingAnyConversion);

  *out_begin = srcBegin;

  const auto& unpacking = mDesc.unpacking;

  if (!rowLength || !rowCount) return true;

  const auto srcIsPremult = (mDesc.srcAlphaType == gfxAlphaType::Premult);
  const auto& dstIsPremult = unpacking.mPremultiplyAlpha;
  const auto fnHasPremultMismatch = [&]() {
    if (mDesc.srcAlphaType == gfxAlphaType::Opaque) return false;

    if (!HasColorAndAlpha(srcFormat)) return false;

    return srcIsPremult != dstIsPremult;
  };

  const auto srcOrigin =
      (unpacking.mFlipY ? gl::OriginPos::TopLeft : gl::OriginPos::BottomLeft);
  const auto dstOrigin = gl::OriginPos::BottomLeft;

  if (srcFormat != dstFormat) {
    webgl->GeneratePerfWarning(
        "Conversion requires pixel reformatting. (%u->%u)", uint32_t(srcFormat),
        uint32_t(dstFormat));
  } else if (fnHasPremultMismatch()) {
    webgl->GeneratePerfWarning(
        "Conversion requires change in"
        " alpha-premultiplication.");
  } else if (srcOrigin != dstOrigin) {
    webgl->GeneratePerfWarning("Conversion requires y-flip.");
  } else if (srcStride != dstStride) {
    webgl->GeneratePerfWarning("Conversion requires change in stride. (%u->%u)",
                               uint32_t(srcStride), uint32_t(dstStride));
  } else {
    return true;
  }

  ////

  const auto dstTotalBytes = CheckedUint32(rowCount) * dstStride;
  if (!dstTotalBytes.isValid()) {
    webgl->ErrorOutOfMemory("Calculation failed.");
    return false;
  }

  UniqueBuffer dstBuffer = calloc(1u, (size_t)dstTotalBytes.value());
  if (!dstBuffer.get()) {
    webgl->ErrorOutOfMemory("Failed to allocate dest buffer.");
    return false;
  }
  const auto dstBegin = static_cast<uint8_t*>(dstBuffer.get());

  ////

  // And go!:
  bool wasTrivial;
  if (!ConvertImage(rowLength, rowCount, srcBegin, srcStride, srcOrigin,
                    srcFormat, srcIsPremult, dstBegin, dstStride, dstOrigin,
                    dstFormat, dstIsPremult, &wasTrivial)) {
    webgl->ErrorImplementationBug("ConvertImage failed.");
    return false;
  }

  *out_begin = dstBegin;
  *out_anchoredBuffer = std::move(dstBuffer);
  return true;
}

static GLenum DoTexOrSubImage(bool isSubImage, gl::GLContext* gl,
                              TexImageTarget target, GLint level,
                              const DriverUnpackInfo* dui, GLint xOffset,
                              GLint yOffset, GLint zOffset, GLsizei width,
                              GLsizei height, GLsizei depth, const void* data) {
  if (isSubImage) {
    return DoTexSubImage(gl, target, level, xOffset, yOffset, zOffset, width,
                         height, depth, dui->ToPacking(), data);
  } else {
    return DoTexImage(gl, target, level, dui, width, height, depth, data);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////
// TexUnpackBytes

bool TexUnpackBytes::Validate(const WebGLContext* const webgl,
                              const webgl::PackingInfo& pi) {
  if (!HasData()) return true;

  CheckedInt<size_t> availBytes = 0;
  if (mDesc.cpuData) {
    const auto& range = mDesc.cpuData->Data();
    availBytes = range.length();
  } else if (mDesc.pboOffset) {
    const auto& pboOffset = *mDesc.pboOffset;

    const auto& pbo =
        webgl->ValidateBufferSelection(LOCAL_GL_PIXEL_UNPACK_BUFFER);
    if (!pbo) return false;  // Might be invalid e.g. due to in-use by TF.
    availBytes = pbo->ByteLength();
    availBytes -= pboOffset;
  } else {
    MOZ_ASSERT(false, "Must be one of the above");
  }
  if (!availBytes.isValid()) {
    webgl->ErrorInvalidOperation("Offset is passed end of buffer.");
    return false;
  }

  return ValidateUnpackBytes(webgl, pi, availBytes.value(), this);
}

bool TexUnpackBytes::TexOrSubImage(bool isSubImage, bool needsRespec,
                                   WebGLTexture* tex, GLint level,
                                   const webgl::DriverUnpackInfo* dui,
                                   GLint xOffset, GLint yOffset, GLint zOffset,
                                   const webgl::PackingInfo& pi,
                                   GLenum* const out_error) const {
  const auto& webgl = tex->mContext;
  const auto& target = mDesc.imageTarget;
  const auto& size = mDesc.size;
  const auto& unpacking = mDesc.unpacking;

  const auto format = FormatForPackingInfo(pi);
  const auto bytesPerPixel = webgl::BytesPerPixel(pi);

  const uint8_t* uploadPtr = nullptr;
  if (mDesc.cpuData) {
    const auto range = mDesc.cpuData->Data();
    uploadPtr = range.begin().get();
    if (!uploadPtr) {
      MOZ_ASSERT(!range.length());
    }
  } else if (mDesc.pboOffset) {
    uploadPtr = reinterpret_cast<const uint8_t*>(*mDesc.pboOffset);
  }

  UniqueBuffer tempBuffer;

  do {
    if (mDesc.pboOffset || !uploadPtr) break;

    if (!unpacking.mFlipY && !unpacking.mPremultiplyAlpha) {
      break;
    }

    webgl->GenerateWarning(
        "Alpha-premult and y-flip are deprecated for"
        " non-DOM-Element uploads.");

    const uint32_t rowLength = size.x;
    const uint32_t rowCount = size.y * size.z;
    const auto stride = RoundUpToMultipleOf(rowLength * bytesPerPixel,
                                            unpacking.mUnpackAlignment);
    if (!ConvertIfNeeded(webgl, rowLength, rowCount, format, uploadPtr, stride,
                         format, stride, &uploadPtr, &tempBuffer)) {
      return false;
    }
  } while (false);

  //////

  const auto& gl = webgl->gl;

  bool useParanoidHandling = false;
  if (mNeedsExactUpload && webgl->mBoundPixelUnpackBuffer) {
    webgl->GenerateWarning(
        "Uploads from a buffer with a final row with a byte"
        " count smaller than the row stride can incur extra"
        " overhead.");

    if (gl->WorkAroundDriverBugs()) {
      useParanoidHandling |= (gl->Vendor() == gl::GLVendor::NVIDIA);
    }
  }

  if (!useParanoidHandling) {
    const ScopedLazyBind bindPBO(gl, LOCAL_GL_PIXEL_UNPACK_BUFFER,
                                 webgl->mBoundPixelUnpackBuffer);

    *out_error =
        DoTexOrSubImage(isSubImage, gl, target, level, dui, xOffset, yOffset,
                        zOffset, size.x, size.y, size.z, uploadPtr);
    return true;
  }

  //////

  MOZ_ASSERT(webgl->mBoundPixelUnpackBuffer);

  if (!isSubImage) {
    // Alloc first to catch OOMs.
    AssertUintParamCorrect(gl, LOCAL_GL_PIXEL_UNPACK_BUFFER_BINDING, 0);
    *out_error =
        DoTexOrSubImage(false, gl, target, level, dui, xOffset, yOffset,
                        zOffset, size.x, size.y, size.z, nullptr);
    if (*out_error) return true;
  }

  const ScopedLazyBind bindPBO(gl, LOCAL_GL_PIXEL_UNPACK_BUFFER,
                               webgl->mBoundPixelUnpackBuffer);

  //////

  // Make our sometimes-implicit values explicit. Also this keeps them constant
  // when we ask for height=mHeight-1 and such.
  gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, unpacking.mUnpackRowLength);
  gl->fPixelStorei(LOCAL_GL_UNPACK_IMAGE_HEIGHT, unpacking.mUnpackImageHeight);

  if (size.z > 1) {
    *out_error =
        DoTexOrSubImage(true, gl, target, level, dui, xOffset, yOffset, zOffset,
                        size.x, size.y, size.z - 1, uploadPtr);
  }

  // Skip the images we uploaded.
  const auto skipImages = ZeroOn2D(target, unpacking.mUnpackSkipImages);
  gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_IMAGES, skipImages + size.z - 1);

  if (size.y > 1) {
    *out_error =
        DoTexOrSubImage(true, gl, target, level, dui, xOffset, yOffset,
                        zOffset + size.z - 1, size.x, size.y - 1, 1, uploadPtr);
  }

  const auto totalSkipRows =
      CheckedUint32(skipImages) * unpacking.mUnpackImageHeight +
      unpacking.mUnpackSkipRows;
  const auto totalFullRows =
      CheckedUint32(size.z - 1) * unpacking.mUnpackImageHeight + size.y - 1;
  const auto tailOffsetRows = totalSkipRows + totalFullRows;

  const auto bytesPerRow =
      CheckedUint32(unpacking.mUnpackRowLength) * bytesPerPixel;
  const auto rowStride =
      RoundUpToMultipleOf(bytesPerRow, unpacking.mUnpackAlignment);
  if (!rowStride.isValid()) {
    MOZ_CRASH("Should be checked earlier.");
  }
  const auto tailOffsetBytes = tailOffsetRows * rowStride;

  uploadPtr += tailOffsetBytes.value();

  //////

  gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 1);    // No stride padding.
  gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, 0);   // No padding in general.
  gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_IMAGES, 0);  // Don't skip images,
  gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_ROWS,
                   0);  // or rows.
                        // Keep skipping pixels though!

  *out_error = DoTexOrSubImage(true, gl, target, level, dui, xOffset,
                               yOffset + size.y - 1, zOffset + size.z - 1,
                               size.x, 1, 1, uploadPtr);

  // Caller will reset all our modified PixelStorei state.

  return true;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TexUnpackImage

TexUnpackImage::~TexUnpackImage() = default;

bool TexUnpackImage::Validate(const WebGLContext* const webgl,
                              const webgl::PackingInfo& pi) {
  if (!ValidatePIForDOM(webgl, pi)) return false;

  const auto fullRows = mDesc.imageSize.y;
  return ValidateUnpackPixels(webgl, fullRows, 0, this);
}

Maybe<std::string> BlitPreventReason(const int32_t level, const ivec3& offset,
                                     const webgl::PackingInfo& pi,
                                     const TexUnpackBlobDesc& desc) {
  const auto& size = desc.size;
  const auto& unpacking = desc.unpacking;

  const auto ret = [&]() -> const char* {
    if (size.z != 1) {
      return "depth is not 1";
    }
    if (offset.x != 0 || offset.y != 0 || offset.z != 0) {
      return "x/y/zOffset is not 0";
    }

    if (unpacking.mUnpackSkipPixels || unpacking.mUnpackSkipRows ||
        unpacking.mUnpackSkipImages) {
      return "non-zero UNPACK_SKIP_* not yet supported";
    }

    const auto premultReason = [&]() -> const char* {
      if (desc.srcAlphaType == gfxAlphaType::Opaque) return nullptr;

      const bool srcIsPremult = (desc.srcAlphaType == gfxAlphaType::Premult);
      const auto& dstIsPremult = unpacking.mPremultiplyAlpha;
      if (srcIsPremult == dstIsPremult) return nullptr;

      if (dstIsPremult) {
        return "UNPACK_PREMULTIPLY_ALPHA_WEBGL is not true";
      } else {
        return "UNPACK_PREMULTIPLY_ALPHA_WEBGL is not false";
      }
    }();
    if (premultReason) return premultReason;

    if (pi.format != LOCAL_GL_RGBA) {
      return "`format` is not RGBA";
    }

    if (pi.type != LOCAL_GL_UNSIGNED_BYTE) {
      return "`type` is not UNSIGNED_BYTE";
    }
    return nullptr;
  }();
  if (ret) {
    return Some(std::string(ret));
  }
  return {};
}

bool TexUnpackImage::TexOrSubImage(bool isSubImage, bool needsRespec,
                                   WebGLTexture* tex, GLint level,
                                   const webgl::DriverUnpackInfo* dui,
                                   GLint xOffset, GLint yOffset, GLint zOffset,
                                   const webgl::PackingInfo& pi,
                                   GLenum* const out_error) const {
  MOZ_ASSERT_IF(needsRespec, !isSubImage);

  const auto& webgl = tex->mContext;
  const auto& target = mDesc.imageTarget;
  const auto& size = mDesc.size;
  const auto& sd = *(mDesc.sd);
  const auto& unpacking = mDesc.unpacking;

  const auto& gl = webgl->GL();

  // -

  const auto reason =
      BlitPreventReason(level, {xOffset, yOffset, zOffset}, pi, mDesc);
  if (reason) {
    webgl->GeneratePerfWarning(
        "Failed to hit GPU-copy fast-path."
        " (%s) Falling back to CPU upload.",
        reason->c_str());
    return false;
  }

  // -

  if (needsRespec) {
    *out_error =
        DoTexOrSubImage(isSubImage, gl, target, level, dui, xOffset, yOffset,
                        zOffset, size.x, size.y, size.z, nullptr);
    if (*out_error) return true;
  }

  {
    gl::ScopedFramebuffer scopedFB(gl);
    gl::ScopedBindFramebuffer bindFB(gl, scopedFB.FB());

    {
      gl::GLContext::LocalErrorScope errorScope(*gl);

      gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                LOCAL_GL_COLOR_ATTACHMENT0, target,
                                tex->mGLName, level);

      const auto err = errorScope.GetError();
      MOZ_ALWAYS_TRUE(!err);
    }

    const GLenum status = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    MOZ_ALWAYS_TRUE(status == LOCAL_GL_FRAMEBUFFER_COMPLETE);

    const auto dstOrigin =
        (unpacking.mFlipY ? gl::OriginPos::TopLeft : gl::OriginPos::BottomLeft);
    if (!gl->BlitHelper()->BlitSdToFramebuffer(sd, {size.x, size.y},
                                               dstOrigin)) {
      gfxCriticalNote << "BlitSdToFramebuffer failed for type "
                      << int(sd.type());
      // Maybe the resource isn't valid anymore?
      gl->fClearColor(0.2, 0.0, 0.2, 1.0);
      gl->fClear(LOCAL_GL_COLOR_BUFFER_BIT);
      const auto& cur = webgl->mColorClearValue;
      gl->fClearColor(cur[0], cur[1], cur[2], cur[3]);
      webgl->GenerateWarning(
          "Fast Tex(Sub)Image upload failed without recourse, clearing to "
          "[0.2, 0.0, 0.2, 1.0]. Please file a bug!");
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TexUnpackSurface

TexUnpackSurface::~TexUnpackSurface() = default;

//////////

static bool GetFormatForSurf(const gfx::SourceSurface* surf,
                             WebGLTexelFormat* const out_texelFormat,
                             uint8_t* const out_bpp) {
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

bool TexUnpackSurface::Validate(const WebGLContext* const webgl,
                                const webgl::PackingInfo& pi) {
  if (!ValidatePIForDOM(webgl, pi)) return false;

  const auto fullRows = mDesc.dataSurf->GetSize().height;
  return ValidateUnpackPixels(webgl, fullRows, 0, this);
}

bool TexUnpackSurface::TexOrSubImage(bool isSubImage, bool needsRespec,
                                     WebGLTexture* tex, GLint level,
                                     const webgl::DriverUnpackInfo* dui,
                                     GLint xOffset, GLint yOffset,
                                     GLint zOffset,
                                     const webgl::PackingInfo& dstPI,
                                     GLenum* const out_error) const {
  const auto& webgl = tex->mContext;
  const auto& size = mDesc.size;
  auto& surf = *(mDesc.dataSurf);

  ////

  const auto rowLength = surf.GetSize().width;
  const auto rowCount = surf.GetSize().height;

  const auto& dstBPP = webgl::BytesPerPixel(dstPI);
  const auto dstFormat = FormatForPackingInfo(dstPI);

  ////

  WebGLTexelFormat srcFormat;
  uint8_t srcBPP;
  if (!GetFormatForSurf(&surf, &srcFormat, &srcBPP)) {
    webgl->ErrorImplementationBug(
        "GetFormatForSurf failed for"
        " WebGLTexelFormat::%u.",
        uint32_t(surf.GetFormat()));
    return false;
  }

  gfx::DataSourceSurface::ScopedMap map(&surf,
                                        gfx::DataSourceSurface::MapType::READ);
  if (!map.IsMapped()) {
    webgl->ErrorOutOfMemory("Failed to map source surface for upload.");
    return false;
  }

  const auto& srcBegin = map.GetData();
  const auto& srcStride = map.GetStride();

  ////

  const auto srcRowLengthBytes = rowLength * srcBPP;

  const uint8_t maxGLAlignment = 8;
  uint8_t srcAlignment = 1;
  for (; srcAlignment <= maxGLAlignment; srcAlignment *= 2) {
    const auto strideGuess =
        RoundUpToMultipleOf(srcRowLengthBytes, srcAlignment);
    if (strideGuess == srcStride) break;
  }
  const uint32_t dstAlignment =
      (srcAlignment > maxGLAlignment) ? 1 : srcAlignment;

  const auto dstRowLengthBytes = rowLength * dstBPP;
  const auto dstStride = RoundUpToMultipleOf(dstRowLengthBytes, dstAlignment);

  ////

  const uint8_t* dstBegin = srcBegin;
  UniqueBuffer tempBuffer;
  if (!ConvertIfNeeded(webgl, rowLength, rowCount, srcFormat, srcBegin,
                       srcStride, dstFormat, dstStride, &dstBegin,
                       &tempBuffer)) {
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

  *out_error =
      DoTexOrSubImage(isSubImage, gl, mDesc.imageTarget, level, dui, xOffset,
                      yOffset, zOffset, size.x, size.y, size.z, dstBegin);

  // Caller will reset all our modified PixelStorei state.

  return true;
}

}  // namespace webgl
}  // namespace mozilla
