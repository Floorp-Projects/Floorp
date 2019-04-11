/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENTOGL_H
#define MOZILLA_GFX_TEXTURECLIENTOGL_H

#include "GLContextTypes.h"  // for SharedTextureHandle, etc
#include "GLImages.h"
#include "gfxTypes.h"
#include "mozilla/Attributes.h"  // for override
#include "mozilla/gfx/Point.h"   // for IntSize
#include "mozilla/gfx/Types.h"   // for SurfaceFormat
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureClient.h"   // for TextureClient, etc
#include "AndroidSurfaceTexture.h"
#include "AndroidNativeWindow.h"
#ifdef MOZ_WIDGET_ANDROID
#  include "GeneratedJNIWrappers.h"
#endif

namespace mozilla {

namespace gfx {

class DrawTarget;

}  // namespace gfx

namespace layers {

#ifdef MOZ_WIDGET_ANDROID

class AndroidSurfaceTextureData : public TextureData {
 public:
  static already_AddRefed<TextureClient> CreateTextureClient(
      AndroidSurfaceTextureHandle aHandle, gfx::IntSize aSize, bool aContinuous,
      gl::OriginPos aOriginPos, bool aHasAlpha, LayersIPCChannel* aAllocator,
      TextureFlags aFlags);

  virtual ~AndroidSurfaceTextureData();

  void FillInfo(TextureData::Info& aInfo) const override;

  bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  // Useless functions.
  bool Lock(OpenMode) override { return true; }

  void Unlock() override {}

  // Our data is always owned externally.
  void Deallocate(LayersIPCChannel*) override {}

 protected:
  AndroidSurfaceTextureData(AndroidSurfaceTextureHandle aHandle,
                            gfx::IntSize aSize, bool aContinuous,
                            bool aHasAlpha);

  const AndroidSurfaceTextureHandle mHandle;
  const gfx::IntSize mSize;
  const bool mContinuous;
  const bool mHasAlpha;
};

class AndroidNativeWindowTextureData : public TextureData {
 public:
  static AndroidNativeWindowTextureData* Create(gfx::IntSize aSize,
                                                gfx::SurfaceFormat aFormat);

  void FillInfo(TextureData::Info& aInfo) const override;

  bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  bool Lock(OpenMode) override;
  void Unlock() override;

  void Forget(LayersIPCChannel*) override;
  void Deallocate(LayersIPCChannel*) override {}

  already_AddRefed<gfx::DrawTarget> BorrowDrawTarget() override;

  void OnForwardedToHost() override;

 protected:
  AndroidNativeWindowTextureData(java::GeckoSurface::Param aSurface,
                                 gfx::IntSize aSize,
                                 gfx::SurfaceFormat aFormat);

 private:
  java::GeckoSurface::GlobalRef mSurface;
  ANativeWindow* mNativeWindow;
  ANativeWindow_Buffer mBuffer;
  // Keeps track of whether the underlying NativeWindow is actually locked.
  bool mIsLocked;

  const gfx::IntSize mSize;
  const gfx::SurfaceFormat mFormat;
};

#endif  // MOZ_WIDGET_ANDROID

}  // namespace layers
}  // namespace mozilla

#endif
