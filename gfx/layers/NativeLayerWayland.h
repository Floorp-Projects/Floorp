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
#include "mozilla/widget/WaylandShmBuffer.h"
#include "nsRegion.h"
#include "nsTArray.h"

namespace mozilla::layers {

using gfx::BackendType;
using gfx::DrawTarget;
using gfx::IntPoint;
using gfx::IntRect;
using gfx::IntRegion;
using gfx::IntSize;
using gfx::Matrix4x4;
using gfx::SamplingFilter;

class NativeLayerRootWayland : public NativeLayerRoot {
 public:
  static already_AddRefed<NativeLayerRootWayland> CreateForMozContainer(
      MozContainer* aContainer);

  virtual NativeLayerRootWayland* AsNativeLayerRootWayland() override {
    return this;
  }

  // Overridden methods
  already_AddRefed<NativeLayer> CreateLayer(
      const IntSize& aSize, bool aIsOpaque,
      SurfacePoolHandle* aSurfacePoolHandle) override;
  void AppendLayer(NativeLayer* aLayer) override;
  void RemoveLayer(NativeLayer* aLayer) override;
  void SetLayers(const nsTArray<RefPtr<NativeLayer>>& aLayers) override;
  void UpdateLayersOnMainThread();
  UniquePtr<NativeLayerRootSnapshotter> CreateSnapshotter() override;
  bool CommitToScreen() override;

  // When the compositor is paused the wl_surface of the MozContainer will
  // get destroyed. We thus have to recreate subsurface relationships for
  // all tiles after resume. This is a implementation specific quirk of
  // our GTK-Wayland backend.
  void PauseCompositor() override;
  bool ResumeCompositor() override;

  already_AddRefed<NativeLayer> CreateLayerForExternalTexture(
      bool aIsOpaque) override;

  void AfterFrameClockAfterPaint();
  void RequestFrameCallback(CallbackFunc aCallbackFunc, void* aCallbackData);

 protected:
  explicit NativeLayerRootWayland(MozContainer* aContainer);
  ~NativeLayerRootWayland() = default;

  void EnsureSurfaceInitialized();
  bool EnsureShowLayer(const RefPtr<NativeLayerWayland>& aLayer);
  void EnsureHideLayer(const RefPtr<NativeLayerWayland>& aLayer);
  void UnmapLayer(const RefPtr<NativeLayerWayland>& aLayer);

  Mutex mMutex;

  nsTArray<RefPtr<NativeLayerWayland>> mSublayers;
  nsTArray<RefPtr<NativeLayerWayland>> mSublayersOnMainThread;
  MozContainer* mContainer = nullptr;
  RefPtr<widget::WaylandShmBuffer> mShmBuffer;
  bool mCompositorRunning = true;
  gulong mGdkAfterPaintId = 0;
  RefPtr<CallbackMultiplexHelper> mCallbackMultiplexHelper;
  bool mCommitRequested = false;
};

class NativeLayerWayland : public NativeLayer {
 public:
  virtual NativeLayerWayland* AsNativeLayerWayland() override { return this; }

  // Overridden methods
  IntSize GetSize() override;
  void SetPosition(const IntPoint& aPosition) override;
  IntPoint GetPosition() override;
  void SetTransform(const Matrix4x4& aTransform) override;
  Matrix4x4 GetTransform() override;
  IntRect GetRect() override;
  void SetSamplingFilter(SamplingFilter aSamplingFilter) override;
  RefPtr<DrawTarget> NextSurfaceAsDrawTarget(const IntRect& aDisplayRect,
                                             const IntRegion& aUpdateRegion,
                                             BackendType aBackendType) override;
  Maybe<GLuint> NextSurfaceAsFramebuffer(const IntRect& aDisplayRect,
                                         const IntRegion& aUpdateRegion,
                                         bool aNeedsDepth) override;
  void NotifySurfaceReady() override;
  void DiscardBackbuffers() override;
  bool IsOpaque() override;
  void SetClipRect(const Maybe<IntRect>& aClipRect) override;
  Maybe<IntRect> ClipRect() override;
  IntRect CurrentSurfaceDisplayRect() override;
  void SetSurfaceIsFlipped(bool aIsFlipped) override;
  bool SurfaceIsFlipped() override;

  void AttachExternalImage(wr::RenderTextureHost* aExternalImage) override;

 protected:
  friend class NativeLayerRootWayland;

  NativeLayerWayland(const IntSize& aSize, bool aIsOpaque,
                     SurfacePoolHandleWayland* aSurfacePoolHandle);
  explicit NativeLayerWayland(bool aIsOpaque);
  ~NativeLayerWayland() override;

  Mutex mMutex;

  RefPtr<SurfacePoolHandleWayland> mSurfacePoolHandle;
  IntPoint mPosition;
  Matrix4x4 mTransform;
  IntRect mDisplayRect;
  IntRect mValidRect;
  IntRegion mDirtyRegion;
  IntSize mSize;
  Maybe<IntRect> mClipRect;
  SamplingFilter mSamplingFilter = SamplingFilter::POINT;
  bool mSurfaceIsFlipped = false;
  const bool mIsOpaque = false;
  bool mIsShown = false;

  RefPtr<NativeSurfaceWayland> mNativeSurface;
};

}  // namespace mozilla::layers

#endif  // mozilla_layers_NativeLayerWayland_h
