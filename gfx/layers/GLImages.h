/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GLIMAGES_H
#define GFX_GLIMAGES_H

#include "GLContextTypes.h"
#include "GLTypes.h"
#include "ImageContainer.h"             // for Image
#include "ImageTypes.h"                 // for ImageFormat::SHARED_GLTEXTURE
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "mozilla/gfx/Point.h"          // for IntSize

namespace mozilla {
namespace gl {
class AndroidSurfaceTexture;
} // namespace gl
namespace layers {

class GLImage : public Image {
public:
  explicit GLImage(ImageFormat aFormat) : Image(nullptr, aFormat){}

  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;
};

class EGLImageImage : public GLImage {
public:
  struct Data {
    EGLImage mImage;
    EGLSync mSync;
    gfx::IntSize mSize;
    gl::OriginPos mOriginPos;
    bool mOwns;

    Data() : mImage(nullptr), mSync(nullptr), mSize(0, 0),
             mOriginPos(gl::OriginPos::TopLeft), mOwns(false)
    {
    }
  };

  void SetData(const Data& aData) { mData = aData; }
  const Data* GetData() { return &mData; }

  gfx::IntSize GetSize() { return mData.mSize; }

  EGLImageImage() : GLImage(ImageFormat::EGLIMAGE) {}

protected:
  virtual ~EGLImageImage();

private:
  Data mData;
};

#ifdef MOZ_WIDGET_ANDROID

class SurfaceTextureImage : public GLImage {
public:
  struct Data {
    mozilla::gl::AndroidSurfaceTexture* mSurfTex;
    gfx::IntSize mSize;
    gl::OriginPos mOriginPos;
  };

  void SetData(const Data& aData) { mData = aData; }
  const Data* GetData() { return &mData; }

  gfx::IntSize GetSize() { return mData.mSize; }

  SurfaceTextureImage() : GLImage(ImageFormat::SURFACE_TEXTURE) {}

private:
  Data mData;
};

#endif // MOZ_WIDGET_ANDROID

} // namespace layers
} // namespace mozilla

#endif // GFX_GLIMAGES_H
