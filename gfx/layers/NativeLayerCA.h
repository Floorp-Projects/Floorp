/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_NativeLayerCA_h
#define mozilla_layers_NativeLayerCA_h

#include <IOSurface/IOSurface.h>

#include <deque>

#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"

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
namespace layers {

class IOSurfaceRegistry {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(IOSurfaceRegistry)

  virtual void RegisterSurface(CFTypeRefPtr<IOSurfaceRef> aSurface) = 0;
  virtual void UnregisterSurface(CFTypeRefPtr<IOSurfaceRef> aSurface) = 0;

 protected:
  virtual ~IOSurfaceRegistry() {}
};

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

  // Must be called within a current CATransaction on the transaction's thread.
  void ApplyChanges();

  void SetBackingScale(float aBackingScale);

  // Overridden methods
  already_AddRefed<NativeLayer> CreateLayer() override;
  void AppendLayer(NativeLayer* aLayer) override;
  void RemoveLayer(NativeLayer* aLayer) override;

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
//
// Creation and destruction of IOSurface objects is broadcast to an optional
// "surface registry" so that associated objects such as framebuffer objects
// don't need to be recreated on every frame: Instead, the surface registry can
// maintain one object per IOSurface in this layer's swap chain, and those
// objects will be reused in different frames as the layer cycles through the
// surfaces in its swap chain.
class NativeLayerCA : public NativeLayer {
 public:
  virtual NativeLayerCA* AsNativeLayerCA() override { return this; }

  // Overridden methods
  void SetRect(const gfx::IntRect& aRect) override;
  gfx::IntRect GetRect() override;

  // The invalid region of the surface that has been returned from the most
  // recent call to NextSurface. Newly-created surfaces are entirely invalid.
  // For surfaces that have been used before, the invalid region is the union of
  // all invalid regions that have been passed to
  // invalidateRegionThroughoutSwapchain since the last time that
  // NotifySurfaceReady was called for this surface. Can only be called between
  // calls to NextSurface and NotifySurfaceReady. Can be called on any thread.
  gfx::IntRegion CurrentSurfaceInvalidRegion();

  // Invalidates the specified region in all surfaces that are tracked by this
  // layer.
  void InvalidateRegionThroughoutSwapchain(const gfx::IntRegion& aRegion);

  // Returns an IOSurface that can be drawn to. The size of the IOSurface will
  // be the size of the rect that has been passed to SetRect.
  // The returned surface is guaranteed to be not in use by the window server.
  // After a call to NextSurface, NextSurface must not be called again until
  // after NotifySurfaceReady has been called. Can be called on any thread. When
  // used from multiple threads, callers need to make sure that they still only
  // call NextSurface and NotifySurfaceReady alternatingly and not in any other
  // order.
  CFTypeRefPtr<IOSurfaceRef> NextSurface();

  // Indicates that the surface which has been returned from the most recent
  // call to NextSurface is now finished being drawn to and can be displayed on
  // the screen. The surface will be used during the next call to the layer's
  // ApplyChanges method. Resets the invalid region on the surface to the empty
  // region.
  void NotifySurfaceReady();

  // Consumers may provide an object that implements the IOSurfaceRegistry
  // interface.
  // The registry's methods, Register/UnregisterSurface, will be called
  // synchronously during calls to NextSurface(), SetSurfaceRegistry(), and the
  // NativeLayer destructor, on the thread that those things happen to run on.
  // If this layer already owns surfaces when SetSurfaceRegistry gets called
  // with a non-null surface registry, those surfaces will immediately
  // (synchronously) be registered with that registry. If the current surface
  // registry is unset (via a call to SetSurfaceRegistry with a different value,
  // such as null), and the NativeLayer still owns surfaces, then those surfaces
  // will immediately be unregistered.
  // Since NativeLayer objects are reference counted and can be used from
  // different threads, it is recommended to call SetSurfaceRegistry(nullptr)
  // before destroying the NativeLayer so that the UnregisterSurface calls
  // happen at a deterministic time and on the right thread.
  void SetSurfaceRegistry(RefPtr<IOSurfaceRegistry> aSurfaceRegistry);
  RefPtr<IOSurfaceRegistry> GetSurfaceRegistry();

  // Whether the surface contents are flipped vertically compared to this
  // layer's coordinate system. Can be set on any thread at any time.
  void SetSurfaceIsFlipped(bool aIsFlipped);
  bool SurfaceIsFlipped();

  // Set an opaque region on the layer. Internally, this causes the creation
  // of opaque and transparent sublayers to cover the regions.
  // The coordinates in aRegion are relative to mPosition.
  void SetOpaqueRegion(const gfx::IntRegion& aRegion) override;
  gfx::IntRegion OpaqueRegion() override;

 protected:
  friend class NativeLayerRootCA;

  NativeLayerCA();
  ~NativeLayerCA() override;

  // To be called by NativeLayerRootCA:
  CALayer* UnderlyingCALayer() { return mWrappingCALayer; }
  void ApplyChanges();
  void SetBackingScale(float aBackingScale);

  struct SurfaceWithInvalidRegion {
    CFTypeRefPtr<IOSurfaceRef> mSurface;
    gfx::IntRegion mInvalidRegion;
    gfx::IntSize mSize;
  };

  std::vector<SurfaceWithInvalidRegion> RemoveExcessUnusedSurfaces(
      const MutexAutoLock&);
  void PlaceContentLayers(const MutexAutoLock&, const gfx::IntRegion& aRegion,
                          bool aOpaque, std::deque<CALayer*>* aLayersToRecycle);

  // Controls access to all fields of this class.
  Mutex mMutex;

  RefPtr<IOSurfaceRegistry> mSurfaceRegistry;  // can be null

  // Each IOSurface is initially created inside NextSurface.
  // The surface stays alive until the recycling mechanism in NextSurface
  // determines it is no longer needed, for example because the layer size
  // changed or because the swap chain has grown too long, or until the layer
  // is destroyed.
  // During the surface's lifetime, it will continuously move through the fields
  // mInProgressSurface, mReadySurface, and back to front through the
  // mSurfaces queue:
  //
  //  mSurfaces.front()
  //  ------[NextSurface()]-----> mInProgressSurface
  //  --[NotifySurfaceReady()]--> mReadySurface
  //  ----[ApplyChanges()]------> mSurfaces.back()  --> .... -->
  //  mSurfaces.front()
  //
  // We mark an IOSurface as "in use" as long as it is either in
  // mInProgressSurface or in mReadySurface. When it is in the mSurfaces queue,
  // it is not marked as "in use" by us - but it can be "in use" by the window
  // server. Consequently, IOSurfaceIsInUse on a surface from mSurfaces reflects
  // whether the window server is still reading from the surface, and we can use
  // this indicator to decide when to recycle the surface.
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

  // The queue of surfaces which make up our "swap chain".
  // mSurfaces.front() is the next surface we'll attempt to use.
  // mSurfaces.back() is the one we submitted most recently.
  std::deque<SurfaceWithInvalidRegion> mSurfaces;

  gfx::IntPoint mPosition;
  gfx::IntSize mSize;
  gfx::IntRegion mOpaqueRegion;  // coordinates relative to mPosition

  // Lazily initialized by first call to ApplyChanges.
  CALayer* mWrappingCALayer = nullptr;    // strong
  std::deque<CALayer*> mContentCALayers;  // strong

  float mBackingScale = 1.0f;
  bool mSurfaceIsFlipped = false;
  bool mMutatedPosition = false;
  bool mMutatedGeometry = false;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_NativeLayerCA_h
