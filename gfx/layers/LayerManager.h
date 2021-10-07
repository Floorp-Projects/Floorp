/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERMANAGER_H
#define GFX_LAYERMANAGER_H

#include <cstdint>         // for uint32_t, uint64_t, int32_t, uint8_t
#include <iosfwd>          // for stringstream
#include <new>             // for operator new
#include <unordered_set>   // for unordered_set
#include <utility>         // for forward
#include "FrameMetrics.h"  // for ScrollUpdatesMap
#include "ImageContainer.h"  // for ImageContainer, ImageContainer::Mode, ImageContainer::SYNCHRONOUS
#include "WindowRenderer.h"
#include "mozilla/AlreadyAddRefed.h"  // for already_AddRefed
#include "mozilla/Maybe.h"            // for Maybe
#include "mozilla/RefPtr.h"           // for RefPtr
#include "mozilla/TimeStamp.h"        // for TimeStamp
#include "mozilla/UniquePtr.h"        // for UniquePtr
#include "mozilla/dom/Animation.h"    // for Animation
#include "mozilla/gfx/Point.h"        // for IntSize
#include "mozilla/gfx/Types.h"        // for SurfaceFormat
#include "mozilla/gfx/UserData.h"     // for UserData, UserDataKey (ptr only)
#include "mozilla/layers/CompositorTypes.h"  // for TextureFactoryIdentifier
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid, ScrollableLayerGuid::ViewID
#include "nsHashKeys.h"                          // for nsUint64HashKey
#include "nsISupports.h"        // for NS_INLINE_DECL_REFCOUNTING
#include "nsIWidget.h"          // for nsIWidget
#include "nsRefPtrHashtable.h"  // for nsRefPtrHashtable
#include "nsRegion.h"           // for nsIntRegion
#include "nsStringFwd.h"        // for nsCString, nsAString
#include "nsTArray.h"           // for nsTArray

// XXX These includes could be avoided by moving function implementations to the
// cpp file
#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT, MOZ_A...
#include "mozilla/layers/LayersTypes.h"  // for CompositionPayload, LayersBackend, TransactionId, DrawRegionClip, LayersBackend:...

class gfxContext;

extern uint8_t gLayerManagerLayerBuilder;

namespace mozilla {

class FrameLayerBuilder;
class LogModule;
class ScrollPositionUpdate;

namespace gfx {
class DrawTarget;
}  // namespace gfx

namespace layers {

class AsyncPanZoomController;
class ClientLayerManager;
class Layer;
class CompositorBridgeChild;
class ReadbackProcessor;
class FocusTarget;
class KnowsCompositor;
class TransactionIdAllocator;
class FrameUniformityData;
class PersistentBufferProvider;
class GlyphArray;
class WebRenderLayerManager;
struct AnimData;

// Defined in LayerUserData.h; please include that file instead.
class LayerUserData;

class DidCompositeObserver {
 public:
  virtual void DidComposite() = 0;
};

/*
 * Motivation: For truly smooth animation and video playback, we need to
 * be able to compose frames and render them on a dedicated thread (i.e.
 * off the main thread where DOM manipulation, script execution and layout
 * induce difficult-to-bound latency). This requires Gecko to construct
 * some kind of persistent scene structure (graph or tree) that can be
 * safely transmitted across threads. We have other scenarios (e.g. mobile
 * browsing) where retaining some rendered data between paints is desired
 * for performance, so again we need a retained scene structure.
 *
 * Our retained scene structure is a layer tree. Each layer represents
 * content which can be composited onto a destination surface; the root
 * layer is usually composited into a window, and non-root layers are
 * composited into their parent layers. Layers have attributes (e.g.
 * opacity and clipping) that influence their compositing.
 *
 * We want to support a variety of layer implementations, including
 * a simple "immediate mode" implementation that doesn't retain any
 * rendered data between paints (i.e. uses cairo in just the way that
 * Gecko used it before layers were introduced). But we also don't want
 * to have bifurcated "layers"/"non-layers" rendering paths in Gecko.
 * Therefore the layers API is carefully designed to permit maximally
 * efficient implementation in an "immediate mode" style. See the
 * BasicLayerManager for such an implementation.
 */

/**
 * A LayerManager controls a tree of layers. All layers in the tree
 * must use the same LayerManager.
 *
 * All modifications to a layer tree must happen inside a transaction.
 * Only the state of the layer tree at the end of a transaction is
 * rendered. Transactions cannot be nested
 *
 * Each transaction has two phases:
 * 1) Construction: layers are created, inserted, removed and have
 * properties set on them in this phase.
 * BeginTransaction and BeginTransactionWithTarget start a transaction in
 * the Construction phase.
 * 2) Drawing: PaintedLayers are rendered into in this phase, in tree
 * order. When the client has finished drawing into the PaintedLayers, it should
 * call EndTransaction to complete the transaction.
 *
 * All layer API calls happen on the main thread.
 *
 * Layers are refcounted. The layer manager holds a reference to the
 * root layer, and each container layer holds a reference to its children.
 */
class LayerManager : public WindowRenderer {
 protected:
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::IntSize IntSize;
  typedef mozilla::gfx::SurfaceFormat SurfaceFormat;

