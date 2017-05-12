/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GLIMAGES_H
#define GFX_GLIMAGES_H

#include "AndroidSurfaceTexture.h"
#include "GLContextTypes.h"
#include "GLTypes.h"
#include "ImageContainer.h"             // for Image
#include "ImageTypes.h"                 // for ImageFormat::SHARED_GLTEXTURE
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "mozilla/gfx/Point.h"          // for IntSize

namespace mozilla {
namespace layers {

class GLImage : public Image {
public:
  explicit GLImage(ImageFormat aFormat) : Image(nullptr, aFormat){}

  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  GLImage* AsGLImage() override {
    return this;
  }
};

class EGLImageImage : public GLImage {
public:
  EGLImageImage(EGLImage aImage, EGLSync aSync,
                const gfx::IntSize& aSize, const gl::OriginPos& aOrigin,
                bool aOwns);

  gfx::IntSize GetSize() override { return mSize; }
  gl::OriginPos GetOriginPos() const {
    return mPos;
  }
  EGLImage GetImage() const {
    return mImage;
  }
  EGLSync GetSync() const {
    return mSync;
  }

  EGLImageImage* AsEGLImageImage() override {
    return this;
  }

protected:
  virtual ~EGLImageImage();

private:
  EGLImage mImage;
  EGLSync mSync;
  gfx::IntSize mSize;
  gl::OriginPos mPos;
  bool mOwns;
};

#ifdef MOZ_WIDGET_ANDROID

class SurfaceTextureImage : public GLImage {
public:
  SurfaceTextureImage(gl::AndroidSurfaceTexture* aSurfTex,
                      const gfx::IntSize& aSize,
                      gl::OriginPos aOriginPos);

  gfx::IntSize GetSize() override { return mSize; }
  gl::AndroidSurfaceTexture* GetSurfaceTexture() const {
    return mSurfaceTexture;
  }
  gl::OriginPos GetOriginPos() const {
    return mOriginPos;
  }

  SurfaceTextureImage* AsSurfaceTextureImage() override {
    return this;
  }

private:
  RefPtr<gl::AndroidSurfaceTexture> mSurfaceTexture;
  gfx::IntSize mSize;
  gl::OriginPos mOriginPos;
};

#endif // MOZ_WIDGET_ANDROID

} // namespace layers
} // namespace mozilla

#endif // GFX_GLIMAGES_H
