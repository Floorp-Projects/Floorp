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
class BasicLayerManager;
class ClientLayerManager;
class HostLayerManager;
class Layer;
class LayerMetricsWrapper;
class PaintedLayer;
class ContainerLayer;
class ImageLayer;
class ColorLayer;
class CompositorBridgeChild;
class CanvasLayer;
class ReadbackLayer;
class ReadbackProcessor;
class RefLayer;
class HostLayer;
class FocusTarget;
class KnowsCompositor;
class ShadowableLayer;
class ShadowLayerForwarder;
class LayerManagerComposite;
class TransactionIdAllocator;
class FrameUniformityData;
class PersistentBufferProvider;
class GlyphArray;
class WebRenderLayerManager;
struct AnimData;

namespace layerscope {
class LayersPacket;
}  // namespace layerscope

// Defined in LayerUserData.h; please include that file instead.
class LayerUserData;

class DidCompositeObserver {
 public:
  virtual void DidComposite() = 0;
};

class FrameRecorder {
 public:
  /**
   * Record (and return) frame-intervals and paint-times for frames which were
   * presented between calling StartFrameTimeRecording and
   * StopFrameTimeRecording.
   *
   * - Uses a cyclic buffer and serves concurrent consumers, so if Stop is
   *   called too late
   *     (elements were overwritten since Start), result is considered invalid
   *      and hence empty.)
   * - Buffer is capable of holding 10 seconds @ 60fps (or more if frames were
   *   less frequent).
   *     Can be changed (up to 1 hour) via pref:
   *     toolkit.framesRecording.bufferSize.
   * - Note: the first frame-interval may be longer than expected because last
   *   frame
   *     might have been presented some time before calling
   *     StartFrameTimeRecording.
   */

  /**
   * Returns a handle which represents current recording start position.
   */
  virtual uint32_t StartFrameTimeRecording(int32_t aBufferSize);

  /**
   *  Clears, then populates aFrameIntervals with the recorded frame timing
   *  data. The array will be empty if data was overwritten since
   *  aStartIndex was obtained.
   */
  virtual void StopFrameTimeRecording(uint32_t aStartIndex,
                                      nsTArray<float>& aFrameIntervals);

  void RecordFrame();

 private:
  struct FramesTimingRecording {
    // Stores state and data for frame intervals and paint times recording.
    // see LayerManager::StartFrameTimeRecording() at Layers.cpp for more
    // details.
    FramesTimingRecording()
        : mNextIndex(0),
          mLatestStartIndex(0),
          mCurrentRunStartIndex(0),
          mIsPaused(true) {}
    nsTArray<float> mIntervals;
    TimeStamp mLastFrameTime;
    uint32_t mNextIndex;
    uint32_t mLatestStartIndex;
    uint32_t mCurrentRunStartIndex;
    bool mIsPaused;
  };
  FramesTimingRecording mRecording;
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
class LayerManager : public FrameRecorder {
  NS_INLINE_DECL_REFCOUNTING(LayerManager)

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
  virtual void Destroy();
  bool IsDestroyed() { return mDestroyed; }

  virtual ShadowLayerForwarder* AsShadowForwarder() { return nullptr; }

  virtual KnowsCompositor* AsKnowsCompositor() { return nullptr; }

  virtual LayerManagerComposite* AsLayerManagerComposite() { return nullptr; }

  virtual ClientLayerManager* AsClientLayerManager() { return nullptr; }

  virtual BasicLayerManager* AsBasicLayerManager() { return nullptr; }
  virtual HostLayerManager* AsHostLayerManager() { return nullptr; }

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
   * This transaction will update the state of the window from which
   * this LayerManager was obtained.
   */
  virtual bool BeginTransaction(const nsCString& aURL = nsCString()) = 0;
  /**
   * Start a new transaction. Nested transactions are not allowed so
   * there must be no transaction currently in progress.
   * This transaction will render the contents of the layer tree to
   * the given target context. The rendering will be complete when
   * EndTransaction returns.
   */
  virtual bool BeginTransactionWithTarget(
      gfxContext* aTarget, const nsCString& aURL = nsCString()) = 0;

