/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLUploadHelpers.h"

#include "GLContext.h"
#include "mozilla/gfx/2D.h"
#include "gfxUtils.h"
#include "mozilla/gfx/Tools.h"  // For BytesPerPixel
#include "nsRegion.h"
#include "GfxTexturesReporter.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {

using namespace gfx;

namespace gl {

static unsigned int DataOffset(const IntPoint& aPoint, int32_t aStride,
                               SurfaceFormat aFormat) {
  unsigned int data = aPoint.y * aStride;
  data += aPoint.x * BytesPerPixel(aFormat);
  return data;
}

static bool CheckUploadBounds(const IntSize& aDst, const IntSize& aSrc,
                              const IntPoint& aOffset) {
  if (aOffset.x < 0 || aOffset.y < 0 || aOffset.x >= aSrc.width ||
      aOffset.y >= aSrc.height) {
    MOZ_ASSERT_UNREACHABLE("Offset outside source bounds");
    return false;
  }
  if (aDst.width > (aSrc.width - aOffset.x) ||
      aDst.height > (aSrc.height - aOffset.y)) {
    MOZ_ASSERT_UNREACHABLE("Source has insufficient data");
    return false;
  }
  return true;
}

static GLint GetAddressAlignment(ptrdiff_t aAddress) {
  if (!(aAddress & 0x7)) {
    return 8;
  } else if (!(aAddress & 0x3)) {
    return 4;
  } else if (!(aAddress & 0x1)) {
    return 2;
  } else {
    return 1;
  }
}

// Take texture data in a given buffer and copy it into a larger buffer,
// padding out the edge pixels for filtering if necessary
static void CopyAndPadTextureData(const GLvoid* srcBuffer, GLvoid* dstBuffer,
                                  GLsizei srcWidth, GLsizei srcHeight,
                                  GLsizei dstWidth, GLsizei dstHeight,
                                  GLsizei stride, GLint pixelsize) {
  unsigned char* rowDest = static_cast<unsigned char*>(dstBuffer);
  const unsigned char* source = static_cast<const unsigned char*>(srcBuffer);

  for (GLsizei h = 0; h < srcHeight; ++h) {
    memcpy(rowDest, source, srcWidth * pixelsize);
    rowDest += dstWidth * pixelsize;
    source += stride;
  }

  GLsizei padHeight = srcHeight;

  // Pad out an extra row of pixels so that edge filtering doesn't use garbage
  // data
  if (dstHeight > srcHeight) {
    memcpy(rowDest, source - stride, srcWidth * pixelsize);
    padHeight++;
  }

  // Pad out an extra column of pixels
  if (dstWidth > srcWidth) {
    rowDest = static_cast<unsigned char*>(dstBuffer) + srcWidth * pixelsize;
    for (GLsizei h = 0; h < padHeight; ++h) {
      memcpy(rowDest, rowDest - pixelsize, pixelsize);
      rowDest += dstWidth * pixelsize;
    }
  }
}

// In both of these cases (for the Adreno at least) it is impossible
// to determine good or bad driver versions for POT texture uploads,
// so blacklist them all. Newer drivers use a different rendering
// string in the form "Adreno (TM) 200" and the drivers we've seen so
// far work fine with NPOT textures, so don't blacklist those until we
// have evidence of any problems with them.
bool ShouldUploadSubTextures(GLContext* gl) {
  if (!gl->WorkAroundDriverBugs()) return true;

  // There are certain GPUs that we don't want to use glTexSubImage2D on
  // because that function can be very slow and/or buggy
  if (gl->Renderer() == GLRenderer::Adreno200 ||
      gl->Renderer() == GLRenderer::Adreno205) {
    return false;
  }

  // On PowerVR glTexSubImage does a readback, so it will be slower
  // than just doing a glTexImage2D() directly. i.e. 26ms vs 10ms
  if (gl->Renderer() == GLRenderer::SGX540 ||
      gl->Renderer() == GLRenderer::SGX530) {
    return false;
  }

  return true;
}

static void TexSubImage2DWithUnpackSubimageGLES(
    GLContext* gl, GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLsizei width, GLsizei height, GLsizei stride, GLint pixelsize,
    GLenum format, GLenum type, const GLvoid* pixels) {
  gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                   std::min(GetAddressAlignment((ptrdiff_t)pixels),
                            GetAddressAlignment((ptrdiff_t)stride)));
  // When using GL_UNPACK_ROW_LENGTH, we need to work around a Tegra
  // driver crash where the driver apparently tries to read
  // (stride - width * pixelsize) bytes past the end of the last input
  // row. We only upload the first height-1 rows using GL_UNPACK_ROW_LENGTH,
  // and then we upload the final row separately. See bug 697990.
  int rowLength = stride / pixelsize;
  if (gl->HasPBOState()) {
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, rowLength);
    gl->fTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                       type, pixels);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, 0);
  } else {
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, rowLength);
    gl->fTexSubImage2D(target, level, xoffset, yoffset, width, height - 1,
                       format, type, pixels);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, 0);
    gl->fTexSubImage2D(target, level, xoffset, yoffset + height - 1, width, 1,
                       format, type,
                       (const unsigned char*)pixels + (height - 1) * stride);
  }
  gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
}

