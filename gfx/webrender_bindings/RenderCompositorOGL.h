/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERCOMPOSITOR_OGL_H
#define MOZILLA_GFX_RENDERCOMPOSITOR_OGL_H

#include "mozilla/webrender/RenderCompositor.h"

namespace mozilla {

namespace layers {
class NativeLayerRoot;
class NativeLayer;
}  // namespace layers

namespace wr {

class RenderCompositorOGL : public RenderCompositor {
 public:
  static UniquePtr<RenderCompositor> Create(
      RefPtr<widget::CompositorWidget>&& aWidget);

  RenderCompositorOGL(RefPtr<gl::GLContext>&& aGL,
                      RefPtr<widget::CompositorWidget>&& aWidget);
  virtual ~RenderCompositorOGL();

  bool BeginFrame() override;
  void EndFrame() override;
  bool WaitForGPU() override;
  void Pause() override;
  bool Resume() override;

  gl::GLContext* gl() const override { return mGL; }

  bool UseANGLE() const override { return false; }

  LayoutDeviceIntSize GetBufferSize() override;

  bool ShouldUseNativeCompositor() override;

  // Interface for wr::Compositor
  void CompositorBeginFrame() override;
  void CompositorEndFrame() override;
  void Bind(wr::NativeSurfaceId aId, wr::DeviceIntPoint* aOffset,
            uint32_t* aFboId, wr::DeviceIntRect aDirtyRect) override;
  void Unbind() override;
  void CreateSurface(wr::NativeSurfaceId aId, wr::DeviceIntSize aSize) override;
  void DestroySurface(NativeSurfaceId aId) override;
  void AddSurface(wr::NativeSurfaceId aId, wr::DeviceIntPoint aPosition,
                  wr::DeviceIntRect aClipRect) override;

 protected:
  void InsertFrameDoneSync();

  RefPtr<gl::GLContext> mGL;

  // Can be null.
  RefPtr<layers::NativeLayerRoot> mNativeLayerRoot;
  RefPtr<layers::NativeLayer> mNativeLayerForEntireWindow;

  // Used in native compositor mode:
  RefPtr<layers::NativeLayer> mCurrentlyBoundNativeLayer;
  nsTArray<RefPtr<layers::NativeLayer>> mAddedLayers;
  uint64_t mAddedPixelCount = 0;
  uint64_t mAddedClippedPixelCount = 0;
  uint64_t mDrawnPixelCount = 0;
  gfx::IntRect mVisibleBounds;
  std::unordered_map<uint64_t, RefPtr<layers::NativeLayer>> mNativeLayers;

  // Used to apply back-pressure in WaitForGPU().
  GLsync mPreviousFrameDoneSync;
  GLsync mThisFrameDoneSync;
};

}  // namespace wr
}  // namespace mozilla

#endif
