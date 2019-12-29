/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_NativeLayerCA_h
#define mozilla_layers_NativeLayerCA_h

#include <IOSurface/IOSurface.h>

#include <deque>
#include <unordered_map>

#include "mozilla/Mutex.h"

#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/layers/NativeLayer.h"
#include "CFTypeRefPtr.h"
#include "nsRegion.h"
#include "nsISupportsImpl.h"

#ifdef __OBJC__
@class CALayer;
#else
typedef void CALayer;
#endif

namespace mozilla {

namespace gl {
class GLContextCGL;
class MozFramebuffer;
}  // namespace gl

namespace layers {

class SurfacePoolHandleCA;

// NativeLayerRootCA is the CoreAnimation implementation of the NativeLayerRoot
// interface. A NativeLayerRootCA is created by the widget around an existing
// CALayer with a call to CreateForCALayer.
// All methods can be called from any thread, there is internal locking.
// All effects from mutating methods are buffered locally and don't modify the
// underlying CoreAnimation layers until ApplyChanges() is called. This ensures
// that the modifications can be limited to run within a CoreAnimation
// transaction, and on a thread of the caller's choosing.
class NativeLayerRootCA : public NativeLayerRoot {
 public:
  static already_AddRefed<NativeLayerRootCA> CreateForCALayer(CALayer* aLayer);

  // Overridden methods
  already_AddRefed<NativeLayer> CreateLayer(
      const gfx::IntSize& aSize, bool aIsOpaque,
      SurfacePoolHandle* aSurfacePoolHandle) override;
  void AppendLayer(NativeLayer* aLayer) override;
  void RemoveLayer(NativeLayer* aLayer) override;
  void SetLayers(const nsTArray<RefPtr<NativeLayer>>& aLayers) override;

  void SetBackingScale(float aBackingScale);
  float BackingScale();

  // Must be called within a current CATransaction on the transaction's thread.
  void ApplyChanges();

 protected:
  explicit NativeLayerRootCA(CALayer* aLayer);
  ~NativeLayerRootCA() override;

  Mutex mMutex;                                // protects all other fields
  nsTArray<RefPtr<NativeLayerCA>> mSublayers;  // in z-order
  CALayer* mRootCALayer = nullptr;             // strong
  float mBackingScale = 1.0f;
  bool mMutated = false;
};

// NativeLayerCA wraps a CALayer and lets you draw to it. It ensures that only
// fully-drawn frames make their way to the screen, by maintaining a swap chain
// of IOSurfaces.
// All calls to mutating methods are buffered, and don't take effect on the
// underlying CoreAnimation layers until ApplyChanges() is called.
// The two most important methods are NextSurface and NotifySurfaceReady:
// NextSurface takes an available surface from the swap chain or creates a new
// surface if necessary. This surface can then be drawn to. Once drawing is
// finished, NotifySurfaceReady marks the surface as ready. This surface is
// committed to the layer during the next call to ApplyChanges().
// The swap chain keeps track of invalid areas within the surfaces.
class NativeLayerCA : public NativeLayer {
 public:
  virtual NativeLayerCA* AsNativeLayerCA() override { return this; }

  // Overridden methods
  gfx::IntSize GetSize() override;
  void SetPosition(const gfx::IntPoint& aPosition) override;
  gfx::IntPoint GetPosition() override;
  gfx::IntRect GetRect() override;
  RefPtr<gfx::DrawTarget> NextSurfaceAsDrawTarget(
      const gfx::IntRegion& aUpdateRegion,
      gfx::BackendType aBackendType) override;
  Maybe<GLuint> NextSurfaceAsFramebuffer(const gfx::IntRegion& aUpdateRegion,
                                         bool aNeedsDepth) override;
  void NotifySurfaceReady() override;
  void DiscardBackbuffers() override;
  bool IsOpaque() override;
  void SetClipRect(const Maybe<gfx::IntRect>& aClipRect) override;
  Maybe<gfx::IntRect> ClipRect() override;
  void SetSurfaceIsFlipped(bool aIsFlipped) override;
  bool SurfaceIsFlipped() override;

 protected:
  friend class NativeLayerRootCA;

  NativeLayerCA(const gfx::IntSize& aSize, bool aIsOpaque,
                SurfacePoolHandleCA* aSurfacePoolHandle);
  ~NativeLayerCA() override;

  // Gets the next surface for drawing from our swap chain and stores it in
  // mInProgressSurface. Returns whether this was successful.
  // mInProgressSurface is guaranteed to be not in use by the window server.
  // After a call to NextSurface, NextSurface must not be called again until
  // after NotifySurfaceReady has been called. Can be called on any thread. When
  // used from multiple threads, callers need to make sure that they still only
  // call NextSurface and NotifySurfaceReady alternatingly and not in any other
  // order.
  bool NextSurface(const MutexAutoLock&);

  // To be called by NativeLayerRootCA:
  CALayer* UnderlyingCALayer() { return mWrappingCALayer; }
  void ApplyChanges();
  void SetBackingScale(float aBackingScale);

  // Invalidates the specified region in all surfaces that are tracked by this
  // layer.
  void InvalidateRegionThroughoutSwapchain(const MutexAutoLock&,
                                           const gfx::IntRegion& aRegion);

  GLuint GetOrCreateFramebufferForSurface(const MutexAutoLock&,
                                          CFTypeRefPtr<IOSurfaceRef> aSurface,
                                          bool aNeedsDepth);