static void TexSubImage2DWithoutUnpackSubimage(
    GLContext* gl, GLenum target, GLint level, GLint xoffset, GLint yoffset,
    GLsizei width, GLsizei height, GLsizei stride, GLint pixelsize,
    GLenum format, GLenum type, const GLvoid* pixels) {
  // Not using the whole row of texture data and GL_UNPACK_ROW_LENGTH
  // isn't supported. We make a copy of the texture data we're using,
  // such that we're using the whole row of data in the copy. This turns
  // out to be more efficient than uploading row-by-row; see bug 698197.

  // Width and height are never more than 16384. At 16Ki*16Ki, 4bpp is 1GiB, but
  // if we allow 8bpp (or higher) here, that's 2GiB, which would overflow on
  // 32-bit.
  MOZ_ASSERT(width <= 16384);
  MOZ_ASSERT(height <= 16384);
  MOZ_ASSERT(pixelsize < 8);

  const auto size = CheckedInt<size_t>(width) * height * pixelsize;
  if (!size.isValid()) {
    // This should never happen, but we use a defensive check.
    MOZ_ASSERT_UNREACHABLE("Unacceptable size calculated.!");
    return;
  }

  unsigned char* newPixels = new (fallible) unsigned char[size.value()];

  if (newPixels) {
    unsigned char* rowDest = newPixels;
    const unsigned char* rowSource = (const unsigned char*)pixels;
    for (int h = 0; h < height; h++) {
      memcpy(rowDest, rowSource, width * pixelsize);
      rowDest += width * pixelsize;
      rowSource += stride;
    }

    stride = width * pixelsize;
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                     std::min(GetAddressAlignment((ptrdiff_t)newPixels),
                              GetAddressAlignment((ptrdiff_t)stride)));
    gl->fTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                       type, newPixels);
    delete[] newPixels;
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);

  } else {
    // If we did not have sufficient memory for the required
    // temporary buffer, then fall back to uploading row-by-row.
    const unsigned char* rowSource = (const unsigned char*)pixels;

    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                     std::min(GetAddressAlignment((ptrdiff_t)pixels),
                              GetAddressAlignment((ptrdiff_t)stride)));

    for (int i = 0; i < height; i++) {
      gl->fTexSubImage2D(target, level, xoffset, yoffset + i, width, 1, format,
                         type, rowSource);
      rowSource += stride;
    }

    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
  }
}
static void TexSubImage2DHelper(GLContext* gl, GLenum target, GLint level,
                                GLint xoffset, GLint yoffset, GLsizei width,
                                GLsizei height, GLsizei stride, GLint pixelsize,
                                GLenum format, GLenum type,
                                const GLvoid* pixels) {
  if (gl->IsGLES()) {
    if (stride == width * pixelsize) {
      gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                       std::min(GetAddressAlignment((ptrdiff_t)pixels),
                                GetAddressAlignment((ptrdiff_t)stride)));
      gl->fTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                         type, pixels);
      gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
    } else if (gl->IsExtensionSupported(GLContext::EXT_unpack_subimage) ||
               gl->HasPBOState()) {
      TexSubImage2DWithUnpackSubimageGLES(gl, target, level, xoffset, yoffset,
                                          width, height, stride, pixelsize,
                                          format, type, pixels);

    } else {
      TexSubImage2DWithoutUnpackSubimage(gl, target, level, xoffset, yoffset,
                                         width, height, stride, pixelsize,
                                         format, type, pixels);
    }
  } else {
    // desktop GL (non-ES) path
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                     std::min(GetAddressAlignment((ptrdiff_t)pixels),
                              GetAddressAlignment((ptrdiff_t)stride)));
    int rowLength = stride / pixelsize;
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, rowLength);
    gl->fTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                       type, pixels);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, 0);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
  }
}