  enum EndTransactionFlags {
    END_DEFAULT = 0,
    END_NO_IMMEDIATE_REDRAW = 1 << 0,  // Do not perform the drawing phase
    END_NO_COMPOSITE =
        1 << 1,  // Do not composite after drawing painted layer contents.
    END_NO_REMOTE_COMPOSITE = 1 << 2  // Do not schedule a composition with a
                                      // remote Compositor, if one exists.
  };

  FrameLayerBuilder* GetLayerBuilder() {
    return reinterpret_cast<FrameLayerBuilder*>(
        GetUserData(&gLayerManagerLayerBuilder));
  }

  /**
   * Attempts to end an "empty transaction". There must have been no
   * changes to the layer tree since the BeginTransaction().
   * It's possible for this to fail; PaintedLayers may need to be updated
   * due to VRAM data being lost, for example. In such cases this method
   * returns false, and the caller must proceed with a normal layer tree
   * update and EndTransaction.
   */
  virtual bool EndEmptyTransaction(
      EndTransactionFlags aFlags = END_DEFAULT) = 0;

  /**
   * Function called to draw the contents of each PaintedLayer.
   * aRegionToDraw contains the region that needs to be drawn.
   * This would normally be a subregion of the visible region.
   * The callee must draw all of aRegionToDraw. Drawing outside
   * aRegionToDraw will be clipped out or ignored.
   * The callee must draw all of aRegionToDraw.
   * This region is relative to 0,0 in the PaintedLayer.
   *
   * aDirtyRegion should contain the total region that is be due to be painted
   * during the transaction, even though only aRegionToDraw should be drawn
   * during this call. aRegionToDraw must be entirely contained within
   * aDirtyRegion. If the total dirty region is unknown it is okay to pass a
   * subregion of the total dirty region, e.g. just aRegionToDraw, though it
   * may not be as efficient.
   *
   * aRegionToInvalidate contains a region whose contents have been
   * changed by the layer manager and which must therefore be invalidated.
   * For example, this could be non-empty if a retained layer internally
   * switches from RGBA to RGB or back ... we might want to repaint it to
   * consistently use subpixel-AA or not.
   * This region is relative to 0,0 in the PaintedLayer.
   * aRegionToInvalidate may contain areas that are outside
   * aRegionToDraw; the callee must ensure that these areas are repainted
   * in the current layer manager transaction or in a later layer
   * manager transaction.
   *
   * aContext must not be used after the call has returned.
   * We guarantee that buffered contents in the visible
   * region are valid once drawing is complete.
   *
   * The origin of aContext is 0,0 in the PaintedLayer.
   */
  typedef void (*DrawPaintedLayerCallback)(
      PaintedLayer* aLayer, gfxContext* aContext,
      const nsIntRegion& aRegionToDraw, const nsIntRegion& aDirtyRegion,
      DrawRegionClip aClip, const nsIntRegion& aRegionToInvalidate,
      void* aCallbackData);

  /**
   * Finish the construction phase of the transaction, perform the
   * drawing phase, and end the transaction.
   * During the drawing phase, all PaintedLayers in the tree are
   * drawn in tree order, exactly once each, except for those layers
   * where it is known that the visible region is empty.
   */
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) = 0;

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
   * Returns true if the underlying platform can properly support layers with
   * SurfaceMode::SURFACE_COMPONENT_ALPHA.
   */
  static bool LayersComponentAlphaEnabled();

  /**
   * Returns true if this LayerManager can properly support layers with
   * SurfaceMode::SURFACE_COMPONENT_ALPHA. LayerManagers that can't will use
   * transparent surfaces (and lose subpixel-AA for text).
   */
  virtual bool AreComponentAlphaLayersEnabled();

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
  Layer* GetRoot() { return mRoot; }

