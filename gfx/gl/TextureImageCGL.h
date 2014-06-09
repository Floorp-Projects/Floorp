/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TextureImageCGL_h_
#define TextureImageCGL_h_

#include "GLTextureImage.h"
#include "GLContextTypes.h"
#include "nsAutoPtr.h"
#include "nsSize.h"

class gfxASurface;

namespace mozilla {
namespace gl {

class TextureImageCGL : public BasicTextureImage
{
public:

    TextureImageCGL(GLuint aTexture,
                    const nsIntSize& aSize,
                    GLenum aWrapMode,
                    ContentType aContentType,
                    GLContext* aContext,
                    TextureImage::Flags aFlags = TextureImage::NoFlags,
                    TextureImage::ImageFormat aImageFormat = gfxImageFormat::Unknown);

    ~TextureImageCGL();

protected:
    bool FinishedSurfaceUpdate();

    void FinishedSurfaceUpload();

private:

    GLuint mPixelBuffer;
    bool mBoundPixelBuffer;
};

already_AddRefed<TextureImage>
CreateTextureImageCGL(GLContext *gl,
                      const gfx::IntSize& aSize,
                      TextureImage::ContentType aContentType,
                      GLenum aWrapMode,
                      TextureImage::Flags aFlags,
                      TextureImage::ImageFormat aImageFormat);

already_AddRefed<TextureImage>
TileGenFuncCGL(GLContext *gl,
               const nsIntSize& aSize,
               TextureImage::ContentType aContentType,
               TextureImage::Flags aFlags,
               TextureImage::ImageFormat aImageFormat);

}
}

#endif
