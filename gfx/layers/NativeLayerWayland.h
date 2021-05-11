/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_NativeLayerWayland_h
#define mozilla_layers_NativeLayerWayland_h

#include <deque>
#include <unordered_map>

#include "mozilla/Mutex.h"

#include "mozilla/layers/NativeLayer.h"
#include "mozilla/layers/SurfacePoolWayland.h"
#include "mozilla/widget/MozContainerWayland.h"
#include "nsRegion.h"
#include "nsISupportsImpl.h"

namespace mozilla {

namespace gl {
class GLContextEGL;
}  // namespace gl

namespace layers {

class NativeLayerRootWayland : public NativeLayerRoot {
 public:
  static already_AddRefed<NativeLayerRootWayland> CreateForMozContainer(
      MozContainer* aContainer);

  // Overridden methods
  already_AddRefed<NativeLayer> CreateLayer(
      const gfx::IntSize& aSize, bool aIsOpaque,
      SurfacePoolHandle* aSurfacePoolHandle) override;
  void AppendLayer(NativeLayer* aLayer) override;
  void RemoveLayer(NativeLayer* aLayer) override;
  void SetLayers(const nsTArray<RefPtr<NativeLayer>>& aLayers) override;
  UniquePtr<NativeLayerRootSnapshotter> CreateSnapshotter() override;
  bool CommitToScreen() override;

  // When the compositor is paused the wl_surface of the MozContainer will
  // get destroyed. We thus have to recreate subsurface relationships for
  // all tiles after resume. This is a implementation specific quirk of
  // our GTK-Wayland backend.
  void PauseCompositor() override;
  bool ResumeCompositor() override;

  void SetBackingScale(float aBackingScale);
  float BackingScale();

  already_AddRefed<NativeLayer> CreateLayerForExternalTexture(
      bool aIsOpaque) override;

 protected:
  explicit NativeLayerRootWayland(MozContainer* aContainer);
  ~NativeLayerRootWayland() = default;

  void EnsureSurfaceInitialized();
  void EnsureShowLayer(const RefPtr<NativeLayerWayland>& aLayer);
  void EnsureHideLayer(const RefPtr<NativeLayerWayland>& aLayer);
  void UnmapLayer(const RefPtr<NativeLayerWayland>& aLayer);

  Mutex mMutex;

  nsTArray<RefPtr<NativeLayerWayland>> mSublayers;
  float mBackingScale = 1.0f;
  MozContainer* mContainer = nullptr;
  bool mInitialized = false;
  wl_egl_window* mEGLWindow = nullptr;
  EGLSurface mEGLSurface = nullptr;
  struct wp_viewport* mViewport = nullptr;
};

class NativeLayerWayland : public NativeLayer {
 public:
  virtual NativeLayerWayland* AsNativeLayerWayland() override { return this; }

  // Overridden methods
  gfx::IntSize GetSize() override;
  void SetPosition(const gfx::IntPoint& aPosition) override;
  gfx::IntPoint GetPosition() override;
  void SetTransform(const gfx::Matrix4x4& aTransform) override;
  gfx::Matrix4x4 GetTransform() override;
  gfx::IntRect GetRect() override;
  void SetSamplingFilter(gfx::SamplingFilter aSamplingFilter) override;
  RefPtr<gfx::DrawTarget> NextSurfaceAsDrawTarget(
      const gfx::IntRect& aDisplayRect, const gfx::IntRegion& aUpdateRegion,
      gfx::BackendType aBackendType) override;
  Maybe<GLuint> NextSurfaceAsFramebuffer(const gfx::IntRect& aDisplayRect,
                                         const gfx::IntRegion& aUpdateRegion,
                                         bool aNeedsDepth) override;
  void NotifySurfaceReady() override;
  void DiscardBackbuffers() override;
  bool IsOpaque() override;
  void SetClipRect(const Maybe<gfx::IntRect>& aClipRect) override;
  Maybe<gfx::IntRect> ClipRect() override;
  gfx::IntRect CurrentSurfaceDisplayRect() override;
  void SetSurfaceIsFlipped(bool aIsFlipped) override;
  bool SurfaceIsFlipped() override;

  void AttachExternalImage(wr::RenderTextureHost* aExternalImage) override;

  void SetBackingScale(float aBackingScale);

 protected:
  friend class NativeLayerRootWayland;

  NativeLayerWayland(const gfx::IntSize& aSize, bool aIsOpaque,
                     SurfacePoolHandleWayland* aSurfacePoolHandle);
  explicit NativeLayerWayland(bool aIsOpaque);
  ~NativeLayerWayland() override;

  Mutex mMutex;

  RefPtr<SurfacePoolHandleWayland> mSurfacePoolHandle;
  gfx::IntPoint mPosition;
  gfx::Matrix4x4 mTransform;
  gfx::IntRect mDisplayRect;
  gfx::IntRect mValidRect;
  gfx::IntRegion mDirtyRegion;
  gfx::IntSize mSize;
  Maybe<gfx::IntRect> mClipRect;
  gfx::SamplingFilter mSamplingFilter = gfx::SamplingFilter::POINT;
  float mBackingScale = 1.0f;
  bool mSurfaceIsFlipped = false;
  const bool mIsOpaque = false;
  bool mIsShown = false;

  RefPtr<NativeSurfaceWayland> mNativeSurface;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_NativeLayerWayland_h