 public:
  LayerManager();

  /**
   * Release layers and resources held by this layer manager, and mark
   * it as destroyed.  Should do any cleanup necessary in preparation
   * for its widget going away.  After this call, only user data calls
   * are valid on the layer manager.
   */
  void Destroy() override;
  bool IsDestroyed() { return mDestroyed; }

  virtual WebRenderLayerManager* AsWebRenderLayerManager() { return nullptr; }

  /**
   * Returns true if this LayerManager is owned by an nsIWidget,
   * and is used for drawing into the widget.
   */
  virtual bool IsWidgetLayerManager() { return true; }
  virtual bool IsInactiveLayerManager() { return false; }

  /**
   * Start a new transaction. Nested transactions are not allowed so
   * there must be no transaction currently in progress.
   * This transaction will render the contents of the layer tree to
   * the given target context. The rendering will be complete when
   * EndTransaction returns.
   */
  virtual bool BeginTransactionWithTarget(
      gfxContext* aTarget, const nsCString& aURL = nsCString()) = 0;

  FrameLayerBuilder* GetLayerBuilder() {
    return reinterpret_cast<FrameLayerBuilder*>(
        GetUserData(&gLayerManagerLayerBuilder));
  }

  /**
   * Schedule a composition with the remote Compositor, if one exists
   * for this LayerManager. Useful in conjunction with the
   * END_NO_REMOTE_COMPOSITE flag to EndTransaction.
   */
  virtual void ScheduleComposite() {}

  virtual void SetNeedsComposite(bool aNeedsComposite) {}
  virtual bool NeedsComposite() const { return false; }

  virtual bool HasShadowManagerInternal() const { return false; }
  bool HasShadowManager() const { return HasShadowManagerInternal(); }
  virtual void StorePluginWidgetConfigurations(
      const nsTArray<nsIWidget::Configuration>& aConfigurations) {}
  bool IsSnappingEffectiveTransforms() { return mSnapEffectiveTransforms; }

  /**
   * Returns true if this LayerManager always requires an intermediate surface
   * to render blend operations.
   */
  virtual bool BlendingRequiresIntermediateSurface() { return false; }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the root layer. The root layer is initially null. If there is
   * no root layer, EndTransaction won't draw anything.
   */
  virtual void SetRoot(Layer* aLayer) = 0;
  /**
   * Can be called anytime
   */
  Layer* GetRoot() { return nullptr; }

  /**
   * CONSTRUCTION PHASE ONLY
   * Called when a managee has mutated.
   * Subclasses overriding this method must first call their
   * superclass's impl
   */
  virtual void Mutated(Layer* aLayer) {}
  virtual void MutatedSimple(Layer* aLayer) {}

  /**
   * Can be called anytime, from any thread.
   *
   * Creates an Image container which forwards its images to the compositor
   * within layer transactions on the main thread or asynchronously using the
   * ImageBridge IPDL protocol. In the case of asynchronous, If the protocol is
   * not available, the returned ImageContainer will forward images within layer
   * transactions.
   */
  static already_AddRefed<ImageContainer> CreateImageContainer(
      ImageContainer::Mode flag = ImageContainer::SYNCHRONOUS);

  /**
   * Creates a DrawTarget which is optimized for inter-operating with this
   * layer manager.
   */
  virtual already_AddRefed<DrawTarget> CreateOptimalDrawTarget(
      const IntSize& aSize, SurfaceFormat imageFormat);

  /**
   * Creates a DrawTarget for alpha masks which is optimized for inter-
   * operating with this layer manager. In contrast to CreateOptimalDrawTarget,
   * this surface is optimised for drawing alpha only and we assume that
   * drawing the mask is fairly simple.
   */
  virtual already_AddRefed<DrawTarget> CreateOptimalMaskDrawTarget(
      const IntSize& aSize);

  /**
   * Creates a DrawTarget for use with canvas which is optimized for
   * inter-operating with this layermanager.
   */
  virtual already_AddRefed<mozilla::gfx::DrawTarget> CreateDrawTarget(
      const mozilla::gfx::IntSize& aSize, mozilla::gfx::SurfaceFormat aFormat);

  /**
   * This setter can be used anytime. The user data for all keys is
   * initially null. Ownership pases to the layer manager.
   */
  void SetUserData(void* aKey, LayerUserData* aData) {
    mUserData.Add(static_cast<gfx::UserDataKey*>(aKey), aData,
                  LayerUserDataDestroy);
  }
  /**
   * This can be used anytime. Ownership passes to the caller!
   */
  UniquePtr<LayerUserData> RemoveUserData(void* aKey);

  /**
   * This getter can be used anytime.
   */
  bool HasUserData(void* aKey) {
    return mUserData.Has(static_cast<gfx::UserDataKey*>(aKey));
  }
  /**
   * This getter can be used anytime. Ownership is retained by the layer
   * manager.
   */
  LayerUserData* GetUserData(void* aKey) const {
    return static_cast<LayerUserData*>(
        mUserData.Get(static_cast<gfx::UserDataKey*>(aKey)));
  }

