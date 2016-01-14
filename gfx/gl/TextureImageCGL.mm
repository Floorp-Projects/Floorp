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
                const IntSize& aSize,
                GLenum aWrapMode,
                ContentType aContentType,
                GLContext* aContext,
                TextureImage::Flags aFlags)
    : BasicTextureImage(aTexture, aSize, aWrapMode, aContentType,
                        aContext, aFlags)
    , mPixelBuffer(0)
    , mBoundPixelBuffer(false)
{}

TextureImageCGL::~TextureImageCGL()
{
    if (mPixelBuffer) {
        mGLContext->MakeCurrent();
        mGLContext->fDeleteBuffers(1, &mPixelBuffer);
    }
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
      RefPtr<TextureImage> t = new gl::TiledTextureImage(gl, aSize, aContentType,
                                                           aFlags, aImageFormat);
      return t.forget();
    }

    return CreateBasicTextureImage(gl, aSize, aContentType, aWrapMode,
                                   aFlags);
}

already_AddRefed<TextureImage>
TileGenFuncCGL(GLContext *gl,
               const IntSize& aSize,
               TextureImage::ContentType aContentType,
               TextureImage::Flags aFlags)
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

    RefPtr<TextureImageCGL> teximage
        (new TextureImageCGL(texture, aSize, LOCAL_GL_CLAMP_TO_EDGE, aContentType,
                             gl, aFlags));
    return teximage.forget();
}

} // namespace gl
} // namespace mozilla