static void TexImage2DHelper(GLContext* gl, GLenum target, GLint level,
                             GLint internalformat, GLsizei width,
                             GLsizei height, GLsizei stride, GLint pixelsize,
                             GLint border, GLenum format, GLenum type,
                             const GLvoid* pixels) {
  if (gl->IsGLES()) {
    NS_ASSERTION(
        format == (GLenum)internalformat,
        "format and internalformat not the same for glTexImage2D on GLES2");

    MOZ_ASSERT(width >= 0 && height >= 0);
    if (!CanUploadNonPowerOfTwo(gl) &&
        (stride != width * pixelsize || !IsPowerOfTwo((uint32_t)width) ||
         !IsPowerOfTwo((uint32_t)height))) {
      // Pad out texture width and height to the next power of two
      // as we don't support/want non power of two texture uploads
      GLsizei paddedWidth = RoundUpPow2((uint32_t)width);
      GLsizei paddedHeight = RoundUpPow2((uint32_t)height);

      // Width and height are never more than 16384. At 16Ki*16Ki, 4bpp is 1GiB,
      // but if we allow 8bpp (or higher) here, that's 2GiB, which would
      // overflow on 32-bit.
      MOZ_ASSERT(width <= 16384);
      MOZ_ASSERT(height <= 16384);
      MOZ_ASSERT(pixelsize < 8);

      const auto size =
          CheckedInt<size_t>(paddedWidth) * paddedHeight * pixelsize;
      if (!size.isValid()) {
        // This should never happen, but we use a defensive check.
        MOZ_ASSERT_UNREACHABLE("Unacceptable size calculated.!");
        return;
      }

      GLvoid* paddedPixels = new unsigned char[size.value()];

      // Pad out texture data to be in a POT sized buffer for uploading to
      // a POT sized texture
      CopyAndPadTextureData(pixels, paddedPixels, width, height, paddedWidth,
                            paddedHeight, stride, pixelsize);

      gl->fPixelStorei(
          LOCAL_GL_UNPACK_ALIGNMENT,
          std::min(GetAddressAlignment((ptrdiff_t)paddedPixels),
                   GetAddressAlignment((ptrdiff_t)paddedWidth * pixelsize)));
      gl->fTexImage2D(target, border, internalformat, paddedWidth, paddedHeight,
                      border, format, type, paddedPixels);
      gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);

      delete[] static_cast<unsigned char*>(paddedPixels);
      return;
    }

    if (stride == width * pixelsize) {
      gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                       std::min(GetAddressAlignment((ptrdiff_t)pixels),
                                GetAddressAlignment((ptrdiff_t)stride)));
      gl->fTexImage2D(target, border, internalformat, width, height, border,
                      format, type, pixels);
      gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
    } else {
      // Use GLES-specific workarounds for GL_UNPACK_ROW_LENGTH; these are
      // implemented in TexSubImage2D.
      gl->fTexImage2D(target, border, internalformat, width, height, border,
                      format, type, nullptr);
      TexSubImage2DHelper(gl, target, level, 0, 0, width, height, stride,
                          pixelsize, format, type, pixels);
    }
  } else {
    // desktop GL (non-ES) path

    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                     std::min(GetAddressAlignment((ptrdiff_t)pixels),
                              GetAddressAlignment((ptrdiff_t)stride)));
    int rowLength = stride / pixelsize;
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, rowLength);
    gl->fTexImage2D(target, level, internalformat, width, height, border,
                    format, type, pixels);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, 0);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
  }
}

