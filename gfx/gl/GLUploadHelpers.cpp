/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLUploadHelpers.h"

#include "GLContext.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Tools.h"  // For BytesPerPixel
#include "nsRegion.h"
#include "GfxTexturesReporter.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {

using namespace gfx;

namespace gl {

/* These two techniques are suggested by "Bit Twiddling Hacks"
 */

/**
 * Returns true if |aNumber| is a power of two
 * 0 is incorreclty considered a power of two
 */
static bool
IsPowerOfTwo(int aNumber)
{
    return (aNumber & (aNumber - 1)) == 0;
}

/**
 * Returns the first integer greater than |aNumber| which is a power of two
 * Undefined for |aNumber| < 0
 */
static int
NextPowerOfTwo(int aNumber)
{
#if defined(__arm__)
    return 1 << (32 - __builtin_clz(aNumber - 1));
#else
    --aNumber;
    aNumber |= aNumber >> 1;
    aNumber |= aNumber >> 2;
    aNumber |= aNumber >> 4;
    aNumber |= aNumber >> 8;
    aNumber |= aNumber >> 16;
    return ++aNumber;
#endif
}

static unsigned int
DataOffset(const IntPoint &aPoint, int32_t aStride, SurfaceFormat aFormat)
{
  unsigned int data = aPoint.y * aStride;
  data += aPoint.x * BytesPerPixel(aFormat);
  return data;
}

static GLint GetAddressAlignment(ptrdiff_t aAddress)
{
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
static void
CopyAndPadTextureData(const GLvoid* srcBuffer,
                      GLvoid* dstBuffer,
                      GLsizei srcWidth, GLsizei srcHeight,
                      GLsizei dstWidth, GLsizei dstHeight,
                      GLsizei stride, GLint pixelsize)
{
    unsigned char *rowDest = static_cast<unsigned char*>(dstBuffer);
    const unsigned char *source = static_cast<const unsigned char*>(srcBuffer);

    for (GLsizei h = 0; h < srcHeight; ++h) {
        memcpy(rowDest, source, srcWidth * pixelsize);
        rowDest += dstWidth * pixelsize;
        source += stride;
    }

    GLsizei padHeight = srcHeight;

    // Pad out an extra row of pixels so that edge filtering doesn't use garbage data
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
bool
CanUploadSubTextures(GLContext* gl)
{
    if (!gl->WorkAroundDriverBugs())
        return true;

    // There are certain GPUs that we don't want to use glTexSubImage2D on
    // because that function can be very slow and/or buggy
    if (gl->Renderer() == GLRenderer::Adreno200 ||
        gl->Renderer() == GLRenderer::Adreno205)
    {
        return false;
    }

    // On PowerVR glTexSubImage does a readback, so it will be slower
    // than just doing a glTexImage2D() directly. i.e. 26ms vs 10ms
    if (gl->Renderer() == GLRenderer::SGX540 ||
        gl->Renderer() == GLRenderer::SGX530)
    {
        return false;
    }

    return true;
}

static void
TexSubImage2DWithUnpackSubimageGLES(GLContext* gl,
                                    GLenum target, GLint level,
                                    GLint xoffset, GLint yoffset,
                                    GLsizei width, GLsizei height,
                                    GLsizei stride, GLint pixelsize,
                                    GLenum format, GLenum type,
                                    const GLvoid* pixels)
{
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                     std::min(GetAddressAlignment((ptrdiff_t)pixels),
                              GetAddressAlignment((ptrdiff_t)stride)));
    // When using GL_UNPACK_ROW_LENGTH, we need to work around a Tegra
    // driver crash where the driver apparently tries to read
    // (stride - width * pixelsize) bytes past the end of the last input
    // row. We only upload the first height-1 rows using GL_UNPACK_ROW_LENGTH,
    // and then we upload the final row separately. See bug 697990.
    int rowLength = stride/pixelsize;
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, rowLength);
    gl->fTexSubImage2D(target,
                       level,
                       xoffset,
                       yoffset,
                       width,
                       height-1,
                       format,
                       type,
                       pixels);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, 0);
    gl->fTexSubImage2D(target,
                       level,
                       xoffset,
                       yoffset+height-1,
                       width,
                       1,
                       format,
                       type,
                       (const unsigned char *)pixels+(height-1)*stride);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
}