  /**
   * Must be called outside of a layers transaction.
   *
   * For the subtree rooted at |aSubtree|, this attempts to free up
   * any free-able resources like retained buffers, but may do nothing
   * at all.  After this call, the layer tree is left in an undefined
   * state; the layers in |aSubtree|'s subtree may no longer have
   * buffers with valid content and may no longer be able to draw
   * their visible and valid regions.
   *
   * In general, a painting or forwarding transaction on |this| must
   * complete on the tree before it returns to a valid state.
   *
   * Resource freeing begins from |aSubtree| or |mRoot| if |aSubtree|
   * is null.  |aSubtree|'s manager must be this.
   */
  virtual void ClearCachedResources(Layer* aSubtree = nullptr) {}

  /**
   * Flag the next paint as the first for a document.
   */
  virtual void SetIsFirstPaint() {}
  virtual bool GetIsFirstPaint() const { return false; }

  /**
   * Set the current focus target to be sent with the next paint.
   */
  virtual void SetFocusTarget(const FocusTarget& aFocusTarget) {}

  virtual void SendInvalidRegion(const nsIntRegion& aRegion) {}

  virtual const char* Name() const { return "???"; }

  /**
   * Dump information about this layer manager and its managed tree to
   * aStream.
   */
  void Dump(std::stringstream& aStream, const char* aPrefix = "",
            bool aDumpHtml = false, bool aSorted = false);
  /**
   * Dump information about just this layer manager itself to aStream
   */
  void DumpSelf(std::stringstream& aStream, const char* aPrefix = "",
                bool aSorted = false);
  void Dump(bool aSorted = false);

  /**
   * Log information about this layer manager and its managed tree to
   * the NSPR log (if enabled for "Layers").
   */
  void Log(const char* aPrefix = "");
  /**
   * Log information about just this layer manager itself to the NSPR
   * log (if enabled for "Layers").
   */
  void LogSelf(const char* aPrefix = "");

  static bool IsLogEnabled();
  static mozilla::LogModule* GetLog();

  bool IsInTransaction() const { return mInTransaction; }

  virtual void SetRegionToClear(const nsIntRegion& aRegion) {
    mRegionToClear = aRegion;
  }

  virtual float RequestProperty(const nsAString& property) { return -1; }

  virtual bool AsyncPanZoomEnabled() const { return false; }

  static void LayerUserDataDestroy(void* data);

  void AddPaintedPixelCount(int32_t aCount) { mPaintedPixelCount += aCount; }

  uint32_t GetAndClearPaintedPixelCount() {
    uint32_t count = mPaintedPixelCount;
    mPaintedPixelCount = 0;
    return count;
  }

  virtual void SetLayersObserverEpoch(LayersObserverEpoch aEpoch) {}

  virtual void DidComposite(TransactionId aTransactionId,
                            const mozilla::TimeStamp& aCompositeStart,
                            const mozilla::TimeStamp& aCompositeEnd) {}

  virtual void AddDidCompositeObserver(DidCompositeObserver* aObserver) {
    MOZ_CRASH("GFX: LayerManager");
  }
  virtual void RemoveDidCompositeObserver(DidCompositeObserver* aObserver) {
    MOZ_CRASH("GFX: LayerManager");
  }

  virtual void UpdateTextureFactoryIdentifier(
      const TextureFactoryIdentifier& aNewIdentifier) {}

  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() {
    return TextureFactoryIdentifier();
  }

  virtual void SetTransactionIdAllocator(TransactionIdAllocator* aAllocator) {}

  virtual TransactionId GetLastTransactionId() { return TransactionId{0}; }

  void SetContainsSVG(bool aContainsSVG) { mContainsSVG = aContainsSVG; }

 protected:
  gfx::UserData mUserData;
  bool mDestroyed;
  bool mSnapEffectiveTransforms;

  nsIntRegion mRegionToClear;

  // Protected destructor, to discourage deletion outside of Release():
  virtual ~LayerManager();

  // Print interesting information about this into aStreamo.  Internally
  // used to implement Dump*() and Log*().
  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  uint64_t mId;
  bool mInTransaction;

  // Used for tracking CONTENT_FRAME_TIME_WITH_SVG
  bool mContainsSVG;
  // The count of pixels that were painted in the current transaction.
  uint32_t mPaintedPixelCount;

 public:
  /*
   * Methods to store/get/clear a "pending scroll info update" object on a
   * per-scrollid basis. This is used for empty transactions that push over
   * scroll position updates to the APZ code.
   */
  virtual bool AddPendingScrollUpdateForNextTransaction(
      ScrollableLayerGuid::ViewID aScrollId,
      const ScrollPositionUpdate& aUpdateInfo) override;
  Maybe<nsTArray<ScrollPositionUpdate>> GetPendingScrollInfoUpdate(
      ScrollableLayerGuid::ViewID aScrollId);
  std::unordered_set<ScrollableLayerGuid::ViewID>
  ClearPendingScrollInfoUpdate();

 protected:
  ScrollUpdatesMap mPendingScrollUpdates;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_LAYERS_H */