SurfaceFormat UploadImageDataToTexture(
    GLContext* gl, unsigned char* aData, const gfx::IntSize& aDataSize,
    const IntPoint& aDstOffset, int32_t aStride, SurfaceFormat aFormat,
    const nsIntRegion& aDstRegion, GLuint aTexture, const gfx::IntSize& aSize,
    size_t* aOutUploadSize, bool aNeedInit, GLenum aTextureUnit,
    GLenum aTextureTarget) {
  gl->MakeCurrent();
  gl->fActiveTexture(aTextureUnit);
  gl->fBindTexture(aTextureTarget, aTexture);

  GLenum format = 0;
  GLenum internalFormat = 0;
  GLenum type = 0;
  int32_t pixelSize = BytesPerPixel(aFormat);
  SurfaceFormat surfaceFormat = gfx::SurfaceFormat::UNKNOWN;

  MOZ_ASSERT(gl->GetPreferredARGB32Format() == LOCAL_GL_BGRA ||
             gl->GetPreferredARGB32Format() == LOCAL_GL_RGBA);

  switch (aFormat) {
    case SurfaceFormat::B8G8R8A8:
      if (gl->GetPreferredARGB32Format() == LOCAL_GL_BGRA) {
        format = LOCAL_GL_BGRA;
        surfaceFormat = SurfaceFormat::R8G8B8A8;
        type = LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV;
      } else {
        format = LOCAL_GL_RGBA;
        surfaceFormat = SurfaceFormat::B8G8R8A8;
        type = LOCAL_GL_UNSIGNED_BYTE;
      }
      internalFormat = LOCAL_GL_RGBA;
      break;
    case SurfaceFormat::B8G8R8X8:
      // Treat BGRX surfaces as BGRA except for the surface
      // format used.
      if (gl->GetPreferredARGB32Format() == LOCAL_GL_BGRA) {
        format = LOCAL_GL_BGRA;
        surfaceFormat = SurfaceFormat::R8G8B8X8;
        type = LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV;
      } else {
        format = LOCAL_GL_RGBA;
        surfaceFormat = SurfaceFormat::B8G8R8X8;
        type = LOCAL_GL_UNSIGNED_BYTE;
      }
      internalFormat = LOCAL_GL_RGBA;
      break;
    case SurfaceFormat::R8G8B8A8:
      if (gl->GetPreferredARGB32Format() == LOCAL_GL_BGRA) {
        // Upload our RGBA as BGRA, but store that the uploaded format is
        // BGRA. (sample from R to get B)
        format = LOCAL_GL_BGRA;
        type = LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV;
        surfaceFormat = SurfaceFormat::B8G8R8A8;
      } else {
        format = LOCAL_GL_RGBA;
        type = LOCAL_GL_UNSIGNED_BYTE;
        surfaceFormat = SurfaceFormat::R8G8B8A8;
      }
      internalFormat = LOCAL_GL_RGBA;
      break;
    case SurfaceFormat::R8G8B8X8:
      // Treat RGBX surfaces as RGBA except for the surface
      // format used.
      if (gl->GetPreferredARGB32Format() == LOCAL_GL_BGRA) {
        format = LOCAL_GL_BGRA;
        type = LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV;
        surfaceFormat = SurfaceFormat::B8G8R8X8;
      } else {
        format = LOCAL_GL_RGBA;
        type = LOCAL_GL_UNSIGNED_BYTE;
        surfaceFormat = SurfaceFormat::R8G8B8X8;
      }
      internalFormat = LOCAL_GL_RGBA;
      break;
    case SurfaceFormat::R5G6B5_UINT16:
      internalFormat = format = LOCAL_GL_RGB;
      type = LOCAL_GL_UNSIGNED_SHORT_5_6_5;
      surfaceFormat = SurfaceFormat::R5G6B5_UINT16;
      break;
    case SurfaceFormat::A8:
      if (gl->IsGLES()) {
        format = LOCAL_GL_LUMINANCE;
        internalFormat = LOCAL_GL_LUMINANCE;
      } else {
        format = LOCAL_GL_RED;
        internalFormat = LOCAL_GL_R8;
      }
      type = LOCAL_GL_UNSIGNED_BYTE;
      // We don't have a specific luminance shader
      surfaceFormat = SurfaceFormat::A8;
      break;
    case SurfaceFormat::A16:
      if (gl->IsGLES()) {
        format = LOCAL_GL_LUMINANCE;
        internalFormat = LOCAL_GL_LUMINANCE16;
      } else {
        format = LOCAL_GL_RED;
        internalFormat = LOCAL_GL_R16;
      }
      type = LOCAL_GL_UNSIGNED_SHORT;
      // We don't have a specific luminance shader
      surfaceFormat = SurfaceFormat::A8;
      pixelSize = 2;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled image surface format!");
  }

  if (aOutUploadSize) {
    *aOutUploadSize = 0;
  }

  if (surfaceFormat == gfx::SurfaceFormat::UNKNOWN) {
    return gfx::SurfaceFormat::UNKNOWN;
  }

  // We can only skip SubTextures if aOffset = 0 because we need the whole
  // buffer
  if (aNeedInit || (!ShouldUploadSubTextures(gl) && aDstOffset == IntPoint())) {
    if (!CheckUploadBounds(aSize, aDataSize, IntPoint())) {
      return SurfaceFormat::UNKNOWN;
    }
    // If the texture needs initialized, or we are unable to
    // upload sub textures, then initialize and upload the entire
    // texture.
    TexImage2DHelper(gl, aTextureTarget, 0, internalFormat, aSize.width,
                     aSize.height, aStride, pixelSize, 0, format, type, aData);

    if (aOutUploadSize && aNeedInit) {
      uint32_t texelSize = GetBytesPerTexel(internalFormat, type);
      size_t numTexels = size_t(aSize.width) * size_t(aSize.height);
      *aOutUploadSize += texelSize * numTexels;
    }
  } else {
    // Upload each rect in the region to the texture
    for (auto iter = aDstRegion.RectIter(); !iter.Done(); iter.Next()) {
      IntRect rect = iter.Get();
      if (!CheckUploadBounds(rect.Size(), aDataSize, rect.TopLeft())) {
        return SurfaceFormat::UNKNOWN;
      }

      const unsigned char* rectData =
          aData + DataOffset(rect.TopLeft(), aStride, aFormat);

      rect += aDstOffset;
      TexSubImage2DHelper(gl, aTextureTarget, 0, rect.X(), rect.Y(),
                          rect.Width(), rect.Height(), aStride, pixelSize,
                          format, type, rectData);
    }
  }

  return surfaceFormat;
}

