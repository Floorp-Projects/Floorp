/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLUploadHelpers.h"

#include "GLContext.h"
#include "mozilla/gfx/2D.h"
#include "gfxASurface.h"
#include "gfxUtils.h"
#include "gfxContext.h"
#include "nsRegion.h"

namespace mozilla {

using gfx::SurfaceFormat;

namespace gl {

static unsigned int
DataOffset(const nsIntPoint &aPoint, int32_t aStride, gfxImageFormat aFormat)
{
  unsigned int data = aPoint.y * aStride;
  data += aPoint.x * gfxASurface::BytePerPixelFromFormat(aFormat);
  return data;
}

static gfxImageFormat
ImageFormatForSurfaceFormat(gfx::SurfaceFormat aFormat)
{
    switch (aFormat) {
        case gfx::FORMAT_B8G8R8A8:
            return gfxImageFormatARGB32;
        case gfx::FORMAT_B8G8R8X8:
            return gfxImageFormatRGB24;
        case gfx::FORMAT_R5G6B5:
            return gfxImageFormatRGB16_565;
        case gfx::FORMAT_A8:
            return gfxImageFormatA8;
        default:
            return gfxImageFormatUnknown;
    }
}

SurfaceFormat
UploadImageDataToTexture(GLContext* gl,
                         unsigned char* aData,
                         int32_t aStride,
                         gfxImageFormat aFormat,
                         const nsIntRegion& aDstRegion,
                         GLuint& aTexture,
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

    GLenum format;
    GLenum internalFormat;
    GLenum type;
    int32_t pixelSize = gfxASurface::BytePerPixelFromFormat(aFormat);
    SurfaceFormat surfaceFormat;

    MOZ_ASSERT(gl->GetPreferredARGB32Format() == LOCAL_GL_BGRA ||
               gl->GetPreferredARGB32Format() == LOCAL_GL_RGBA);
    switch (aFormat) {
        case gfxImageFormatARGB32:
            if (gl->GetPreferredARGB32Format() == LOCAL_GL_BGRA) {
              format = LOCAL_GL_BGRA;
              surfaceFormat = gfx::FORMAT_R8G8B8A8;
              type = LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV;
            } else {
              format = LOCAL_GL_RGBA;
              surfaceFormat = gfx::FORMAT_B8G8R8A8;
              type = LOCAL_GL_UNSIGNED_BYTE;
            }
            internalFormat = LOCAL_GL_RGBA;
            break;
        case gfxImageFormatRGB24:
            // Treat RGB24 surfaces as RGBA32 except for the surface
            // format used.
            if (gl->GetPreferredARGB32Format() == LOCAL_GL_BGRA) {
              format = LOCAL_GL_BGRA;
              surfaceFormat = gfx::FORMAT_R8G8B8X8;
              type = LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV;
            } else {
              format = LOCAL_GL_RGBA;
              surfaceFormat = gfx::FORMAT_B8G8R8X8;
              type = LOCAL_GL_UNSIGNED_BYTE;
            }
            internalFormat = LOCAL_GL_RGBA;
            break;
        case gfxImageFormatRGB16_565:
            internalFormat = format = LOCAL_GL_RGB;
            type = LOCAL_GL_UNSIGNED_SHORT_5_6_5;
            surfaceFormat = gfx::FORMAT_R5G6B5;
            break;
        case gfxImageFormatA8:
            internalFormat = format = LOCAL_GL_LUMINANCE;
            type = LOCAL_GL_UNSIGNED_BYTE;
            // We don't have a specific luminance shader
            surfaceFormat = gfx::FORMAT_A8;
            break;
        default:
            NS_ASSERTION(false, "Unhandled image surface format!");
            format = 0;
            type = 0;
            surfaceFormat = gfx::FORMAT_UNKNOWN;
    }

    nsIntRegionRectIterator iter(paintRegion);
    const nsIntRect *iterRect;

    // Top left point of the region's bounding rectangle.
    nsIntPoint topLeft = paintRegion.GetBounds().TopLeft();

    while ((iterRect = iter.Next())) {
        // The inital data pointer is at the top left point of the region's
        // bounding rectangle. We need to find the offset of this rect
        // within the region and adjust the data pointer accordingly.
        unsigned char *rectData =
            aData + DataOffset(iterRect->TopLeft() - topLeft, aStride, aFormat);

        NS_ASSERTION(textureInited || (iterRect->x == 0 && iterRect->y == 0),
                     "Must be uploading to the origin when we don't have an existing texture");

        if (textureInited && gl->CanUploadSubTextures()) {
            gl->TexSubImage2D(aTextureTarget,
                              0,
                              iterRect->x,
                              iterRect->y,
                              iterRect->width,
                              iterRect->height,
                              aStride,
                              pixelSize,
                              format,
                              type,
                              rectData);
        } else {
            gl->TexImage2D(aTextureTarget,
                           0,
                           internalFormat,
                           iterRect->width,
                           iterRect->height,
                           aStride,
                           pixelSize,
                           0,
                           format,
                           type,
                          rectData);
        }

    }

    return surfaceFormat;
}

SurfaceFormat
UploadSurfaceToTexture(GLContext* gl,
                       gfxASurface *aSurface,
                       const nsIntRegion& aDstRegion,
                       GLuint& aTexture,
                       bool aOverwrite,
                       const nsIntPoint& aSrcPoint,
                       bool aPixelBuffer,
                       GLenum aTextureUnit,
                       GLenum aTextureTarget)
{

    nsRefPtr<gfxImageSurface> imageSurface = aSurface->GetAsImageSurface();
    unsigned char* data = nullptr;

    if (!imageSurface ||
        (imageSurface->Format() != gfxImageFormatARGB32 &&
         imageSurface->Format() != gfxImageFormatRGB24 &&
         imageSurface->Format() != gfxImageFormatRGB16_565 &&
         imageSurface->Format() != gfxImageFormatA8)) {
        // We can't get suitable pixel data for the surface, make a copy
        nsIntRect bounds = aDstRegion.GetBounds();
        imageSurface =
          new gfxImageSurface(gfxIntSize(bounds.width, bounds.height),
                              gfxImageFormatARGB32);

        nsRefPtr<gfxContext> context = new gfxContext(imageSurface);

        context->Translate(-gfxPoint(aSrcPoint.x, aSrcPoint.y));
        context->SetSource(aSurface);
        context->Paint();
        data = imageSurface->Data();
        NS_ASSERTION(!aPixelBuffer,
                     "Must be using an image compatible surface with pixel buffers!");
    } else {
        // If a pixel buffer is bound the data pointer parameter is relative
        // to the start of the data block.
        if (!aPixelBuffer) {
              data = imageSurface->Data();
        }
        data += DataOffset(aSrcPoint, imageSurface->Stride(),
                           imageSurface->Format());
    }

    MOZ_ASSERT(imageSurface);
    imageSurface->Flush();

    return UploadImageDataToTexture(gl,
                                    data,
                                    imageSurface->Stride(),
                                    imageSurface->Format(),
                                    aDstRegion, aTexture, aOverwrite,
                                    aPixelBuffer, aTextureUnit, aTextureTarget);
}

SurfaceFormat
UploadSurfaceToTexture(GLContext* gl,
                       gfx::DataSourceSurface *aSurface,
                       const nsIntRegion& aDstRegion,
                       GLuint& aTexture,
                       bool aOverwrite,
                       const nsIntPoint& aSrcPoint,
                       bool aPixelBuffer,
                       GLenum aTextureUnit,
                       GLenum aTextureTarget)
{
    unsigned char* data = aPixelBuffer ? nullptr : aSurface->GetData();
    int32_t stride = aSurface->Stride();
    gfxImageFormat format =
        ImageFormatForSurfaceFormat(aSurface->GetFormat());
    data += DataOffset(aSrcPoint, stride, format);
    return UploadImageDataToTexture(gl, data, stride, format,
                                    aDstRegion, aTexture, aOverwrite,
                                    aPixelBuffer, aTextureUnit,
                                    aTextureTarget);
}

}
}