  /**
   * Does a breadth-first search from the root layer to find the first
   * scrollable layer, and returns its ViewID. Note that there may be
   * other layers in the tree which share the same ViewID.
   * Can be called any time.
   */
  ScrollableLayerGuid::ViewID GetRootScrollableLayerId();

  /**
   * Returns a LayerMetricsWrapper containing the Root
   * Content Documents layer.
   */
  LayerMetricsWrapper GetRootContentLayer();

  /**
   * CONSTRUCTION PHASE ONLY
   * Called when a managee has mutated.
   * Subclasses overriding this method must first call their
   * superclass's impl
   */
  virtual void Mutated(Layer* aLayer) {}
  virtual void MutatedSimple(Layer* aLayer) {}

  /**
   * Hints that can be used during PaintedLayer creation to influence the type
   * or properties of the layer created.
   *
   * NONE: No hint.
   * SCROLLABLE: This layer may represent scrollable content.
   */
  enum PaintedLayerCreationHint { NONE, SCROLLABLE };

  /**
   * CONSTRUCTION PHASE ONLY
   * Create a PaintedLayer for this manager's layer tree.
   */
  virtual already_AddRefed<PaintedLayer> CreatePaintedLayer() = 0;
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a PaintedLayer for this manager's layer tree, with a creation hint
   * parameter to help optimise the type of layer created.
   */
  virtual already_AddRefed<PaintedLayer> CreatePaintedLayerWithHint(
      PaintedLayerCreationHint) {
    return CreatePaintedLayer();
  }
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a ContainerLayer for this manager's layer tree.
   */
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() = 0;
  /**
   * CONSTRUCTION PHASE ONLY
   * Create an ImageLayer for this manager's layer tree.
   */
  virtual already_AddRefed<ImageLayer> CreateImageLayer() = 0;
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a ColorLayer for this manager's layer tree.
   */
  virtual already_AddRefed<ColorLayer> CreateColorLayer() = 0;
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a CanvasLayer for this manager's layer tree.
   */
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() = 0;
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a ReadbackLayer for this manager's layer tree.
   */
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer() {
    return nullptr;
  }
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a RefLayer for this manager's layer tree.
   */
  virtual already_AddRefed<RefLayer> CreateRefLayer() { return nullptr; }
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
   * Type of layer manager his is. This is to be used sparsely in order to
   * avoid a lot of Layers backend specific code. It should be used only when
   * Layers backend specific functionality is necessary.
   */
  virtual LayersBackend GetBackendType() = 0;