SurfaceFormat UploadSurfaceToTexture(GLContext* gl, DataSourceSurface* aSurface,
                                     const nsIntRegion& aDstRegion,
                                     GLuint aTexture, const gfx::IntSize& aSize,
                                     size_t* aOutUploadSize, bool aNeedInit,
                                     const gfx::IntPoint& aSrcOffset,
                                     const gfx::IntPoint& aDstOffset,
                                     GLenum aTextureUnit,
                                     GLenum aTextureTarget) {
  DataSourceSurface::ScopedMap map(aSurface, DataSourceSurface::READ);
  int32_t stride = map.GetStride();
  SurfaceFormat format = aSurface->GetFormat();
  gfx::IntSize size = aSurface->GetSize();

  // only fail if we'll need the entire surface for initialization
  if (aNeedInit && !CheckUploadBounds(aSize, size, aSrcOffset)) {
    return SurfaceFormat::UNKNOWN;
  }

  unsigned char* data = map.GetData() + DataOffset(aSrcOffset, stride, format);
  size.width -= aSrcOffset.x;
  size.height -= aSrcOffset.y;

  return UploadImageDataToTexture(gl, data, size, aDstOffset, stride, format,
                                  aDstRegion, aTexture, aSize, aOutUploadSize,
                                  aNeedInit, aTextureUnit, aTextureTarget);
}

bool CanUploadNonPowerOfTwo(GLContext* gl) {
  if (!gl->WorkAroundDriverBugs()) return true;

  // Some GPUs driver crash when uploading non power of two 565 textures.
  return gl->Renderer() != GLRenderer::Adreno200 &&
         gl->Renderer() != GLRenderer::Adreno205;
}

}  // namespace gl
}  // namespace mozilla