static void
TexSubImage2DWithoutUnpackSubimage(GLContext* gl,
                                   GLenum target, GLint level,
                                   GLint xoffset, GLint yoffset,
                                   GLsizei width, GLsizei height,
                                   GLsizei stride, GLint pixelsize,
                                   GLenum format, GLenum type,
                                   const GLvoid* pixels)
{
    // Not using the whole row of texture data and GL_UNPACK_ROW_LENGTH
    // isn't supported. We make a copy of the texture data we're using,
    // such that we're using the whole row of data in the copy. This turns
    // out to be more efficient than uploading row-by-row; see bug 698197.
    unsigned char *newPixels = new unsigned char[width*height*pixelsize];
    unsigned char *rowDest = newPixels;
    const unsigned char *rowSource = (const unsigned char *)pixels;
    for (int h = 0; h < height; h++) {
            memcpy(rowDest, rowSource, width*pixelsize);
            rowDest += width*pixelsize;
            rowSource += stride;
    }

    stride = width*pixelsize;
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                     std::min(GetAddressAlignment((ptrdiff_t)newPixels),
                              GetAddressAlignment((ptrdiff_t)stride)));
    gl->fTexSubImage2D(target,
                       level,
                       xoffset,
                       yoffset,
                       width,
                       height,
                       format,
                       type,
                       newPixels);
    delete [] newPixels;
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
}

static void
TexSubImage2DHelper(GLContext *gl,
                    GLenum target, GLint level,
                    GLint xoffset, GLint yoffset,
                    GLsizei width, GLsizei height, GLsizei stride,
                    GLint pixelsize, GLenum format,
                    GLenum type, const GLvoid* pixels)
{
    if (gl->IsGLES()) {
        if (stride == width * pixelsize) {
            gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                             std::min(GetAddressAlignment((ptrdiff_t)pixels),
                                      GetAddressAlignment((ptrdiff_t)stride)));
            gl->fTexSubImage2D(target,
                               level,
                               xoffset,
                               yoffset,
                               width,
                               height,
                               format,
                               type,
                               pixels);
            gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
        } else if (gl->IsExtensionSupported(GLContext::EXT_unpack_subimage)) {
            TexSubImage2DWithUnpackSubimageGLES(gl, target, level, xoffset, yoffset,
                                                width, height, stride,
                                                pixelsize, format, type, pixels);

        } else {
            TexSubImage2DWithoutUnpackSubimage(gl, target, level, xoffset, yoffset,
                                              width, height, stride,
                                              pixelsize, format, type, pixels);
        }
    } else {
        // desktop GL (non-ES) path
        gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                         std::min(GetAddressAlignment((ptrdiff_t)pixels),
                                  GetAddressAlignment((ptrdiff_t)stride)));
        int rowLength = stride/pixelsize;
        gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, rowLength);
        gl->fTexSubImage2D(target,
                           level,
                           xoffset,
                           yoffset,
                           width,
                           height,
                           format,
                           type,
                           pixels);
        gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, 0);
        gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
    }
}

