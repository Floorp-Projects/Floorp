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
#include "nsTArray.h"

namespace mozilla::layers {

typedef void (*CallbackFunc)(void* aData, uint32_t aTime);

class CallbackMultiplexHelper final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CallbackMultiplexHelper);

  explicit CallbackMultiplexHelper(CallbackFunc aCallbackFunc,
                                   void* aCallbackData);

  void Callback(uint32_t aTime);
  bool IsActive() { return mActive; }

 private:
  ~CallbackMultiplexHelper() = default;

  void RunCallback(uint32_t aTime);

  bool mActive = true;
  CallbackFunc mCallbackFunc = nullptr;
  void* mCallbackData = nullptr;
};

class NativeLayerRootWayland final : public NativeLayerRoot {
 public:
  static already_AddRefed<NativeLayerRootWayland> CreateForMozContainer(
      MozContainer* aContainer);

  virtual NativeLayerRootWayland* AsNativeLayerRootWayland() override {
    return this;
  }

  // Overridden methods
  already_AddRefed<NativeLayer> CreateLayer(
      const gfx::IntSize& aSize, bool aIsOpaque,
      SurfacePoolHandle* aSurfacePoolHandle) override;
  already_AddRefed<NativeLayer> CreateLayerForExternalTexture(
      bool aIsOpaque) override;

  void AppendLayer(NativeLayer* aLayer) override;
  void RemoveLayer(NativeLayer* aLayer) override;
  void SetLayers(const nsTArray<RefPtr<NativeLayer>>& aLayers) override;

  void PrepareForCommit() override { mFrameInProcess = true; };
  bool CommitToScreen() override;

  void UpdateLayersOnMainThread();
  void AfterFrameClockAfterPaint();
  void RequestFrameCallback(CallbackFunc aCallbackFunc, void* aCallbackData);

 private:
  explicit NativeLayerRootWayland(MozContainer* aContainer);
  ~NativeLayerRootWayland();

  bool CommitToScreen(const MutexAutoLock& aProofOfLock);

  Mutex mMutex MOZ_UNANNOTATED;

  MozContainer* mContainer = nullptr;
  wl_surface* mWlSurface = nullptr;
  RefPtr<widget::WaylandBufferSHM> mShmBuffer;

  nsTArray<RefPtr<NativeLayerWayland>> mSublayers;
  nsTArray<RefPtr<NativeLayerWayland>> mOldSublayers;
  nsTArray<RefPtr<NativeLayerWayland>> mSublayersOnMainThread;
  bool mNewLayers = false;

  bool mFrameInProcess = false;
  bool mCallbackRequested = false;

  gulong mGdkAfterPaintId = 0;
  RefPtr<CallbackMultiplexHelper> mCallbackMultiplexHelper;
};

class NativeLayerWayland final : public NativeLayer {
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

  void Commit();
  void Unmap();
  void EnsureParentSurface(wl_surface* aParentSurface);
  const RefPtr<SurfacePoolHandleWayland> GetSurfacePoolHandle() {
    return mSurfacePoolHandle;
  };
  void SetBufferTransformFlipped(bool aFlippedX, bool aFlippedY);
  void SetSubsurfacePosition(int aX, int aY);
  void SetViewportSourceRect(const gfx::Rect aSourceRect);
  void SetViewportDestinationSize(int aWidth, int aHeight);

  void RequestFrameCallback(
      const RefPtr<CallbackMultiplexHelper>& aMultiplexHelper);
  static void FrameCallbackHandler(void* aData, wl_callback* aCallback,
                                   uint32_t aTime);

 private:
  friend class NativeLayerRootWayland;

  NativeLayerWayland(const gfx::IntSize& aSize, bool aIsOpaque,
                     SurfacePoolHandleWayland* aSurfacePoolHandle);
  explicit NativeLayerWayland(bool aIsOpaque);
  ~NativeLayerWayland() override;

  void HandlePartialUpdate(const MutexAutoLock& aProofOfLock);
  void FrameCallbackHandler(wl_callback* aCallback, uint32_t aTime);

  Mutex mMutex MOZ_UNANNOTATED;

  const RefPtr<SurfacePoolHandleWayland> mSurfacePoolHandle;
  const gfx::IntSize mSize;
  const bool mIsOpaque = false;
  gfx::IntPoint mPosition;
  gfx::Matrix4x4 mTransform;
  gfx::IntRect mDisplayRect;
  gfx::IntRegion mDirtyRegion;
  Maybe<gfx::IntRect> mClipRect;
  gfx::SamplingFilter mSamplingFilter = gfx::SamplingFilter::POINT;
  bool mSurfaceIsFlipped = false;
  bool mHasBufferAttached = false;

  wl_surface* mWlSurface = nullptr;
  wl_surface* mParentWlSurface = nullptr;
  wl_subsurface* mWlSubsurface = nullptr;
  wl_callback* mCallback = nullptr;
  wp_viewport* mViewport = nullptr;
  bool mBufferTransformFlippedX = false;
  bool mBufferTransformFlippedY = false;
  gfx::IntPoint mSubsurfacePosition = gfx::IntPoint(0, 0);
  gfx::Rect mViewportSourceRect = gfx::Rect(-1, -1, -1, -1);
  gfx::IntSize mViewportDestinationSize = gfx::IntSize(-1, -1);
  nsTArray<RefPtr<CallbackMultiplexHelper>> mCallbackMultiplexHelpers;

  RefPtr<widget::WaylandBuffer> mInProgressBuffer;
  RefPtr<widget::WaylandBuffer> mFrontBuffer;
};

}  // namespace mozilla::layers

#endif  // mozilla_layers_NativeLayerWayland_h
