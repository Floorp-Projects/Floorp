/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureImageCGL.h"
#include "GLContext.h"
#include "gfx2DGlue.h"
#include "gfxQuartzSurface.h"
#include "gfxPlatform.h"
#include "gfxFailure.h"

namespace mozilla {

using namespace gfx;

namespace gl {

TextureImageCGL::TextureImageCGL(GLuint aTexture,
                const nsIntSize& aSize,
                GLenum aWrapMode,
                ContentType aContentType,
                GLContext* aContext,
                TextureImage::Flags aFlags,
                TextureImage::ImageFormat aImageFormat)
    : BasicTextureImage(aTexture, aSize, aWrapMode, aContentType,
                        aContext, aFlags, aImageFormat)
    , mPixelBuffer(0)
    , mPixelBufferSize(0)
    , mBoundPixelBuffer(false)
{}

TextureImageCGL::~TextureImageCGL()
{
    if (mPixelBuffer) {
        mGLContext->MakeCurrent();
        mGLContext->fDeleteBuffers(1, &mPixelBuffer);
    }
}

already_AddRefed<gfxASurface>
TextureImageCGL::GetSurfaceForUpdate(const gfxIntSize& aSize, ImageFormat aFmt)
{
    IntSize size(aSize.width + 1, aSize.height + 1);
    mGLContext->MakeCurrent();
    if (!mGLContext->
        IsExtensionSupported(GLContext::ARB_pixel_buffer_object))
    {
        return gfxPlatform::GetPlatform()->
            CreateOffscreenSurface(size,
                                    gfxASurface::ContentFromFormat(aFmt));
    }

    if (!mPixelBuffer) {
        mGLContext->fGenBuffers(1, &mPixelBuffer);
    }
    mGLContext->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, mPixelBuffer);
    int32_t length = size.width * 4 * size.height;

    if (length > mPixelBufferSize) {
        mGLContext->fBufferData(LOCAL_GL_PIXEL_UNPACK_BUFFER, length,
                                NULL, LOCAL_GL_STREAM_DRAW);
        mPixelBufferSize = length;
    }
    unsigned char* data =
        (unsigned char*)mGLContext->
            fMapBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER,
                        LOCAL_GL_WRITE_ONLY);

    mGLContext->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);

    if (!data) {
        nsAutoCString failure;
        failure += "Pixel buffer binding failed: ";
        failure.AppendPrintf("%dx%d\n", size.width, size.height);
        gfx::LogFailure(failure);

        mGLContext->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);
        return gfxPlatform::GetPlatform()->
            CreateOffscreenSurface(size,
                                    gfxASurface::ContentFromFormat(aFmt));
    }

    nsRefPtr<gfxQuartzSurface> surf =
        new gfxQuartzSurface(data, ThebesIntSize(size), size.width * 4, aFmt);

    mBoundPixelBuffer = true;
    return surf.forget();
}

bool
TextureImageCGL::FinishedSurfaceUpdate()
{
    if (mBoundPixelBuffer) {
        mGLContext->MakeCurrent();
        mGLContext->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, mPixelBuffer);
        mGLContext->fUnmapBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER);
        return true;
    }
    return false;
}

void
TextureImageCGL::FinishedSurfaceUpload()
{
    if (mBoundPixelBuffer) {
        mGLContext->MakeCurrent();
        mGLContext->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);
        mBoundPixelBuffer = false;
    }
}

already_AddRefed<TextureImage>
CreateTextureImageCGL(GLContext* gl,
                      const gfx::IntSize& aSize,
                      TextureImage::ContentType aContentType,
                      GLenum aWrapMode,
                      TextureImage::Flags aFlags,
                      TextureImage::ImageFormat aImageFormat)
{
    if (!gl->IsOffscreenSizeAllowed(aSize) &&
        gfxPlatform::OffMainThreadCompositingEnabled()) {
      NS_ASSERTION(aWrapMode == LOCAL_GL_CLAMP_TO_EDGE, "Can't support wrapping with tiles!");
      nsRefPtr<TextureImage> t = new gl::TiledTextureImage(gl, aSize, aContentType,
                                                           aFlags, aImageFormat);
      return t.forget();
    }

    return CreateBasicTextureImage(gl, aSize, aContentType, aWrapMode,
                                   aFlags, aImageFormat);
}

already_AddRefed<TextureImage>
TileGenFuncCGL(GLContext *gl,
               const nsIntSize& aSize,
               TextureImage::ContentType aContentType,
               TextureImage::Flags aFlags,
               TextureImage::ImageFormat aImageFormat)
{
    bool useNearestFilter = aFlags & TextureImage::UseNearestFilter;
    gl->MakeCurrent();

    GLuint texture;
    gl->fGenTextures(1, &texture);

    gl->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl->fBindTexture(LOCAL_GL_TEXTURE_2D, texture);

    GLint texfilter = useNearestFilter ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR;
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, texfilter);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, texfilter);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

    nsRefPtr<TextureImageCGL> teximage
        (new TextureImageCGL(texture, aSize, LOCAL_GL_CLAMP_TO_EDGE, aContentType,
                             gl, aFlags, aImageFormat));
    return teximage.forget();
}

}
}