static void
TexImage2DHelper(GLContext *gl,
                 GLenum target, GLint level, GLint internalformat,
                 GLsizei width, GLsizei height, GLsizei stride,
                 GLint pixelsize, GLint border, GLenum format,
                 GLenum type, const GLvoid *pixels)
{
    if (gl->IsGLES()) {

        NS_ASSERTION(format == (GLenum)internalformat,
                    "format and internalformat not the same for glTexImage2D on GLES2");

        if (!CanUploadNonPowerOfTwo(gl)
            && (stride != width * pixelsize
            || !IsPowerOfTwo(width)
            || !IsPowerOfTwo(height))) {

            // Pad out texture width and height to the next power of two
            // as we don't support/want non power of two texture uploads
            GLsizei paddedWidth = NextPowerOfTwo(width);
            GLsizei paddedHeight = NextPowerOfTwo(height);

            GLvoid* paddedPixels = new unsigned char[paddedWidth * paddedHeight * pixelsize];

            // Pad out texture data to be in a POT sized buffer for uploading to
            // a POT sized texture
            CopyAndPadTextureData(pixels, paddedPixels, width, height,
                                  paddedWidth, paddedHeight, stride, pixelsize);

            gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                             std::min(GetAddressAlignment((ptrdiff_t)paddedPixels),
                                      GetAddressAlignment((ptrdiff_t)paddedWidth * pixelsize)));
            gl->fTexImage2D(target,
                            border,
                            internalformat,
                            paddedWidth,
                            paddedHeight,
                            border,
                            format,
                            type,
                            paddedPixels);
            gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);

            delete[] static_cast<unsigned char*>(paddedPixels);
            return;
        }

        if (stride == width * pixelsize) {
            gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                             std::min(GetAddressAlignment((ptrdiff_t)pixels),
                                      GetAddressAlignment((ptrdiff_t)stride)));
            gl->fTexImage2D(target,
                            border,
                            internalformat,
                            width,
                            height,
                            border,
                            format,
                            type,
                            pixels);
            gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
        } else {
            // Use GLES-specific workarounds for GL_UNPACK_ROW_LENGTH; these are
            // implemented in TexSubImage2D.
            gl->fTexImage2D(target,
                            border,
                            internalformat,
                            width,
                            height,
                            border,
                            format,
                            type,
                            nullptr);
            TexSubImage2DHelper(gl,
                                target,
                                level,
                                0,
                                0,
                                width,
                                height,
                                stride,
                                pixelsize,
                                format,
                                type,
                                pixels);
        }
    } else {
        // desktop GL (non-ES) path

        gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT,
                         std::min(GetAddressAlignment((ptrdiff_t)pixels),
                                  GetAddressAlignment((ptrdiff_t)stride)));
        int rowLength = stride/pixelsize;
        gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, rowLength);
        gl->fTexImage2D(target,
                        level,
                        internalformat,
                        width,
                        height,
                        border,
                        format,
                        type,
                        pixels);
        gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH, 0);
        gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);
    }
}

static uint32_t
GetBytesPerTexel(GLenum format, GLenum type)
{
    // If there is no defined format or type, we're not taking up any memory
    if (!format || !type) {
        return 0;
    }

    if (format == LOCAL_GL_DEPTH_COMPONENT) {
        if (type == LOCAL_GL_UNSIGNED_SHORT)
            return 2;
        else if (type == LOCAL_GL_UNSIGNED_INT)
            return 4;
    } else if (format == LOCAL_GL_DEPTH_STENCIL) {
        if (type == LOCAL_GL_UNSIGNED_INT_24_8_EXT)
            return 4;
    }

    if (type == LOCAL_GL_UNSIGNED_BYTE || type == LOCAL_GL_FLOAT || type == LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV) {
        uint32_t multiplier = type == LOCAL_GL_UNSIGNED_BYTE ? 1 : 4;
        switch (format) {
            case LOCAL_GL_ALPHA:
            case LOCAL_GL_LUMINANCE:
                return 1 * multiplier;
            case LOCAL_GL_LUMINANCE_ALPHA:
                return 2 * multiplier;
            case LOCAL_GL_RGB:
                return 3 * multiplier;
            case LOCAL_GL_RGBA:
                return 4 * multiplier;
            default:
                break;
        }
    } else if (type == LOCAL_GL_UNSIGNED_SHORT_4_4_4_4 ||
               type == LOCAL_GL_UNSIGNED_SHORT_5_5_5_1 ||
               type == LOCAL_GL_UNSIGNED_SHORT_5_6_5)
    {
        return 2;
    }

    gfxCriticalError() << "Unknown texture type " << type << " or format " << format;
    MOZ_CRASH();
    return 0;
}

