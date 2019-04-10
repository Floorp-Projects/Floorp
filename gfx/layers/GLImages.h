/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GLIMAGES_H
#define GFX_GLIMAGES_H

#include "AndroidSurfaceTexture.h"
#include "GLContextTypes.h"
#include "GLTypes.h"
#include "ImageContainer.h"     // for Image
#include "ImageTypes.h"         // for ImageFormat::SHARED_GLTEXTURE
#include "nsCOMPtr.h"           // for already_AddRefed
#include "mozilla/gfx/Point.h"  // for IntSize

namespace mozilla {
namespace layers {

class GLImage : public Image {
 public:
  explicit GLImage(ImageFormat aFormat) : Image(nullptr, aFormat) {}

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  GLImage* AsGLImage() override { return this; }
};

#ifdef MOZ_WIDGET_ANDROID

class SurfaceTextureImage : public GLImage {
 public:
  SurfaceTextureImage(AndroidSurfaceTextureHandle aHandle,
                      const gfx::IntSize& aSize, bool aContinuous,
                      gl::OriginPos aOriginPos, bool aHasAlpha = true);

  gfx::IntSize GetSize() const override { return mSize; }
  AndroidSurfaceTextureHandle GetHandle() const { return mHandle; }
  bool GetContinuous() const { return mContinuous; }
  gl::OriginPos GetOriginPos() const { return mOriginPos; }
  bool GetHasAlpha() const { return mHasAlpha; }

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override {
    // We can implement this, but currently don't want to because it will cause
    // the SurfaceTexture to be permanently bound to the snapshot readback
    // context.
    return nullptr;
  }

  SurfaceTextureImage* AsSurfaceTextureImage() override { return this; }

 private:
  AndroidSurfaceTextureHandle mHandle;
  gfx::IntSize mSize;
  bool mContinuous;
  gl::OriginPos mOriginPos;
  const bool mHasAlpha;
};

#endif  // MOZ_WIDGET_ANDROID

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_GLIMAGES_H