  /**
   * Type of layers backend that will be used to composite this layer tree.
   * When compositing is done remotely, then this returns the layers type
   * of the compositor.
   */
  virtual LayersBackend GetCompositorBackendType() { return GetBackendType(); }

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
   * Creates a PersistentBufferProvider for use with canvas which is optimized
   * for inter-operating with this layermanager.
   */
  virtual already_AddRefed<PersistentBufferProvider>
  CreatePersistentBufferProvider(const mozilla::gfx::IntSize& aSize,
                                 mozilla::gfx::SurfaceFormat aFormat);

  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize& aSize) {
    return true;
  }

  /**
   * returns the maximum texture size on this layer backend, or INT32_MAX
   * if there is no maximum
   */
  virtual int32_t GetMaxTextureSize() const = 0;

  /**
   * Return the name of the layer manager's backend.
   */
  virtual void GetBackendName(nsAString& aName) = 0;

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

  /**
   * Make sure that the previous transaction has been entirely
   * completed.
   *
   * Note: This may sychronously wait on a remote compositor
   * to complete rendering.
   */
  virtual void FlushRendering() {}

  /**
   * Make sure that the previous transaction has been
   * received. This will synchronsly wait on a remote compositor. */
  virtual void WaitOnTransactionProcessed() {}

  virtual void SendInvalidRegion(const nsIntRegion& aRegion) {}

  /**
   * Checks if we need to invalidate the OS widget to trigger
   * painting when updating this layer manager.
   */
  virtual bool NeedsWidgetInvalidation() { return true; }

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
   * Dump information about this layer manager and its managed tree to
   * layerscope packet.
   */
  void Dump(layerscope::LayersPacket* aPacket);

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

  bool IsCompositingCheap(LayersBackend aBackend) {
    // LayersBackend::LAYERS_NONE is an error state, but in that case we should
    // try to avoid loading the compositor!
    return LayersBackend::LAYERS_BASIC != aBackend &&
           LayersBackend::LAYERS_NONE != aBackend;
  }

  virtual bool IsCompositingCheap() { return true; }

  bool IsInTransaction() const { return mInTransaction; }
  virtual void GetFrameUniformity(FrameUniformityData* aOutData) {}

  virtual void SetRegionToClear(const nsIntRegion& aRegion) {
    mRegionToClear = aRegion;
  }

  virtual float RequestProperty(const nsAString& property) { return -1; }

  const TimeStamp& GetAnimationReadyTime() const { return mAnimationReadyTime; }

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

  virtual CompositorBridgeChild* GetCompositorBridgeChild() { return nullptr; }

  void RegisterPayload(const CompositionPayload& aPayload) {
    mPayload.AppendElement(aPayload);
    MOZ_ASSERT(mPayload.Length() < 10000);
  }

  void RegisterPayloads(const nsTArray<CompositionPayload>& aPayload) {
    mPayload.AppendElements(aPayload);
    MOZ_ASSERT(mPayload.Length() < 10000);
  }

  virtual void PayloadPresented(const TimeStamp& aTimeStamp);

  void SetContainsSVG(bool aContainsSVG) { mContainsSVG = aContainsSVG; }

  void AddPartialPrerenderedAnimation(uint64_t aCompositorAnimationId,
                                      dom::Animation* aAnimation);
  void RemovePartialPrerenderedAnimation(uint64_t aCompositorAnimationId,
                                         dom::Animation* aAnimation);
  void UpdatePartialPrerenderedAnimations(
      const nsTArray<uint64_t>& aJankedAnimations);

 protected:
  RefPtr<Layer> mRoot;
  gfx::UserData mUserData;
  bool mDestroyed;
  bool mSnapEffectiveTransforms;

  nsIntRegion mRegionToClear;

  // Protected destructor, to discourage deletion outside of Release():
  virtual ~LayerManager();

  // Print interesting information about this into aStreamo.  Internally
  // used to implement Dump*() and Log*().
  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  // Print interesting information about this into layerscope packet.
  // Internally used to implement Dump().
  virtual void DumpPacket(layerscope::LayersPacket* aPacket);

  uint64_t mId;
  bool mInTransaction;

  // Used for tracking CONTENT_FRAME_TIME_WITH_SVG
  bool mContainsSVG;
  // The time when painting most recently finished. This is recorded so that
  // we can time any play-pending animations from this point.
  TimeStamp mAnimationReadyTime;
  // The count of pixels that were painted in the current transaction.
  uint32_t mPaintedPixelCount;
  // The payload associated with currently pending painting work, for
  // client layer managers that typically means payload that is part of the
  // 'upcoming transaction', for HostLayerManagers this typically means
  // what has been included in received transactions to be presented on the
  // next composite.
  // IMPORTANT: Clients should take care to clear this or risk it slowly
  // growing out of control.
  nsTArray<CompositionPayload> mPayload;
  // Transform animations which are not fully pre-rendered because it's on a
  // large frame.  We need to update the pre-rendered area once after we tried
  // to composite area which is outside of the pre-rendered area on the
  // compositor.
  nsRefPtrHashtable<nsUint64HashKey, dom::Animation>
      mPartialPrerenderedAnimations;

 public:
  /*
   * Methods to store/get/clear a "pending scroll info update" object on a
   * per-scrollid basis. This is used for empty transactions that push over
   * scroll position updates to the APZ code.
   */
  virtual bool AddPendingScrollUpdateForNextTransaction(
      ScrollableLayerGuid::ViewID aScrollId,
      const ScrollPositionUpdate& aUpdateInfo);
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