SurfaceFormat
UploadImageDataToTexture(GLContext* gl,
                         unsigned char* aData,
                         int32_t aStride,
                         SurfaceFormat aFormat,
                         const nsIntRegion& aDstRegion,
                         GLuint& aTexture,
                         size_t* aOutUploadSize,
                         bool aOverwrite,
                         bool aPixelBuffer,
                         GLenum aTextureUnit,
                         GLenum aTextureTarget)
{
    bool textureInited = aOverwrite ? false : true;
    gl->MakeCurrent();
    gl->fActiveTexture(aTextureUnit);

    if (!aTexture) {
        gl->fGenTextures(1, &aTexture);
        gl->fBindTexture(aTextureTarget, aTexture);
        gl->fTexParameteri(aTextureTarget,
                           LOCAL_GL_TEXTURE_MIN_FILTER,
                           LOCAL_GL_LINEAR);
        gl->fTexParameteri(aTextureTarget,
                           LOCAL_GL_TEXTURE_MAG_FILTER,
                           LOCAL_GL_LINEAR);
        gl->fTexParameteri(aTextureTarget,
                           LOCAL_GL_TEXTURE_WRAP_S,
                           LOCAL_GL_CLAMP_TO_EDGE);
        gl->fTexParameteri(aTextureTarget,
                           LOCAL_GL_TEXTURE_WRAP_T,
                           LOCAL_GL_CLAMP_TO_EDGE);
        textureInited = false;
    } else {
        gl->fBindTexture(aTextureTarget, aTexture);
    }

    nsIntRegion paintRegion;
    if (!textureInited) {
        paintRegion = nsIntRegion(aDstRegion.GetBounds());
    } else {
        paintRegion = aDstRegion;
    }

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
            internalFormat = format = LOCAL_GL_LUMINANCE;
            type = LOCAL_GL_UNSIGNED_BYTE;
            // We don't have a specific luminance shader
            surfaceFormat = SurfaceFormat::A8;
            break;
        default:
            NS_ASSERTION(false, "Unhandled image surface format!");
    }


    // Top left point of the region's bounding rectangle.
    IntPoint topLeft = paintRegion.GetBounds().TopLeft();

    if (aOutUploadSize) {
        *aOutUploadSize = 0;
    }

    for (auto iter = paintRegion.RectIter(); !iter.Done(); iter.Next()) {
        const IntRect& rect = iter.Get();
        // The inital data pointer is at the top left point of the region's
        // bounding rectangle. We need to find the offset of this rect
        // within the region and adjust the data pointer accordingly.
        unsigned char *rectData =
            aData + DataOffset(rect.TopLeft() - topLeft, aStride, aFormat);

        NS_ASSERTION(textureInited || (rect.x == 0 && rect.y == 0),
                     "Must be uploading to the origin when we don't have an existing texture");

        if (textureInited && CanUploadSubTextures(gl)) {
            TexSubImage2DHelper(gl,
                                aTextureTarget,
                                0,
                                rect.x,
                                rect.y,
                                rect.width,
                                rect.height,
                                aStride,
                                pixelSize,
                                format,
                                type,
                                rectData);
        } else {
            TexImage2DHelper(gl,
                             aTextureTarget,
                             0,
                             internalFormat,
                             rect.width,
                             rect.height,
                             aStride,
                             pixelSize,
                             0,
                             format,
                             type,
                             rectData);
        }

        if (aOutUploadSize && !textureInited) {
            uint32_t texelSize = GetBytesPerTexel(internalFormat, type);
            size_t numTexels = size_t(rect.width) * size_t(rect.height);
            *aOutUploadSize += texelSize * numTexels;
        }
    }

    return surfaceFormat;
}

SurfaceFormat
UploadSurfaceToTexture(GLContext* gl,
                       DataSourceSurface* aSurface,
                       const nsIntRegion& aDstRegion,
                       GLuint& aTexture,
                       size_t* aOutUploadSize,
                       bool aOverwrite,
                       const gfx::IntPoint& aSrcPoint,
                       bool aPixelBuffer,
                       GLenum aTextureUnit,
                       GLenum aTextureTarget)
{
    unsigned char* data = aPixelBuffer ? nullptr : aSurface->GetData();
    int32_t stride = aSurface->Stride();
    SurfaceFormat format = aSurface->GetFormat();
    data += DataOffset(aSrcPoint, stride, format);
    return UploadImageDataToTexture(gl, data, stride, format,
                                    aDstRegion, aTexture, aOutUploadSize,
                                    aOverwrite, aPixelBuffer, aTextureUnit,
                                    aTextureTarget);
}

bool
CanUploadNonPowerOfTwo(GLContext* gl)
{
    if (!gl->WorkAroundDriverBugs())
        return true;

    // Some GPUs driver crash when uploading non power of two 565 textures.
    return gl->Renderer() != GLRenderer::Adreno200 &&
           gl->Renderer() != GLRenderer::Adreno205;
}

} // namespace gl
} // namespace mozilla
