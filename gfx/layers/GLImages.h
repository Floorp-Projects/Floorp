/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GLIMAGES_H
#define GFX_GLIMAGES_H

#include "GLTypes.h"
#include "ImageContainer.h"             // for Image
#include "ImageTypes.h"                 // for ImageFormat::SHARED_GLTEXTURE
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "mozilla/gfx/Point.h"          // for IntSize

namespace mozilla {
namespace gl {
class AndroidSurfaceTexture;
}
namespace layers {

class EGLImageImage : public Image {
public:
  struct Data {
    EGLImage mImage;
    gfx::IntSize mSize;
    bool mInverted;
  };

  void SetData(const Data& aData) { mData = aData; }
  const Data* GetData() { return &mData; }

  gfx::IntSize GetSize() { return mData.mSize; }

  virtual TemporaryRef<gfx::SourceSurface> GetAsSourceSurface() MOZ_OVERRIDE
  {
    return nullptr;
  }

  EGLImageImage() : Image(nullptr, ImageFormat::EGLIMAGE) {}

private:
  Data mData;
};

#ifdef MOZ_WIDGET_ANDROID

class SurfaceTextureImage : public Image {
public:
  struct Data {
    mozilla::gl::AndroidSurfaceTexture* mSurfTex;
    gfx::IntSize mSize;
    bool mInverted;
  };

  void SetData(const Data& aData) { mData = aData; }
  const Data* GetData() { return &mData; }

  gfx::IntSize GetSize() { return mData.mSize; }

  virtual TemporaryRef<gfx::SourceSurface> GetAsSourceSurface() MOZ_OVERRIDE
  {
    return nullptr;
  }

  SurfaceTextureImage() : Image(nullptr, ImageFormat::SURFACE_TEXTURE) {}

private:
  Data mData;
};

#endif // MOZ_WIDGET_ANDROID

} // layers
} // mozilla

#endif // GFX_GLIMAGES_H
