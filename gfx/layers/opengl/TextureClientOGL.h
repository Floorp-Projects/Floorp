/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENTOGL_H
#define MOZILLA_GFX_TEXTURECLIENTOGL_H

#include "GLContextTypes.h"             // for SharedTextureHandle, etc
#include "GLImages.h"
#include "gfxTypes.h"
#include "mozilla/Attributes.h"         // for override
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "AndroidSurfaceTexture.h"
#include "AndroidNativeWindow.h"
#ifdef MOZ_WIDGET_ANDROID
#include "GeneratedJNIWrappers.h"
#endif

namespace mozilla {

namespace layers {

#ifdef MOZ_WIDGET_ANDROID

class AndroidSurfaceTextureData : public TextureData
{
public:
  static already_AddRefed<TextureClient>
  CreateTextureClient(AndroidSurfaceTextureHandle aHandle,
                      gfx::IntSize aSize,
                      bool aContinuous,
                      gl::OriginPos aOriginPos,
                      LayersIPCChannel* aAllocator,
                      TextureFlags aFlags);

  ~AndroidSurfaceTextureData();

  virtual void FillInfo(TextureData::Info& aInfo) const override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  // Useless functions.
  virtual bool Lock(OpenMode) override { return true; }

  virtual void Unlock() override {}

  // Our data is always owned externally.
  virtual void Deallocate(LayersIPCChannel*) override {}

protected:
  AndroidSurfaceTextureData(AndroidSurfaceTextureHandle aHandle, gfx::IntSize aSize, bool aContinuous);

  const AndroidSurfaceTextureHandle mHandle;
  const gfx::IntSize mSize;
  const bool mContinuous;
};

#endif // MOZ_WIDGET_ANDROID

#ifdef MOZ_WIDGET_ANDROID

class AndroidNativeWindowTextureData : public TextureData
{
public:
  static AndroidNativeWindowTextureData* Create(gfx::IntSize aSize, SurfaceFormat aFormat);

  virtual void FillInfo(TextureData::Info& aInfo) const override;

  virtual bool Serialize(SurfaceDescriptor& aOutDescriptor) override;

  virtual bool Lock(OpenMode) override;
  virtual void Unlock() override;

  virtual void Forget(LayersIPCChannel*) override;
  virtual void Deallocate(LayersIPCChannel*) override {}

  virtual already_AddRefed<DrawTarget> BorrowDrawTarget() override;

  virtual void OnForwardedToHost() override;

protected:
  AndroidNativeWindowTextureData(java::GeckoSurface::Param aSurface,
                                 gfx::IntSize aSize,
                                 SurfaceFormat aFormat);

private:
  java::GeckoSurface::GlobalRef mSurface;
  ANativeWindow* mNativeWindow;
  ANativeWindow_Buffer mBuffer;
  // Keeps track of whether the underlying NativeWindow is actually locked.
  bool mIsLocked;

  const gfx::IntSize mSize;
  const SurfaceFormat mFormat;
};

#endif // MOZ_WIDGET_ANDROID

} // namespace layers
} // namespace mozilla

#endif
