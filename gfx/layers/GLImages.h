/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GLIMAGES_H
#define GFX_GLIMAGES_H

#include "GLContextTypes.h"
#include "GLTypes.h"
#include "ImageContainer.h"      // for Image
#include "ImageTypes.h"          // for ImageFormat::SHARED_GLTEXTURE
#include "nsCOMPtr.h"            // for already_AddRefed
#include "mozilla/Maybe.h"       // for Maybe
#include "mozilla/gfx/Matrix.h"  // for Matrix4x4
#include "mozilla/gfx/Point.h"   // for IntSize

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidSurfaceTexture.h"
#endif

namespace mozilla {
namespace layers {

class GLImage : public Image {
 public:
  explicit GLImage(ImageFormat aFormat) : Image(nullptr, aFormat) {}

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  nsresult BuildSurfaceDescriptorBuffer(
      SurfaceDescriptorBuffer& aSdBuffer, BuildSdbFlags aFlags,
      const std::function<MemoryOrShmem(uint32_t)>& aAllocate) override;

  GLImage* AsGLImage() override { return this; }

 protected:
  nsresult ReadIntoBuffer(uint8_t* aData, int32_t aStride,
                          const gfx::IntSize& aSize,
                          gfx::SurfaceFormat aFormat);
};

#ifdef MOZ_WIDGET_ANDROID

class SurfaceTextureImage final : public GLImage {
 public:
  class SetCurrentCallback {
   public:
    virtual void operator()(void) = 0;
    virtual ~SetCurrentCallback() {}
  };

  SurfaceTextureImage(AndroidSurfaceTextureHandle aHandle,
                      const gfx::IntSize& aSize, bool aContinuous,
                      gl::OriginPos aOriginPos, bool aHasAlpha,
                      Maybe<gfx::Matrix4x4> aTransformOverride);

  gfx::IntSize GetSize() const override { return mSize; }
  AndroidSurfaceTextureHandle GetHandle() const { return mHandle; }
  bool GetContinuous() const { return mContinuous; }
  gl::OriginPos GetOriginPos() const { return mOriginPos; }
  bool GetHasAlpha() const { return mHasAlpha; }
  const Maybe<gfx::Matrix4x4>& GetTransformOverride() const {
    return mTransformOverride;
  }

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override {
    // We can implement this, but currently don't want to because it will cause
    // the SurfaceTexture to be permanently bound to the snapshot readback
    // context.
    return nullptr;
  }

  nsresult BuildSurfaceDescriptorBuffer(
      SurfaceDescriptorBuffer& aSdBuffer, BuildSdbFlags aFlags,
      const std::function<MemoryOrShmem(uint32_t)>& aAllocate) override {
    // We can implement this, but currently don't want to because it will cause
    // the SurfaceTexture to be permanently bound to the snapshot readback
    // context.
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  SurfaceTextureImage* AsSurfaceTextureImage() override { return this; }

  Maybe<SurfaceDescriptor> GetDesc() override;

  void RegisterSetCurrentCallback(UniquePtr<SetCurrentCallback> aCallback) {
    mSetCurrentCallback = std::move(aCallback);
  }

  void OnSetCurrent() {
    if (mSetCurrentCallback) {
      (*mSetCurrentCallback)();
      mSetCurrentCallback.reset();
    }
  }

 private:
  AndroidSurfaceTextureHandle mHandle;
  gfx::IntSize mSize;
  bool mContinuous;
  gl::OriginPos mOriginPos;
  const bool mHasAlpha;
  const Maybe<gfx::Matrix4x4> mTransformOverride;
  UniquePtr<SetCurrentCallback> mSetCurrentCallback;
};

#endif  // MOZ_WIDGET_ANDROID

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_GLIMAGES_H