  // Invalidate aUpdateRegion and make sure that mInProgressSurface has valid
  // content everywhere outside aUpdateRegion, so that only aUpdateRegion needs
  // to be drawn. If content needs to be copied from a previous surface, aCopyFn
  // is called to do the copying.
  // aCopyFn: Fn(CFTypeRefPtr<IOSurfaceRef> aValidSourceIOSurface,
  //             const gfx::IntRegion& aCopyRegion) -> void
  template <typename F>
  void HandlePartialUpdate(const MutexAutoLock&,
                           const gfx::IntRegion& aUpdateRegion, F&& aCopyFn);

  struct SurfaceWithInvalidRegion {
    CFTypeRefPtr<IOSurfaceRef> mSurface;
    gfx::IntRegion mInvalidRegion;
  };

  struct SurfaceWithInvalidRegionAndCheckCount {
    SurfaceWithInvalidRegion mEntry;
    uint32_t mCheckCount;  // The number of calls to IOSurfaceIsInUse
  };

  Maybe<SurfaceWithInvalidRegion> GetUnusedSurfaceAndCleanUp(
      const MutexAutoLock&);

  // Controls access to all fields of this class.
  Mutex mMutex;

  // Each IOSurface is initially created inside NextSurface.
  // The surface stays alive until the recycling mechanism in NextSurface
  // determines it is no longer needed (because the swap chain has grown too
  // long) or until DiscardBackbuffers() is called or the layer is destroyed.
  // During the surface's lifetime, it will continuously move through the fields
  // mInProgressSurface, mReadySurface, mFrontSurface, and back to front through
  // the mSurfaces queue:
  //
  //  mSurfaces.front()
  //  ------[NextSurface()]-----> mInProgressSurface
  //  --[NotifySurfaceReady()]--> mReadySurface
  //  ----[ApplyChanges()]------> mFrontSurface
  //  ----[ApplyChanges()]------> mSurfaces.back()  --> .... -->
  //  mSurfaces.front()
  //
  // We mark an IOSurface as "in use" as long as it is either in
  // mInProgressSurface or in mReadySurface. When it is in mFrontSurface or in
  // the mSurfaces queue, it is not marked as "in use" by us - but it can be "in
  // use" by the window server. Consequently, IOSurfaceIsInUse on a surface from
  // mSurfaces reflects whether the window server is still reading from the
  // surface, and we can use this indicator to decide when to recycle the
  // surface.
  //
  // Users of NativeLayerCA normally proceed in this order:
  //  1. Begin a frame by calling NextSurface to get the surface.
  //  2. Draw to the surface.
  //  3. Mark the surface as done by calling NotifySurfaceReady.
  //  4. Trigger a CoreAnimation transaction, and call ApplyChanges within the
  //  transaction.
  //
  // For two consecutive frames, this results in the following ordering of
  // calls:
  // I. NextSurface, NotifySurfaceReady, ApplyChanges, NextSurface,
  //    NotifySurfaceReady, ApplyChanges
  //
  // In this scenario, either mInProgressSurface or mReadySurface is always
  // Nothing().
  //
  // However, sometimes we see the following ordering instead:
  // II. NextSurface, NotifySurfaceReady, NextSurface, ApplyChanges,
  //     NotifySurfaceReady, ApplyChanges
  //
  // This has the NextSurface and ApplyChanges calls in the middle reversed.
  //
  // In that scenario, both mInProgressSurface and mReadySurface will be Some()
  // between the calls to NextSurface and ApplyChanges in the middle, and the
  // two ApplyChanges invocations will submit two different surfaces. It is
  // important that we don't simply discard the first surface during the second
  // call to NextSurface in this scenario.

  // The surface we returned from the most recent call to NextSurface, before
  // the matching call to NotifySurfaceReady.
  // Will only be Some() between calls to NextSurface and NotifySurfaceReady.
  Maybe<SurfaceWithInvalidRegion> mInProgressSurface;

  // The surface that the most recent call to NotifySurfaceReady was for.
  // Will only be Some() between calls to NotifySurfaceReady and the next call
  // to ApplyChanges.
  // Both mInProgressSurface and mReadySurface can be Some() at the same time.
  Maybe<SurfaceWithInvalidRegion> mReadySurface;

  // The surface that the most recent call to ApplyChanges set on the CALayer.
  // Will be Some() after the first sequence of NextSurface, NotifySurfaceReady,
  // ApplyChanges calls, for the rest of the layer's life time.
  Maybe<SurfaceWithInvalidRegion> mFrontSurface;

  // The queue of surfaces which make up the rest of our "swap chain".
  // mSurfaces.front() is the next surface we'll attempt to use.
  // mSurfaces.back() is the one that was used most recently.
  std::vector<SurfaceWithInvalidRegionAndCheckCount> mSurfaces;

  // Non-null between calls to NextSurfaceAsDrawTarget and NotifySurfaceReady.
  RefPtr<MacIOSurface> mInProgressLockedIOSurface;

  RefPtr<SurfacePoolHandleCA> mSurfacePoolHandle;

  gfx::IntPoint mPosition;
  const gfx::IntSize mSize;
  Maybe<gfx::IntRect> mClipRect;

  // Lazily initialized by first call to ApplyChanges. mWrappingLayer is the
  // layer that applies mClipRect (if set), and mContentCALayer is the layer
  // that hosts the IOSurface. We do not share clip layers between consecutive
  // NativeLayerCA objects with the same clip rect.
  CALayer* mWrappingCALayer = nullptr;      // strong
  CALayer* mContentCALayer = nullptr;       // strong
  CALayer* mOpaquenessTintLayer = nullptr;  // strong

  float mBackingScale = 1.0f;
  bool mSurfaceIsFlipped = false;
  const bool mIsOpaque = false;
  bool mMutatedBackingScale = false;
  bool mMutatedSurfaceIsFlipped = false;
  bool mMutatedPosition = false;
  bool mMutatedClipRect = false;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_NativeLayerCA_h
