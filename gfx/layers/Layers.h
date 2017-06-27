/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERS_H
#define GFX_LAYERS_H

#include <map>
#include <stdint.h>                     // for uint32_t, uint64_t, uint8_t
#include <stdio.h>                      // for FILE
#include <sys/types.h>                  // for int32_t
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for LayerMargin, LayerPoint, ParentLayerIntRect
#include "gfxContext.h"
#include "gfxTypes.h"
#include "gfxPoint.h"                   // for gfxPoint
#include "gfxRect.h"                    // for gfxRect
#include "gfx2DGlue.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2, etc
#include "mozilla/Array.h"
#include "mozilla/DebugOnly.h"          // for DebugOnly
#include "mozilla/EventForwards.h"      // for nsPaintEvent
#include "mozilla/Maybe.h"              // for Maybe
#include "mozilla/Poison.h"
#include "mozilla/RefPtr.h"             // for already_AddRefed
#include "mozilla/StyleAnimationValue.h" // for StyleAnimationValue, etc
#include "mozilla/TimeStamp.h"          // for TimeStamp, TimeDuration
#include "mozilla/UniquePtr.h"          // for UniquePtr
#include "mozilla/gfx/BaseMargin.h"     // for BaseMargin
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/TiledRegion.h"    // for TiledIntRegion
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "mozilla/gfx/UserData.h"       // for UserData, etc
#include "mozilla/layers/BSPTree.h"     // for LayerPolygon
#include "mozilla/layers/LayerAttributes.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsAutoPtr, nsRefPtr, etc
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsCSSPropertyID.h"              // for nsCSSPropertyID
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for Layer::Release, etc
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsString.h"                   // for nsCString
#include "nsTArray.h"                   // for nsTArray
#include "nsTArrayForwardDeclare.h"     // for InfallibleTArray
#include "nscore.h"                     // for nsACString, nsAString
#include "mozilla/Logging.h"                      // for PRLogModuleInfo
#include "nsIWidget.h"                  // For plugin window configuration information structs
#include "ImageContainer.h"

class gfxContext;
class nsDisplayListBuilder;
class nsDisplayItem;

extern uint8_t gLayerManagerLayerBuilder;

namespace mozilla {

class ComputedTimingFunction;
class FrameLayerBuilder;
class StyleAnimationValue;

namespace gl {
class GLContext;
} // namespace gl

namespace gfx {
class DrawTarget;
} // namespace gfx

namespace layers {

class Animation;
class AnimationData;
class AsyncCanvasRenderer;
class AsyncPanZoomController;
class BasicLayerManager;
class ClientLayerManager;
class HostLayerManager;
class Layer;
class LayerMetricsWrapper;
class PaintedLayer;
class ContainerLayer;
class ImageLayer;
class DisplayItemLayer;
class ColorLayer;
class CompositorAnimations;
class CompositorBridgeChild;
class TextLayer;
class CanvasLayer;
class BorderLayer;
class ReadbackLayer;
class ReadbackProcessor;
class RefLayer;
class HostLayer;
class KnowsCompositor;
class ShadowableLayer;
class ShadowLayerForwarder;
class LayerManagerComposite;
class SpecificLayerAttributes;
class TransactionIdAllocator;
class Compositor;
class FrameUniformityData;
class PersistentBufferProvider;
class GlyphArray;
class WebRenderLayerManager;
struct AnimData;

namespace layerscope {
class LayersPacket;
} // namespace layerscope

#define MOZ_LAYER_DECL_NAME(n, e)                              \
  virtual const char* Name() const override { return n; }  \
  virtual LayerType GetType() const override { return e; } \
  static LayerType Type() { return e; }

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
class LayerManager {
  NS_INLINE_DECL_REFCOUNTING(LayerManager)

protected:
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::IntSize IntSize;
  typedef mozilla::gfx::SurfaceFormat SurfaceFormat;

public:
  LayerManager()
    : mDestroyed(false)
    , mSnapEffectiveTransforms(true)
    , mId(0)
    , mInTransaction(false)
    , mPaintedPixelCount(0)
  {}

  /**
   * Release layers and resources held by this layer manager, and mark
   * it as destroyed.  Should do any cleanup necessary in preparation
   * for its widget going away.  After this call, only user data calls
   * are valid on the layer manager.
   */
  virtual void Destroy()
  {
    mDestroyed = true;
    mUserData.Destroy();
    mRoot = nullptr;
  }
  bool IsDestroyed() { return mDestroyed; }

  virtual ShadowLayerForwarder* AsShadowForwarder()
  { return nullptr; }

  virtual KnowsCompositor* AsKnowsCompositor()
  { return nullptr; }

  virtual LayerManagerComposite* AsLayerManagerComposite()
  { return nullptr; }

  virtual ClientLayerManager* AsClientLayerManager()
  { return nullptr; }

  virtual BasicLayerManager* AsBasicLayerManager()
  { return nullptr; }
  virtual HostLayerManager* AsHostLayerManager()
  { return nullptr; }

  virtual WebRenderLayerManager* AsWebRenderLayerManager()
  { return nullptr; }

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
  virtual bool BeginTransaction() = 0;
  /**
   * Start a new transaction. Nested transactions are not allowed so
   * there must be no transaction currently in progress.
   * This transaction will render the contents of the layer tree to
   * the given target context. The rendering will be complete when
   * EndTransaction returns.
   */
  virtual bool BeginTransactionWithTarget(gfxContext* aTarget) = 0;

  enum EndTransactionFlags {
    END_DEFAULT = 0,
    END_NO_IMMEDIATE_REDRAW = 1 << 0,  // Do not perform the drawing phase
    END_NO_COMPOSITE = 1 << 1, // Do not composite after drawing painted layer contents.
    END_NO_REMOTE_COMPOSITE = 1 << 2 // Do not schedule a composition with a remote Compositor, if one exists.
  };

  FrameLayerBuilder* GetLayerBuilder() {
    return reinterpret_cast<FrameLayerBuilder*>(GetUserData(&gLayerManagerLayerBuilder));
  }

  /**
   * Attempts to end an "empty transaction". There must have been no
   * changes to the layer tree since the BeginTransaction().
   * It's possible for this to fail; PaintedLayers may need to be updated
   * due to VRAM data being lost, for example. In such cases this method
   * returns false, and the caller must proceed with a normal layer tree
   * update and EndTransaction.
   */
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) = 0;

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
  typedef void (* DrawPaintedLayerCallback)(PaintedLayer* aLayer,
                                           gfxContext* aContext,
                                           const nsIntRegion& aRegionToDraw,
                                           const nsIntRegion& aDirtyRegion,
                                           DrawRegionClip aClip,
                                           const nsIntRegion& aRegionToInvalidate,
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
   * for this LayerManager. Useful in conjunction with the END_NO_REMOTE_COMPOSITE
   * flag to EndTransaction.
   */
  virtual void ScheduleComposite() {}

  virtual void SetNeedsComposite(bool aNeedsComposite) {}
  virtual bool NeedsComposite() const { return false; }

  virtual bool HasShadowManagerInternal() const { return false; }
  bool HasShadowManager() const { return HasShadowManagerInternal(); }
  virtual void StorePluginWidgetConfigurations(const nsTArray<nsIWidget::Configuration>& aConfigurations) {}
  bool IsSnappingEffectiveTransforms() { return mSnapEffectiveTransforms; }


  /**
   * Returns true if the layer manager can't render component alpha
   * layers, and layer building should do it's best to avoid
   * creating them.
   */
  virtual bool ShouldAvoidComponentAlphaLayers() { return false; }

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
   * Returns true if this LayerManager supports component alpha layers in
   * situations that require a copy of the backdrop.
   */
  virtual bool SupportsBackdropCopyForComponentAlpha() { return true; }

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
  FrameMetrics::ViewID GetRootScrollableLayerId();

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
  virtual void Mutated(Layer* aLayer) { }
  virtual void MutatedSimple(Layer* aLayer) { }

  /**
   * Hints that can be used during PaintedLayer creation to influence the type
   * or properties of the layer created.
   *
   * NONE: No hint.
   * SCROLLABLE: This layer may represent scrollable content.
   */
  enum PaintedLayerCreationHint {
    NONE, SCROLLABLE
  };

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
  virtual already_AddRefed<PaintedLayer> CreatePaintedLayerWithHint(PaintedLayerCreationHint) {
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
   * Create a TextLayer for this manager's layer tree.
   */
  virtual already_AddRefed<TextLayer> CreateTextLayer() = 0;
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a BorderLayer for this manager's layer tree.
   */
  virtual already_AddRefed<BorderLayer> CreateBorderLayer() = 0;
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a CanvasLayer for this manager's layer tree.
   */
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() = 0;
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a ReadbackLayer for this manager's layer tree.
   */
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer() { return nullptr; }
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a RefLayer for this manager's layer tree.
   */
  virtual already_AddRefed<RefLayer> CreateRefLayer() { return nullptr; }
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a DisplayItemLayer for this manager's layer tree.
   */
  virtual already_AddRefed<DisplayItemLayer> CreateDisplayItemLayer() { return nullptr; }
  /**
   * Can be called anytime, from any thread.
   *
   * Creates an Image container which forwards its images to the compositor within
   * layer transactions on the main thread or asynchronously using the ImageBridge IPDL protocol.
   * In the case of asynchronous, If the protocol is not available, the returned ImageContainer
   * will forward images within layer transactions.
   */
  static already_AddRefed<ImageContainer> CreateImageContainer(ImageContainer::Mode flag
                                                                = ImageContainer::SYNCHRONOUS);

  /**
   * Since the lifetimes of display items and display item layers are different,
   * calling this tells the layer manager that the display item layer is valid for
   * only one transaction. Users should call ClearDisplayItemLayers() to remove
   * references to the dead display item at the end of a transaction.
   */
  virtual void TrackDisplayItemLayer(RefPtr<DisplayItemLayer> aLayer);
  virtual void ClearDisplayItemLayers();

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
  virtual already_AddRefed<DrawTarget>
    CreateOptimalDrawTarget(const IntSize &aSize,
                            SurfaceFormat imageFormat);

  /**
   * Creates a DrawTarget for alpha masks which is optimized for inter-
   * operating with this layer manager. In contrast to CreateOptimalDrawTarget,
   * this surface is optimised for drawing alpha only and we assume that
   * drawing the mask is fairly simple.
   */
  virtual already_AddRefed<DrawTarget>
    CreateOptimalMaskDrawTarget(const IntSize &aSize);

  /**
   * Creates a DrawTarget for use with canvas which is optimized for
   * inter-operating with this layermanager.
   */
  virtual already_AddRefed<mozilla::gfx::DrawTarget>
    CreateDrawTarget(const mozilla::gfx::IntSize &aSize,
                     mozilla::gfx::SurfaceFormat aFormat);

  /**
   * Creates a PersistentBufferProvider for use with canvas which is optimized for
   * inter-operating with this layermanager.
   */
  virtual already_AddRefed<PersistentBufferProvider>
    CreatePersistentBufferProvider(const mozilla::gfx::IntSize &aSize,
                                   mozilla::gfx::SurfaceFormat aFormat);

  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize &aSize) { return true; }

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
  void SetUserData(void* aKey, LayerUserData* aData)
  {
    mUserData.Add(static_cast<gfx::UserDataKey*>(aKey), aData, LayerUserDataDestroy);
  }
  /**
   * This can be used anytime. Ownership passes to the caller!
   */
  UniquePtr<LayerUserData> RemoveUserData(void* aKey);

  /**
   * This getter can be used anytime.
   */
  bool HasUserData(void* aKey)
  {
    return mUserData.Has(static_cast<gfx::UserDataKey*>(aKey));
  }
  /**
   * This getter can be used anytime. Ownership is retained by the layer
   * manager.
   */
  LayerUserData* GetUserData(void* aKey) const
  {
    return static_cast<LayerUserData*>(mUserData.Get(static_cast<gfx::UserDataKey*>(aKey)));
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

  /**
   * Make sure that the previous transaction has been entirely
   * completed.
   *
   * Note: This may sychronously wait on a remote compositor
   * to complete rendering.
   */
  virtual void FlushRendering() { }

  /**
   * Make sure that the previous transaction has been
   * received. This will synchronsly wait on a remote compositor. */
  virtual void WaitOnTransactionProcessed() { }

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
  void Dump(std::stringstream& aStream, const char* aPrefix="",
            bool aDumpHtml=false, bool aSorted=false);
  /**
   * Dump information about just this layer manager itself to aStream
   */
  void DumpSelf(std::stringstream& aStream, const char* aPrefix="", bool aSorted=false);
  void Dump(bool aSorted=false);

  /**
   * Dump information about this layer manager and its managed tree to
   * layerscope packet.
   */
  void Dump(layerscope::LayersPacket* aPacket);

  /**
   * Log information about this layer manager and its managed tree to
   * the NSPR log (if enabled for "Layers").
   */
  void Log(const char* aPrefix="");
  /**
   * Log information about just this layer manager itself to the NSPR
   * log (if enabled for "Layers").
   */
  void LogSelf(const char* aPrefix="");

  /**
   * Record (and return) frame-intervals and paint-times for frames which were presented
   *   between calling StartFrameTimeRecording and StopFrameTimeRecording.
   *
   * - Uses a cyclic buffer and serves concurrent consumers, so if Stop is called too late
   *     (elements were overwritten since Start), result is considered invalid and hence empty.
   * - Buffer is capable of holding 10 seconds @ 60fps (or more if frames were less frequent).
   *     Can be changed (up to 1 hour) via pref: toolkit.framesRecording.bufferSize.
   * - Note: the first frame-interval may be longer than expected because last frame
   *     might have been presented some time before calling StartFrameTimeRecording.
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
  virtual void StopFrameTimeRecording(uint32_t         aStartIndex,
                                      nsTArray<float>& aFrameIntervals);

  void RecordFrame();

  static bool IsLogEnabled();
  static mozilla::LogModule* GetLog();

  bool IsCompositingCheap(LayersBackend aBackend)
  {
    // LayersBackend::LAYERS_NONE is an error state, but in that case we should try to
    // avoid loading the compositor!
    return LayersBackend::LAYERS_BASIC != aBackend && LayersBackend::LAYERS_NONE != aBackend;
  }

  virtual bool IsCompositingCheap() { return true; }

  bool IsInTransaction() const { return mInTransaction; }
  virtual void GetFrameUniformity(FrameUniformityData* aOutData) { }

  virtual void SetRegionToClear(const nsIntRegion& aRegion)
  {
    mRegionToClear = aRegion;
  }

  virtual float RequestProperty(const nsAString& property) { return -1; }

  const TimeStamp& GetAnimationReadyTime() const {
    return mAnimationReadyTime;
  }

  virtual bool AsyncPanZoomEnabled() const {
    return false;
  }

  static void LayerUserDataDestroy(void* data);

  void AddPaintedPixelCount(int32_t aCount) {
    mPaintedPixelCount += aCount;
  }

  uint32_t GetAndClearPaintedPixelCount() {
    uint32_t count = mPaintedPixelCount;
    mPaintedPixelCount = 0;
    return count;
  }

  virtual void SetLayerObserverEpoch(uint64_t aLayerObserverEpoch) {}

  virtual void DidComposite(uint64_t aTransactionId,
                            const mozilla::TimeStamp& aCompositeStart,
                            const mozilla::TimeStamp& aCompositeEnd) {}

  virtual void AddDidCompositeObserver(DidCompositeObserver* aObserver) { MOZ_CRASH("GFX: LayerManager"); }
  virtual void RemoveDidCompositeObserver(DidCompositeObserver* aObserver) { MOZ_CRASH("GFX: LayerManager"); }

  virtual void UpdateTextureFactoryIdentifier(const TextureFactoryIdentifier& aNewIdentifier,
											  uint64_t aDeviceResetSeqNo) {}

  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier()
  {
    return TextureFactoryIdentifier();
  }

  virtual void SetTransactionIdAllocator(TransactionIdAllocator* aAllocator) {}

  virtual uint64_t GetLastTransactionId() { return 0; }

  virtual CompositorBridgeChild* GetCompositorBridgeChild() { return nullptr; }

protected:
  RefPtr<Layer> mRoot;
  gfx::UserData mUserData;
  bool mDestroyed;
  bool mSnapEffectiveTransforms;

  nsIntRegion mRegionToClear;

  // Protected destructor, to discourage deletion outside of Release():
  virtual ~LayerManager() {}

  // Print interesting information about this into aStreamo.  Internally
  // used to implement Dump*() and Log*().
  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  // Print interesting information about this into layerscope packet.
  // Internally used to implement Dump().
  virtual void DumpPacket(layerscope::LayersPacket* aPacket);

  uint64_t mId;
  bool mInTransaction;
  // The time when painting most recently finished. This is recorded so that
  // we can time any play-pending animations from this point.
  TimeStamp mAnimationReadyTime;
  // The count of pixels that were painted in the current transaction.
  uint32_t mPaintedPixelCount;
private:
  struct FramesTimingRecording
  {
    // Stores state and data for frame intervals and paint times recording.
    // see LayerManager::StartFrameTimeRecording() at Layers.cpp for more details.
    FramesTimingRecording()
      : mNextIndex(0)
      , mLatestStartIndex(0)
      , mCurrentRunStartIndex(0)
      , mIsPaused(true)
    {}
    nsTArray<float> mIntervals;
    TimeStamp mLastFrameTime;
    uint32_t mNextIndex;
    uint32_t mLatestStartIndex;
    uint32_t mCurrentRunStartIndex;
    bool mIsPaused;
  };
  FramesTimingRecording mRecording;

public:
  /*
   * Methods to store/get/clear a "pending scroll info update" object on a
   * per-scrollid basis. This is used for empty transactions that push over
   * scroll position updates to the APZ code.
   */
  void SetPendingScrollUpdateForNextTransaction(FrameMetrics::ViewID aScrollId,
                                                const ScrollUpdateInfo& aUpdateInfo);
  Maybe<ScrollUpdateInfo> GetPendingScrollInfoUpdate(FrameMetrics::ViewID aScrollId);
  void ClearPendingScrollInfoUpdate();
private:
  std::map<FrameMetrics::ViewID,ScrollUpdateInfo> mPendingScrollUpdates;

  // Display items are only valid during this transaction.
  // At the end of the transaction, we have to go and clear out
  // DisplayItemLayer's and null their display item. See comment
  // above DisplayItemLayer declaration.
  // Since layers are ref counted, we also have to stop holding
  // a reference to the display item layer as well.
  nsTArray<RefPtr<DisplayItemLayer>> mDisplayItemLayers;
};

/**
 * A Layer represents anything that can be rendered onto a destination
 * surface.
 */
class Layer {
  NS_INLINE_DECL_REFCOUNTING(Layer)

  typedef InfallibleTArray<Animation> AnimationArray;

public:
  // Keep these in alphabetical order
  enum LayerType {
    TYPE_CANVAS,
    TYPE_COLOR,
    TYPE_CONTAINER,
    TYPE_DISPLAYITEM,
    TYPE_IMAGE,
    TYPE_TEXT,
    TYPE_BORDER,
    TYPE_READBACK,
    TYPE_REF,
    TYPE_SHADOW,
    TYPE_PAINTED
  };

  /**
   * Returns the LayerManager this Layer belongs to. Note that the layer
   * manager might be in a destroyed state, at which point it's only
   * valid to set/get user data from it.
   */
  LayerManager* Manager() { return mManager; }

  /**
   * This should only be called when changing layer managers from HostLayers.
   */
  void SetManager(LayerManager* aManager, HostLayer* aSelf);

  enum {
    /**
     * If this is set, the caller is promising that by the end of this
     * transaction the entire visible region (as specified by
     * SetVisibleRegion) will be filled with opaque content.
     */
    CONTENT_OPAQUE = 0x01,
    /**
     * If this is set, the caller is notifying that the contents of this layer
     * require per-component alpha for optimal fidelity. However, there is no
     * guarantee that component alpha will be supported for this layer at
     * paint time.
     * This should never be set at the same time as CONTENT_OPAQUE.
     */
    CONTENT_COMPONENT_ALPHA = 0x02,

    /**
     * If this is set then one of the descendant layers of this one has
     * CONTENT_COMPONENT_ALPHA set.
     */
    CONTENT_COMPONENT_ALPHA_DESCENDANT = 0x04,

    /**
     * If this is set then this layer is part of a preserve-3d group, and should
     * be sorted with sibling layers that are also part of the same group.
     */
    CONTENT_EXTEND_3D_CONTEXT = 0x08,
    /**
     * This indicates that the transform may be changed on during an empty
     * transaction where there is no possibility of redrawing the content, so the
     * implementation should be ready for that.
     */
    CONTENT_MAY_CHANGE_TRANSFORM = 0x10,

    /**
     * Disable subpixel AA for this layer. This is used if the display isn't suited
     * for subpixel AA like hidpi or rotated content.
     */
    CONTENT_DISABLE_SUBPIXEL_AA = 0x20,

    /**
     * If this is set then the layer contains content that may look objectionable
     * if not handled as an active layer (such as text with an animated transform).
     * This is for internal layout/FrameLayerBuilder usage only until flattening
     * code is obsoleted. See bug 633097
     */
    CONTENT_DISABLE_FLATTENING = 0x40,

    /**
     * This layer is hidden if the backface of the layer is visible
     * to user.
     */
    CONTENT_BACKFACE_HIDDEN = 0x80
  };
  /**
   * CONSTRUCTION PHASE ONLY
   * This lets layout make some promises about what will be drawn into the
   * visible region of the PaintedLayer. This enables internal quality
   * and performance optimizations.
   */
  void SetContentFlags(uint32_t aFlags)
  {
    NS_ASSERTION((aFlags & (CONTENT_OPAQUE | CONTENT_COMPONENT_ALPHA)) !=
                 (CONTENT_OPAQUE | CONTENT_COMPONENT_ALPHA),
                 "Can't be opaque and require component alpha");
    if (mSimpleAttrs.SetContentFlags(aFlags)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ContentFlags", this));
      MutatedSimple();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Tell this layer which region will be visible. The visible region
   * is a region which contains all the contents of the layer that can
   * actually affect the rendering of the window. It can exclude areas
   * that are covered by opaque contents of other layers, and it can
   * exclude areas where this layer simply contains no content at all.
   * (This can be an overapproximation to the "true" visible region.)
   *
   * There is no general guarantee that drawing outside the bounds of the
   * visible region will be ignored. So if a layer draws outside the bounds
   * of its visible region, it needs to ensure that what it draws is valid.
   */
  virtual void SetVisibleRegion(const LayerIntRegion& aRegion)
  {
    // IsEmpty is required otherwise we get invalidation glitches.
    // See bug 1288464 for investigating why.
    if (!mVisibleRegion.IsEqual(aRegion) || aRegion.IsEmpty()) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) VisibleRegion was %s is %s", this,
        mVisibleRegion.ToString().get(), aRegion.ToString().get()));
      mVisibleRegion = aRegion;
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the (sub)document metrics used to render the Layer subtree
   * rooted at this. Note that a layer may have multiple FrameMetrics
   * objects; calling this function will remove all of them and replace
   * them with the provided FrameMetrics. See the documentation for
   * SetFrameMetrics(const nsTArray<FrameMetrics>&) for more details.
   */
  void SetScrollMetadata(const ScrollMetadata& aScrollMetadata)
  {
    Manager()->ClearPendingScrollInfoUpdate();
    if (mScrollMetadata.Length() != 1 || mScrollMetadata[0] != aScrollMetadata) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) FrameMetrics", this));
      mScrollMetadata.ReplaceElementsAt(0, mScrollMetadata.Length(), aScrollMetadata);
      ScrollMetadataChanged();
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the (sub)document metrics used to render the Layer subtree
   * rooted at this. There might be multiple metrics on this layer
   * because the layer may, for example, be contained inside multiple
   * nested scrolling subdocuments. In general a Layer having multiple
   * ScrollMetadata objects is conceptually equivalent to having a stack
   * of ContainerLayers that have been flattened into this Layer.
   * See the documentation in LayerMetricsWrapper.h for a more detailed
   * explanation of this conceptual equivalence.
   *
   * Note also that there is actually a many-to-many relationship between
   * Layers and ScrollMetadata, because multiple Layers may have identical
   * ScrollMetadata objects. This happens when those layers belong to the
   * same scrolling subdocument and therefore end up with the same async
   * transform when they are scrolled by the APZ code.
   */
  void SetScrollMetadata(const nsTArray<ScrollMetadata>& aMetadataArray)
  {
    Manager()->ClearPendingScrollInfoUpdate();
    if (mScrollMetadata != aMetadataArray) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) FrameMetrics", this));
      mScrollMetadata = aMetadataArray;
      ScrollMetadataChanged();
      Mutated();
    }
  }

  /*
   * Compositor event handling
   * =========================
   * When a touch-start event (or similar) is sent to the AsyncPanZoomController,
   * it needs to decide whether the event should be sent to the main thread.
   * Each layer has a list of event handling regions. When the compositor needs
   * to determine how to handle a touch event, it scans the layer tree from top
   * to bottom in z-order (traversing children before their parents). Points
   * outside the clip region for a layer cause that layer (and its subtree)
   * to be ignored. If a layer has a mask layer, and that mask layer's alpha
   * value is zero at the event point, then the layer and its subtree should
   * be ignored.
   * For each layer, if the point is outside its hit region, we ignore the layer
   * and move onto the next. If the point is inside its hit region but
   * outside the dispatch-to-content region, we can initiate a gesture without
   * consulting the content thread. Otherwise we must dispatch the event to
   * content.
   * Note that if a layer or any ancestor layer has a ForceEmptyHitRegion
   * override in GetEventRegionsOverride() then the hit-region must be treated
   * as empty. Similarly, if there is a ForceDispatchToContent override then
   * the dispatch-to-content region must be treated as encompassing the entire
   * hit region, and therefore we must consult the content thread before
   * initiating a gesture. (If both flags are set, ForceEmptyHitRegion takes
   * priority.)
   */
  /**
   * CONSTRUCTION PHASE ONLY
   * Set the event handling region.
   */
  void SetEventRegions(const EventRegions& aRegions)
  {
    if (mEventRegions != aRegions) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) eventregions were %s, now %s", this,
        mEventRegions.ToString().get(), aRegions.ToString().get()));
      mEventRegions = aRegions;
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the opacity which will be applied to this layer as it
   * is composited to the destination.
   */
  void SetOpacity(float aOpacity)
  {
    if (mSimpleAttrs.SetOpacity(aOpacity)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Opacity", this));
      MutatedSimple();
    }
  }

  void SetMixBlendMode(gfx::CompositionOp aMixBlendMode)
  {
    if (mSimpleAttrs.SetMixBlendMode(aMixBlendMode)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) MixBlendMode", this));
      MutatedSimple();
    }
  }

  void SetForceIsolatedGroup(bool aForceIsolatedGroup)
  {
    if (mSimpleAttrs.SetForceIsolatedGroup(aForceIsolatedGroup)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ForceIsolatedGroup", this));
      MutatedSimple();
    }
  }

  bool GetForceIsolatedGroup() const
  {
    return mSimpleAttrs.ForceIsolatedGroup();
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set a clip rect which will be applied to this layer as it is
   * composited to the destination. The coordinates are relative to
   * the parent layer (i.e. the contents of this layer
   * are transformed before this clip rect is applied).
   * For the root layer, the coordinates are relative to the widget,
   * in device pixels.
   * If aRect is null no clipping will be performed.
   */
  void SetClipRect(const Maybe<ParentLayerIntRect>& aRect)
  {
    if (mClipRect) {
      if (!aRect) {
        MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ClipRect was %d,%d,%d,%d is <none>", this,
                         mClipRect->x, mClipRect->y, mClipRect->width, mClipRect->height));
        mClipRect.reset();
        Mutated();
      } else {
        if (!aRect->IsEqualEdges(*mClipRect)) {
          MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ClipRect was %d,%d,%d,%d is %d,%d,%d,%d", this,
                           mClipRect->x, mClipRect->y, mClipRect->width, mClipRect->height,
                           aRect->x, aRect->y, aRect->width, aRect->height));
          mClipRect = aRect;
          Mutated();
        }
      }
    } else {
      if (aRect) {
        MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ClipRect was <none> is %d,%d,%d,%d", this,
                         aRect->x, aRect->y, aRect->width, aRect->height));
        mClipRect = aRect;
        Mutated();
      }
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set an optional scrolled clip on the layer.
   * The scrolled clip, if present, consists of a clip rect and an optional mask.
   * This scrolled clip is always scrolled by all scroll frames associated with
   * this layer. (By contrast, the scroll clips stored in ScrollMetadata are
   * only scrolled by scroll frames above that ScrollMetadata, and the layer's
   * mClipRect is always fixed to the layer contents (which may or may not be
   * scrolled by some of the scroll frames associated with the layer, depending
   * on whether the layer is fixed).)
   */
  void SetScrolledClip(const Maybe<LayerClip>& aScrolledClip)
  {
    if (mSimpleAttrs.SetScrolledClip(aScrolledClip)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ScrolledClip", this));
      MutatedSimple();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set a layer to mask this layer.
   *
   * The mask layer should be applied using its effective transform (after it
   * is calculated by ComputeEffectiveTransformForMaskLayer), this should use
   * this layer's parent's transform and the mask layer's transform, but not
   * this layer's. That is, the mask layer is specified relative to this layer's
   * position in it's parent layer's coord space.
   * Currently, only 2D translations are supported for the mask layer transform.
   *
   * Ownership of aMaskLayer passes to this.
   * Typical use would be an ImageLayer with an alpha image used for masking.
   * See also ContainerState::BuildMaskLayer in FrameLayerBuilder.cpp.
   */
  void SetMaskLayer(Layer* aMaskLayer)
  {
#ifdef DEBUG
    if (aMaskLayer) {
      bool maskIs2D = aMaskLayer->GetTransform().CanDraw2D();
      NS_ASSERTION(maskIs2D, "Mask layer has invalid transform.");
    }
#endif

    if (mMaskLayer != aMaskLayer) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) MaskLayer", this));
      mMaskLayer = aMaskLayer;
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Add mask layers associated with LayerClips.
   */
  void SetAncestorMaskLayers(const nsTArray<RefPtr<Layer>>& aLayers) {
    if (aLayers != mAncestorMaskLayers) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) AncestorMaskLayers", this));
      mAncestorMaskLayers = aLayers;
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Add a mask layer associated with a LayerClip.
   */
  void AddAncestorMaskLayer(const RefPtr<Layer>& aLayer) {
    mAncestorMaskLayers.AppendElement(aLayer);
    Mutated();
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Tell this layer what its transform should be. The transformation
   * is applied when compositing the layer into its parent container.
   */
  void SetBaseTransform(const gfx::Matrix4x4& aMatrix)
  {
    NS_ASSERTION(!aMatrix.IsSingular(),
                 "Shouldn't be trying to draw with a singular matrix!");
    mPendingTransform = nullptr;
    if (!mSimpleAttrs.SetTransform(aMatrix)) {
      return;
    }
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) BaseTransform", this));
    MutatedSimple();
  }

  /**
   * Can be called at any time.
   *
   * Like SetBaseTransform(), but can be called before the next
   * transform (i.e. outside an open transaction).  Semantically, this
   * method enqueues a new transform value to be set immediately after
   * the next transaction is opened.
   */
  void SetBaseTransformForNextTransaction(const gfx::Matrix4x4& aMatrix)
  {
    mPendingTransform = new gfx::Matrix4x4(aMatrix);
  }

  void SetPostScale(float aXScale, float aYScale)
  {
    if (!mSimpleAttrs.SetPostScale(aXScale, aYScale)) {
      return;
    }
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) PostScale", this));
    MutatedSimple();
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * A layer is "fixed position" when it draws content from a content
   * (not chrome) document, the topmost content document has a root scrollframe
   * with a displayport, but the layer does not move when that displayport scrolls.
   */
  void SetIsFixedPosition(bool aFixedPosition)
  {
    if (mSimpleAttrs.SetIsFixedPosition(aFixedPosition)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) IsFixedPosition", this));
      MutatedSimple();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * This flag is true when the transform on the layer is a perspective
   * transform. The compositor treats perspective transforms specially
   * for async scrolling purposes.
   */
  void SetTransformIsPerspective(bool aTransformIsPerspective)
  {
    if (mSimpleAttrs.SetTransformIsPerspective(aTransformIsPerspective)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) TransformIsPerspective", this));
      MutatedSimple();
    }
  }

  // Ensure that this layer has a valid (non-zero) animations id. This value is
  // unique across layers.
  void EnsureAnimationsId();
  // Call AddAnimation to add a new animation to this layer from layout code.
  // Caller must fill in all the properties of the returned animation.
  // A later animation overrides an earlier one.
  Animation* AddAnimation();
  // ClearAnimations clears animations on this layer.
  virtual void ClearAnimations();
  // This is only called when the layer tree is updated. Do not call this from
  // layout code.  To add an animation to this layer, use AddAnimation.
  void SetCompositorAnimations(const CompositorAnimations& aCompositorAnimations);
  // Go through all animations in this layer and its children and, for
  // any animations with a null start time, update their start time such
  // that at |aReadyTime| the animation's current time corresponds to its
  // 'initial current time' value.
  void StartPendingAnimations(const TimeStamp& aReadyTime);

  // These are a parallel to AddAnimation and clearAnimations, except
  // they add pending animations that apply only when the next
  // transaction is begun.  (See also
  // SetBaseTransformForNextTransaction.)
  Animation* AddAnimationForNextTransaction();
  void ClearAnimationsForNextTransaction();

  /**
   * CONSTRUCTION PHASE ONLY
   * If a layer represents a fixed position element, this data is stored on the
   * layer for use by the compositor.
   *
   *   - |aScrollId| identifies the scroll frame that this element is fixed
   *     with respect to.
   *
   *   - |aAnchor| is the point on the layer that is considered the "anchor"
   *     point, that is, the point which remains in the same position when
   *     compositing the layer tree with a transformation (such as when
   *     asynchronously scrolling and zooming).
   *
   *   - |aSides| is the set of sides to which the element is fixed relative to.
   *     This is used if the viewport size is changed in the compositor and
   *     fixed position items need to shift accordingly. This value is made up
   *     combining appropriate values from mozilla::SideBits.
   */
  void SetFixedPositionData(FrameMetrics::ViewID aScrollId,
                            const LayerPoint& aAnchor,
                            int32_t aSides)
  {
    if (mSimpleAttrs.SetFixedPositionData(aScrollId, aAnchor, aSides)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) FixedPositionData", this));
      MutatedSimple();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * If a layer is "sticky position", |aScrollId| holds the scroll identifier
   * of the scrollable content that contains it. The difference between the two
   * rectangles |aOuter| and |aInner| is treated as two intervals in each
   * dimension, with the current scroll position at the origin. For each
   * dimension, while that component of the scroll position lies within either
   * interval, the layer should not move relative to its scrolling container.
   */
  void SetStickyPositionData(FrameMetrics::ViewID aScrollId, LayerRect aOuter,
                             LayerRect aInner)
  {
    if (mSimpleAttrs.SetStickyPositionData(aScrollId, aOuter, aInner)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) StickyPositionData", this));
      MutatedSimple();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * If a layer is a scroll thumb container layer, set the scroll identifier
   * of the scroll frame scrolled by the thumb, and other data related to the
   * thumb.
   */
  void SetScrollThumbData(FrameMetrics::ViewID aScrollId, const ScrollThumbData& aThumbData)
  {
    if (mSimpleAttrs.SetScrollThumbData(aScrollId, aThumbData)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ScrollbarData", this));
      MutatedSimple();
    }
  }

  // Set during construction for the container layer of scrollbar components.
  // |aScrollId| holds the scroll identifier of the scrollable content that
  // the scrollbar is for.
  void SetIsScrollbarContainer(FrameMetrics::ViewID aScrollId)
  {
    if (mSimpleAttrs.SetIsScrollbarContainer(aScrollId)) {
      MutatedSimple();
    }
  }

  // Used when forwarding transactions. Do not use at any other time.
  void SetSimpleAttributes(const SimpleLayerAttributes& aAttrs) {
    mSimpleAttrs = aAttrs;
  }
  const SimpleLayerAttributes& GetSimpleAttributes() const {
    return mSimpleAttrs;
  }

  // These getters can be used anytime.
  float GetOpacity() { return mSimpleAttrs.Opacity(); }
  gfx::CompositionOp GetMixBlendMode() const { return mSimpleAttrs.MixBlendMode(); }
  const Maybe<ParentLayerIntRect>& GetClipRect() const { return mClipRect; }
  const Maybe<LayerClip>& GetScrolledClip() const { return mSimpleAttrs.ScrolledClip(); }
  Maybe<ParentLayerIntRect> GetScrolledClipRect() const;
  uint32_t GetContentFlags() { return mSimpleAttrs.ContentFlags(); }
  const LayerIntRegion& GetVisibleRegion() const { return mVisibleRegion; }
  const ScrollMetadata& GetScrollMetadata(uint32_t aIndex) const;
  const FrameMetrics& GetFrameMetrics(uint32_t aIndex) const;
  uint32_t GetScrollMetadataCount() const { return mScrollMetadata.Length(); }
  const nsTArray<ScrollMetadata>& GetAllScrollMetadata() { return mScrollMetadata; }
  bool HasScrollableFrameMetrics() const;
  bool IsScrollInfoLayer() const;
  const EventRegions& GetEventRegions() const { return mEventRegions; }
  ContainerLayer* GetParent() { return mParent; }
  Layer* GetNextSibling() {
    if (mNextSibling) {
      mNextSibling->CheckCanary();
    }
    return mNextSibling;
  }
  const Layer* GetNextSibling() const {
    if (mNextSibling) {
      mNextSibling->CheckCanary();
    }
    return mNextSibling;
  }
  Layer* GetPrevSibling() { return mPrevSibling; }
  const Layer* GetPrevSibling() const { return mPrevSibling; }
  virtual Layer* GetFirstChild() const { return nullptr; }
  virtual Layer* GetLastChild() const { return nullptr; }
  gfx::Matrix4x4 GetTransform() const;
  // Same as GetTransform(), but returns the transform as a strongly-typed
  // matrix. Eventually this will replace GetTransform().
  const CSSTransformMatrix GetTransformTyped() const;
  const gfx::Matrix4x4& GetBaseTransform() const { return mSimpleAttrs.Transform(); }
  // Note: these are virtual because ContainerLayerComposite overrides them.
  virtual float GetPostXScale() const { return mSimpleAttrs.PostXScale(); }
  virtual float GetPostYScale() const { return mSimpleAttrs.PostYScale(); }
  bool GetIsFixedPosition() { return mSimpleAttrs.IsFixedPosition(); }
  bool GetTransformIsPerspective() const { return mSimpleAttrs.TransformIsPerspective(); }
  bool GetIsStickyPosition() { return mSimpleAttrs.IsStickyPosition(); }
  FrameMetrics::ViewID GetFixedPositionScrollContainerId() { return mSimpleAttrs.FixedPositionScrollContainerId(); }
  LayerPoint GetFixedPositionAnchor() { return mSimpleAttrs.FixedPositionAnchor(); }
  int32_t GetFixedPositionSides() { return mSimpleAttrs.FixedPositionSides(); }
  FrameMetrics::ViewID GetStickyScrollContainerId() { return mSimpleAttrs.StickyScrollContainerId(); }
  const LayerRect& GetStickyScrollRangeOuter() { return mSimpleAttrs.StickyScrollRangeOuter(); }
  const LayerRect& GetStickyScrollRangeInner() { return mSimpleAttrs.StickyScrollRangeInner(); }
  FrameMetrics::ViewID GetScrollbarTargetContainerId() { return mSimpleAttrs.ScrollbarTargetContainerId(); }
  const ScrollThumbData& GetScrollThumbData() const { return mSimpleAttrs.ThumbData(); }
  bool IsScrollbarContainer() { return mSimpleAttrs.IsScrollbarContainer(); }
  Layer* GetMaskLayer() const { return mMaskLayer; }
  void CheckCanary() const { mCanary.Check(); }

  // Ancestor mask layers are associated with FrameMetrics, but for simplicity
  // in maintaining the layer tree structure we attach them to the layer.
  size_t GetAncestorMaskLayerCount() const {
    return mAncestorMaskLayers.Length();
  }
  Layer* GetAncestorMaskLayerAt(size_t aIndex) const {
    return mAncestorMaskLayers.ElementAt(aIndex);
  }
  const nsTArray<RefPtr<Layer>>& GetAllAncestorMaskLayers() const {
    return mAncestorMaskLayers;
  }

  bool HasMaskLayers() const {
    return GetMaskLayer() || mAncestorMaskLayers.Length() > 0;
  }

  /*
   * Get the combined clip rect of the Layer clip and all clips on FrameMetrics.
   * This is intended for use in Layout. The compositor needs to apply async
   * transforms to find the combined clip.
   */
  Maybe<ParentLayerIntRect> GetCombinedClipRect() const;

  /**
   * Retrieve the root level visible region for |this| taking into account
   * clipping applied to parent layers of |this| as well as subtracting
   * visible regions of higher siblings of this layer and each ancestor.
   *
   * Note translation values for offsets of visible regions and accumulated
   * aLayerOffset are integer rounded using IntPoint::Round.
   *
   * @param aResult - the resulting visible region of this layer.
   * @param aLayerOffset - this layer's total offset from the root layer.
   * @return - false if during layer tree traversal a parent or sibling
   *  transform is found to be non-translational. This method returns early
   *  in this case, results will not be valid. Returns true on successful
   *  traversal.
   */
  bool GetVisibleRegionRelativeToRootLayer(nsIntRegion& aResult,
                                           nsIntPoint* aLayerOffset);

  // Note that all lengths in animation data are either in CSS pixels or app
  // units and must be converted to device pixels by the compositor.
  AnimationArray& GetAnimations() { return mAnimations; }
  uint64_t GetCompositorAnimationsId() { return mCompositorAnimationsId; }
  InfallibleTArray<AnimData>& GetAnimationData() { return mAnimationData; }

  uint64_t GetAnimationGeneration() { return mAnimationGeneration; }
  void SetAnimationGeneration(uint64_t aCount) { mAnimationGeneration = aCount; }

  bool HasTransformAnimation() const;
  bool HasOpacityAnimation() const;

  StyleAnimationValue GetBaseAnimationStyle() const
  {
    return mBaseAnimationStyle;
  }

  /**
   * Returns the local transform for this layer: either mTransform or,
   * for shadow layers, GetShadowBaseTransform(), in either case with the
   * pre- and post-scales applied.
   */
  gfx::Matrix4x4 GetLocalTransform();

  /**
   * Same as GetLocalTransform(), but returns a strongly-typed matrix.
   * Eventually, this will replace GetLocalTransform().
   */
  const LayerToParentLayerMatrix4x4 GetLocalTransformTyped();

  /**
   * Returns the local opacity for this layer: either mOpacity or,
   * for shadow layers, GetShadowOpacity()
   */
  float GetLocalOpacity();

  /**
   * DRAWING PHASE ONLY
   *
   * Apply pending changes to layers before drawing them, if those
   * pending changes haven't been overridden by later changes.
   */
  void ApplyPendingUpdatesToSubtree();

  /**
   * DRAWING PHASE ONLY
   *
   * Write layer-subtype-specific attributes into aAttrs.  Used to
   * synchronize layer attributes to their shadows'.
   */
  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs) { }

  // Returns true if it's OK to save the contents of aLayer in an
  // opaque surface (a surface without an alpha channel).
  // If we can use a surface without an alpha channel, we should, because
  // it will often make painting of antialiased text faster and higher
  // quality.
  bool CanUseOpaqueSurface();

  SurfaceMode GetSurfaceMode()
  {
    if (CanUseOpaqueSurface())
      return SurfaceMode::SURFACE_OPAQUE;
    if (GetContentFlags() & CONTENT_COMPONENT_ALPHA)
      return SurfaceMode::SURFACE_COMPONENT_ALPHA;
    return SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
  }

  // Returns true if this layer can be treated as opaque for visibility
  // computation. A layer may be non-opaque for visibility even if it
  // is not transparent, for example, if it has a mix-blend-mode.
  bool IsOpaqueForVisibility();

  /**
   * This setter can be used anytime. The user data for all keys is
   * initially null. Ownership pases to the layer manager.
   */
  void SetUserData(void* aKey, LayerUserData* aData)
  {
    mUserData.Add(static_cast<gfx::UserDataKey*>(aKey), aData, LayerManager::LayerUserDataDestroy);
  }
  /**
   * This can be used anytime. Ownership passes to the caller!
   */
  UniquePtr<LayerUserData> RemoveUserData(void* aKey);
  /**
   * This getter can be used anytime.
   */
  bool HasUserData(void* aKey)
  {
    return mUserData.Has(static_cast<gfx::UserDataKey*>(aKey));
  }
  /**
   * This getter can be used anytime. Ownership is retained by the layer
   * manager.
   */
  LayerUserData* GetUserData(void* aKey) const
  {
    return static_cast<LayerUserData*>(mUserData.Get(static_cast<gfx::UserDataKey*>(aKey)));
  }

  /**
   * |Disconnect()| is used by layers hooked up over IPC.  It may be
   * called at any time, and may not be called at all.  Using an
   * IPC-enabled layer after Destroy() (drawing etc.) results in a
   * safe no-op; no crashy or uaf etc.
   *
   * XXX: this interface is essentially LayerManager::Destroy, but at
   * Layer granularity.  It might be beneficial to unify them.
   */
  virtual void Disconnect() {}

  /**
   * Dynamic downcast to a PaintedLayer. Returns null if this is not
   * a PaintedLayer.
   */
  virtual PaintedLayer* AsPaintedLayer() { return nullptr; }

  /**
   * Dynamic cast to a ContainerLayer. Returns null if this is not
   * a ContainerLayer.
   */
  virtual ContainerLayer* AsContainerLayer() { return nullptr; }
  virtual const ContainerLayer* AsContainerLayer() const { return nullptr; }

   /**
    * Dynamic cast to a RefLayer. Returns null if this is not a
    * RefLayer.
    */
  virtual RefLayer* AsRefLayer() { return nullptr; }

   /**
    * Dynamic cast to a Color. Returns null if this is not a
    * ColorLayer.
    */
  virtual ColorLayer* AsColorLayer() { return nullptr; }

  /**
    * Dynamic cast to a TextLayer. Returns null if this is not a
    * TextLayer.
    */
  virtual TextLayer* AsTextLayer() { return nullptr; }

  /**
    * Dynamic cast to a Border. Returns null if this is not a
    * ColorLayer.
    */
  virtual BorderLayer* AsBorderLayer() { return nullptr; }

  /**
    * Dynamic cast to a Canvas. Returns null if this is not a
    * ColorLayer.
    */
  virtual CanvasLayer* AsCanvasLayer() { return nullptr; }

  /**
    * Dynamic cast to an Image. Returns null if this is not a
    * ColorLayer.
    */
  virtual ImageLayer* AsImageLayer() { return nullptr; }

  /**
   * Dynamic cast to a LayerComposite.  Return null if this is not a
   * LayerComposite.  Can be used anytime.
   */
  virtual HostLayer* AsHostLayer() { return nullptr; }

  /**
   * Dynamic cast to a ShadowableLayer.  Return null if this is not a
   * ShadowableLayer.  Can be used anytime.
   */
  virtual ShadowableLayer* AsShadowableLayer() { return nullptr; }

  /**
   * Dynamic cast as a DisplayItemLayer. Return null if not a
   * DisplayItemLayer. Can be used anytime.
   */
  virtual DisplayItemLayer* AsDisplayItemLayer() { return nullptr; }

  // These getters can be used anytime.  They return the effective
  // values that should be used when drawing this layer to screen,
  // accounting for this layer possibly being a shadow.
  const Maybe<ParentLayerIntRect>& GetLocalClipRect();
  const LayerIntRegion& GetLocalVisibleRegion();

  bool Extend3DContext() {
    return GetContentFlags() & CONTENT_EXTEND_3D_CONTEXT;
  }
  bool Combines3DTransformWithAncestors() {
    return GetParent() &&
      reinterpret_cast<Layer*>(GetParent())->Extend3DContext();
  }
  bool Is3DContextLeaf() {
    return !Extend3DContext() && Combines3DTransformWithAncestors();
  }
  /**
   * It is true if the user can see the back of the layer and the
   * backface is hidden.  The compositor should skip the layer if the
   * result is true.
   */
  bool IsBackfaceHidden();
  bool IsVisible() {
    // For containers extending 3D context, visible region
    // is meaningless, since they are just intermediate result of
    // content.
    return !GetLocalVisibleRegion().IsEmpty() || Extend3DContext();
  }

  /**
   * Return true if current layer content is opaque.
   * It does not guarantee that layer content is always opaque.
   */
  virtual bool IsOpaque() { return GetContentFlags() & CONTENT_OPAQUE; }

  /**
   * Returns the product of the opacities of this layer and all ancestors up
   * to and excluding the nearest ancestor that has UseIntermediateSurface() set.
   */
  float GetEffectiveOpacity();

  /**
   * Returns the blendmode of this layer.
   */
  gfx::CompositionOp GetEffectiveMixBlendMode();

  /**
   * This returns the effective transform computed by
   * ComputeEffectiveTransforms. Typically this is a transform that transforms
   * this layer all the way to some intermediate surface or destination
   * surface. For non-BasicLayers this will be a transform to the nearest
   * ancestor with UseIntermediateSurface() (or to the root, if there is no
   * such ancestor), but for BasicLayers it's different.
   */
  const gfx::Matrix4x4& GetEffectiveTransform() const { return mEffectiveTransform; }

  /**
   * This returns the effective transform for Layer's buffer computed by
   * ComputeEffectiveTransforms. Typically this is a transform that transforms
   * this layer's buffer all the way to some intermediate surface or destination
   * surface. For non-BasicLayers this will be a transform to the nearest
   * ancestor with UseIntermediateSurface() (or to the root, if there is no
   * such ancestor), but for BasicLayers it's different.
   *
   * By default, its value is same to GetEffectiveTransform().
   * When ImageLayer is rendered with ScaleMode::STRETCH,
   * it becomes different from GetEffectiveTransform().
   */
  virtual const gfx::Matrix4x4& GetEffectiveTransformForBuffer() const
  {
    return mEffectiveTransform;
  }

  /**
   * @param aTransformToSurface the composition of the transforms
   * from the parent layer (if any) to the destination pixel grid.
   *
   * Computes mEffectiveTransform for this layer and all its descendants.
   * mEffectiveTransform transforms this layer up to the destination
   * pixel grid (whatever aTransformToSurface is relative to).
   *
   * We promise that when this is called on a layer, all ancestor layers
   * have already had ComputeEffectiveTransforms called.
   */
  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) = 0;

  /**
   * Computes the effective transform for mask layers, if this layer has any.
   */
  void ComputeEffectiveTransformForMaskLayers(const gfx::Matrix4x4& aTransformToSurface);
  static void ComputeEffectiveTransformForMaskLayer(Layer* aMaskLayer,
                                                    const gfx::Matrix4x4& aTransformToSurface);

  /**
   * Calculate the scissor rect required when rendering this layer.
   * Returns a rectangle relative to the intermediate surface belonging to the
   * nearest ancestor that has an intermediate surface, or relative to the root
   * viewport if no ancestor has an intermediate surface, corresponding to the
   * clip rect for this layer intersected with aCurrentScissorRect.
   */
  RenderTargetIntRect CalculateScissorRect(const RenderTargetIntRect& aCurrentScissorRect);

  virtual const char* Name() const =0;
  virtual LayerType GetType() const =0;

  /**
   * Only the implementation should call this. This is per-implementation
   * private data. Normally, all layers with a given layer manager
   * use the same type of ImplData.
   */
  void* ImplData() { return mImplData; }

  /**
   * Only the implementation should use these methods.
   */
  void SetParent(ContainerLayer* aParent) { mParent = aParent; }
  void SetNextSibling(Layer* aSibling) { mNextSibling = aSibling; }
  void SetPrevSibling(Layer* aSibling) { mPrevSibling = aSibling; }

  /**
   * Dump information about this layer manager and its managed tree to
   * aStream.
   */
  void Dump(std::stringstream& aStream, const char* aPrefix="",
            bool aDumpHtml=false, bool aSorted=false,
            const Maybe<gfx::Polygon>& aGeometry=Nothing());
  /**
   * Dump information about just this layer manager itself to aStream.
   */
  void DumpSelf(std::stringstream& aStream, const char* aPrefix="",
                const Maybe<gfx::Polygon>& aGeometry=Nothing());

  /**
   * Dump information about this layer and its child & sibling layers to
   * layerscope packet.
   */
  void Dump(layerscope::LayersPacket* aPacket, const void* aParent);

  /**
   * Log information about this layer manager and its managed tree to
   * the NSPR log (if enabled for "Layers").
   */
  void Log(const char* aPrefix="");
  /**
   * Log information about just this layer manager itself to the NSPR
   * log (if enabled for "Layers").
   */
  void LogSelf(const char* aPrefix="");

  // Print interesting information about this into aStream. Internally
  // used to implement Dump*() and Log*(). If subclasses have
  // additional interesting properties, they should override this with
  // an implementation that first calls the base implementation then
  // appends additional info to aTo.
  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  // Just like PrintInfo, but this function dump information into layerscope packet,
  // instead of a StringStream. It is also internally used to implement Dump();
  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent);

  /**
   * Store display list log.
   */
  void SetDisplayListLog(const char *log);

  /**
   * Return display list log.
   */
  void GetDisplayListLog(nsCString& log);

  static bool IsLogEnabled() { return LayerManager::IsLogEnabled(); }

  /**
   * Returns the current area of the layer (in layer-space coordinates)
   * marked as needed to be recomposited.
   */
  const virtual gfx::TiledIntRegion& GetInvalidRegion() { return mInvalidRegion; }
  void AddInvalidRegion(const nsIntRegion& aRegion) {
    mInvalidRegion.Add(aRegion);
  }

  /**
   * Mark the entirety of the layer's visible region as being invalid.
   */
  void SetInvalidRectToVisibleRegion()
  {
    mInvalidRegion.SetEmpty();
    mInvalidRegion.Add(GetVisibleRegion().ToUnknownRegion());
  }

  /**
   * Adds to the current invalid rect.
   */
  void AddInvalidRect(const gfx::IntRect& aRect) { mInvalidRegion.Add(aRect); }

  /**
   * Clear the invalid rect, marking the layer as being identical to what is currently
   * composited.
   */
  virtual void ClearInvalidRegion() { mInvalidRegion.SetEmpty(); }

  // These functions allow attaching an AsyncPanZoomController to this layer,
  // and can be used anytime.
  // A layer has an APZC at index aIndex only-if GetFrameMetrics(aIndex).IsScrollable();
  // attempting to get an APZC for a non-scrollable metrics will return null.
  // The aIndex for these functions must be less than GetScrollMetadataCount().
  void SetAsyncPanZoomController(uint32_t aIndex, AsyncPanZoomController *controller);
  AsyncPanZoomController* GetAsyncPanZoomController(uint32_t aIndex) const;
  // The ScrollMetadataChanged function is used internally to ensure the APZC array length
  // matches the frame metrics array length.

  virtual void ClearCachedResources() {}

  virtual bool SupportsAsyncUpdate() { return false; }
private:
  void ScrollMetadataChanged();
public:

  void ApplyPendingUpdatesForThisTransaction();

#ifdef DEBUG
  void SetDebugColorIndex(uint32_t aIndex) { mDebugColorIndex = aIndex; }
  uint32_t GetDebugColorIndex() { return mDebugColorIndex; }
#endif

  void Mutated() {
    mManager->Mutated(this);
  }
  void MutatedSimple() {
    mManager->MutatedSimple(this);
  }

  virtual int32_t GetMaxLayerSize() { return Manager()->GetMaxTextureSize(); }

  /**
   * Returns true if this layer's effective transform is not just
   * a translation by integers, or if this layer or some ancestor layer
   * is marked as having a transform that may change without a full layer
   * transaction.
   */
  bool MayResample();

  RenderTargetRect TransformRectToRenderTarget(const LayerIntRect& aRect);

  /**
   * Add debugging information to the layer dump.
   */
  void AddExtraDumpInfo(const nsACString& aStr)
  {
#ifdef MOZ_DUMP_PAINTING
    mExtraDumpInfo.AppendElement(aStr);
#endif
  }

  /**
   * Clear debugging information. Useful for recycling.
   */
  void ClearExtraDumpInfo()
  {
#ifdef MOZ_DUMP_PAINTING
     mExtraDumpInfo.Clear();
#endif
  }

protected:
  Layer(LayerManager* aManager, void* aImplData);

  // Protected destructor, to discourage deletion outside of Release():
  virtual ~Layer();

  /**
   * We can snap layer transforms for two reasons:
   * 1) To avoid unnecessary resampling when a transform is a translation
   * by a non-integer number of pixels.
   * Snapping the translation to an integer number of pixels avoids
   * blurring the layer and can be faster to composite.
   * 2) When a layer is used to render a rectangular object, we need to
   * emulate the rendering of rectangular inactive content and snap the
   * edges of the rectangle to pixel boundaries. This is both to ensure
   * layer rendering is consistent with inactive content rendering, and to
   * avoid seams.
   * This function implements type 1 snapping. If aTransform is a 2D
   * translation, and this layer's layer manager has enabled snapping
   * (which is the default), return aTransform with the translation snapped
   * to nearest pixels. Otherwise just return aTransform. Call this when the
   * layer does not correspond to a single rectangular content object.
   * This function does not try to snap if aTransform has a scale, because in
   * that case resampling is inevitable and there's no point in trying to
   * avoid it. In fact snapping can cause problems because pixel edges in the
   * layer's content can be rendered unpredictably (jiggling) as the scale
   * interacts with the snapping of the translation, especially with animated
   * transforms.
   * @param aResidualTransform a transform to apply before the result transform
   * in order to get the results to completely match aTransform.
   */
  gfx::Matrix4x4 SnapTransformTranslation(const gfx::Matrix4x4& aTransform,
                                          gfx::Matrix* aResidualTransform);
  gfx::Matrix4x4 SnapTransformTranslation3D(const gfx::Matrix4x4& aTransform,
                                            gfx::Matrix* aResidualTransform);
  /**
   * See comment for SnapTransformTranslation.
   * This function implements type 2 snapping. If aTransform is a translation
   * and/or scale, transform aSnapRect by aTransform, snap to pixel boundaries,
   * and return the transform that maps aSnapRect to that rect. Otherwise
   * just return aTransform.
   * @param aSnapRect a rectangle whose edges should be snapped to pixel
   * boundaries in the destination surface.
   * @param aResidualTransform a transform to apply before the result transform
   * in order to get the results to completely match aTransform.
   */
  gfx::Matrix4x4 SnapTransform(const gfx::Matrix4x4& aTransform,
                               const gfxRect& aSnapRect,
                               gfx::Matrix* aResidualTransform);

  LayerManager* mManager;
  ContainerLayer* mParent;
  Layer* mNextSibling;
  Layer* mPrevSibling;
  void* mImplData;
  RefPtr<Layer> mMaskLayer;
  nsTArray<RefPtr<Layer>> mAncestorMaskLayers;
  // Look for out-of-bound in the middle of the structure
  mozilla::CorruptionCanary mCanary;
  gfx::UserData mUserData;
  SimpleLayerAttributes mSimpleAttrs;
  LayerIntRegion mVisibleRegion;
  nsTArray<ScrollMetadata> mScrollMetadata;
  EventRegions mEventRegions;
  // A mutation of |mTransform| that we've queued to be applied at the
  // end of the next transaction (if nothing else overrides it in the
  // meantime).
  nsAutoPtr<gfx::Matrix4x4> mPendingTransform;
  gfx::Matrix4x4 mEffectiveTransform;
  AnimationArray mAnimations;
  uint64_t mCompositorAnimationsId;
  // See mPendingTransform above.
  nsAutoPtr<AnimationArray> mPendingAnimations;
  InfallibleTArray<AnimData> mAnimationData;
  Maybe<ParentLayerIntRect> mClipRect;
  gfx::IntRect mTileSourceRect;
  gfx::TiledIntRegion mInvalidRegion;
  nsTArray<RefPtr<AsyncPanZoomController> > mApzcs;
  bool mUseTileSourceRect;
#ifdef DEBUG
  uint32_t mDebugColorIndex;
#endif
  // If this layer is used for OMTA, then this counter is used to ensure we
  // stay in sync with the animation manager
  uint64_t mAnimationGeneration;
#ifdef MOZ_DUMP_PAINTING
  nsTArray<nsCString> mExtraDumpInfo;
#endif
  // Store display list log.
  nsCString mDisplayListLog;

  StyleAnimationValue mBaseAnimationStyle;
};

/**
 * A Layer which we can paint into. It is a conceptually
 * infinite surface, but each PaintedLayer has an associated "valid region"
 * of contents that it is currently storing, which is finite. PaintedLayer
 * implementations can store content between paints.
 *
 * PaintedLayers are rendered into during the drawing phase of a transaction.
 *
 * Currently the contents of a PaintedLayer are in the device output color
 * space.
 */
class PaintedLayer : public Layer {
public:
  /**
   * CONSTRUCTION PHASE ONLY
   * Tell this layer that the content in some region has changed and
   * will need to be repainted. This area is removed from the valid
   * region.
   */
  virtual void InvalidateRegion(const nsIntRegion& aRegion) = 0;
  /**
   * CONSTRUCTION PHASE ONLY
   * Set whether ComputeEffectiveTransforms should compute the
   * "residual translation" --- the translation that should be applied *before*
   * mEffectiveTransform to get the ideal transform for this PaintedLayer.
   * When this is true, ComputeEffectiveTransforms will compute the residual
   * and ensure that the layer is invalidated whenever the residual changes.
   * When it's false, a change in the residual will not trigger invalidation
   * and GetResidualTranslation will return 0,0.
   * So when the residual is to be ignored, set this to false for better
   * performance.
   */
  void SetAllowResidualTranslation(bool aAllow) { mAllowResidualTranslation = aAllow; }

  void SetValidRegion(const nsIntRegion& aRegion)
  {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ValidRegion", this));
    mValidRegion = aRegion;
    mValidRegionIsCurrent = true;
    Mutated();
  }

  /**
   * Can be used anytime
   */
  const nsIntRegion& GetValidRegion() const
  {
    EnsureValidRegionIsCurrent();
    return mValidRegion;
  }

  void InvalidateWholeLayer()
  {
    mInvalidRegion.Add(GetValidRegion().GetBounds());
    ClearValidRegion();
  }

  void ClearValidRegion()
  {
    mValidRegion.SetEmpty();
    mValidRegionIsCurrent = true;
  }
  void AddToValidRegion(const nsIntRegion& aRegion)
  {
    EnsureValidRegionIsCurrent();
    mValidRegion.OrWith(aRegion);
  }
  void SubtractFromValidRegion(const nsIntRegion& aRegion)
  {
    EnsureValidRegionIsCurrent();
    mValidRegion.SubOut(aRegion);
  }
  void UpdateValidRegionAfterInvalidRegionChanged()
  {
    // Changes to mInvalidRegion will be applied to mValidRegion on the next
    // call to EnsureValidRegionIsCurrent().
    mValidRegionIsCurrent = false;
  }

  void ClearInvalidRegion() override
  {
    // mInvalidRegion is about to be reset. This is the last chance to apply
    // any pending changes from it to mValidRegion. Do that by calling
    // EnsureValidRegionIsCurrent().
    EnsureValidRegionIsCurrent();
    mInvalidRegion.SetEmpty();
  }

  virtual PaintedLayer* AsPaintedLayer() override { return this; }

  MOZ_LAYER_DECL_NAME("PaintedLayer", TYPE_PAINTED)

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override
  {
    gfx::Matrix4x4 idealTransform = GetLocalTransform() * aTransformToSurface;
    gfx::Matrix residual;
    mEffectiveTransform = SnapTransformTranslation(idealTransform,
        mAllowResidualTranslation ? &residual : nullptr);
    // The residual can only be a translation because SnapTransformTranslation
    // only changes the transform if it's a translation
    NS_ASSERTION(residual.IsTranslation(),
                 "Residual transform can only be a translation");
    if (!gfx::ThebesPoint(residual.GetTranslation()).WithinEpsilonOf(mResidualTranslation, 1e-3f)) {
      mResidualTranslation = gfx::ThebesPoint(residual.GetTranslation());
      DebugOnly<mozilla::gfx::Point> transformedOrig =
        idealTransform.TransformPoint(mozilla::gfx::Point());
#ifdef DEBUG
      DebugOnly<mozilla::gfx::Point> transformed = idealTransform.TransformPoint(
        mozilla::gfx::Point(mResidualTranslation.x, mResidualTranslation.y)
      ) - *&transformedOrig;
#endif
      NS_ASSERTION(-0.5 <= (&transformed)->x && (&transformed)->x < 0.5 &&
                   -0.5 <= (&transformed)->y && (&transformed)->y < 0.5,
                   "Residual translation out of range");
      ClearValidRegion();
    }
    ComputeEffectiveTransformForMaskLayers(aTransformToSurface);
  }

  LayerManager::PaintedLayerCreationHint GetCreationHint() const { return mCreationHint; }

  bool UsedForReadback() { return mUsedForReadback; }
  void SetUsedForReadback(bool aUsed) { mUsedForReadback = aUsed; }

  /**
   * Returns true if aLayer is optimized for the given PaintedLayerCreationHint.
   */
  virtual bool IsOptimizedFor(LayerManager::PaintedLayerCreationHint aCreationHint)
  { return true; }

  /**
   * Returns the residual translation. Apply this translation when drawing
   * into the PaintedLayer so that when mEffectiveTransform is applied afterwards
   * by layer compositing, the results exactly match the "ideal transform"
   * (the product of the transform of this layer and its ancestors).
   * Returns 0,0 unless SetAllowResidualTranslation(true) has been called.
   * The residual translation components are always in the range [-0.5, 0.5).
   */
  gfxPoint GetResidualTranslation() const { return mResidualTranslation; }

protected:
  PaintedLayer(LayerManager* aManager, void* aImplData,
              LayerManager::PaintedLayerCreationHint aCreationHint = LayerManager::NONE)
    : Layer(aManager, aImplData)
    , mValidRegion()
    , mValidRegionIsCurrent(true)
    , mCreationHint(aCreationHint)
    , mUsedForReadback(false)
    , mAllowResidualTranslation(false)
  {
  }

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent) override;

  /**
   * ComputeEffectiveTransforms snaps the ideal transform to get mEffectiveTransform.
   * mResidualTranslation is the translation that should be applied *before*
   * mEffectiveTransform to get the ideal transform.
   */
  gfxPoint mResidualTranslation;

private:
  /**
   * Needs to be called prior to accessing mValidRegion, unless mValidRegion is
   * being completely overwritten.
   */
  void EnsureValidRegionIsCurrent() const
  {
    if (!mValidRegionIsCurrent) {
      // Apply any pending mInvalidRegion changes to mValidRegion.
      if (!mValidRegion.IsEmpty()) {
        // Calling mInvalidRegion.GetRegion() is expensive.
        // That's why we delay the adjustment of mValidRegion for as long as
        // possible, so that multiple modifications to mInvalidRegion can be
        // applied to mValidRegion in one go.
        mValidRegion.SubOut(mInvalidRegion.GetRegion());
      }
      mValidRegionIsCurrent = true;
    }
  }

  /**
   * The layer's valid region. If mValidRegionIsCurrent is false, then
   * mValidRegion has not yet been updated for recent changes to
   * mInvalidRegion. Those pending changes can be applied by calling
   * EnsureValidRegionIsCurrent().
   */
  mutable nsIntRegion mValidRegion;

  mutable bool mValidRegionIsCurrent;

protected:
  /**
   * The creation hint that was used when constructing this layer.
   */
  const LayerManager::PaintedLayerCreationHint mCreationHint;
  /**
   * Set when this PaintedLayer is participating in readback, i.e. some
   * ReadbackLayer (may) be getting its background from this layer.
   */
  bool mUsedForReadback;
  /**
   * True when
   */
  bool mAllowResidualTranslation;
};

/**
 * A Layer which other layers render into. It holds references to its
 * children.
 */
class ContainerLayer : public Layer {
public:

  ~ContainerLayer();

  /**
   * CONSTRUCTION PHASE ONLY
   * Insert aChild into the child list of this container. aChild must
   * not be currently in any child list or the root for the layer manager.
   * If aAfter is non-null, it must be a child of this container and
   * we insert after that layer. If it's null we insert at the start.
   */
  virtual bool InsertAfter(Layer* aChild, Layer* aAfter);
  /**
   * CONSTRUCTION PHASE ONLY
   * Remove aChild from the child list of this container. aChild must
   * be a child of this container.
   */
  virtual bool RemoveChild(Layer* aChild);
  /**
   * CONSTRUCTION PHASE ONLY
   * Reposition aChild from the child list of this container. aChild must
   * be a child of this container.
   * If aAfter is non-null, it must be a child of this container and we
   * reposition after that layer. If it's null, we reposition at the start.
   */
  virtual bool RepositionChild(Layer* aChild, Layer* aAfter);

  void SetPreScale(float aXScale, float aYScale)
  {
    if (mPreXScale == aXScale && mPreYScale == aYScale) {
      return;
    }

    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) PreScale", this));
    mPreXScale = aXScale;
    mPreYScale = aYScale;
    Mutated();
  }

  void SetInheritedScale(float aXScale, float aYScale)
  {
    if (mInheritedXScale == aXScale && mInheritedYScale == aYScale) {
      return;
    }

    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) InheritedScale", this));
    mInheritedXScale = aXScale;
    mInheritedYScale = aYScale;
    Mutated();
  }

  void SetScaleToResolution(bool aScaleToResolution, float aResolution)
  {
    if (mScaleToResolution == aScaleToResolution && mPresShellResolution == aResolution) {
      return;
    }

    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ScaleToResolution", this));
    mScaleToResolution = aScaleToResolution;
    mPresShellResolution = aResolution;
    Mutated();
  }

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs) override;

  enum class SortMode {
    WITH_GEOMETRY,
    WITHOUT_GEOMETRY,
  };

  nsTArray<LayerPolygon> SortChildrenBy3DZOrder(SortMode aSortMode);

  virtual ContainerLayer* AsContainerLayer() override { return this; }
  virtual const ContainerLayer* AsContainerLayer() const override { return this; }

  // These getters can be used anytime.
  virtual Layer* GetFirstChild() const override { return mFirstChild; }
  virtual Layer* GetLastChild() const override { return mLastChild; }
  float GetPreXScale() const { return mPreXScale; }
  float GetPreYScale() const { return mPreYScale; }
  float GetInheritedXScale() const { return mInheritedXScale; }
  float GetInheritedYScale() const { return mInheritedYScale; }
  float GetPresShellResolution() const { return mPresShellResolution; }
  bool ScaleToResolution() const { return mScaleToResolution; }

  MOZ_LAYER_DECL_NAME("ContainerLayer", TYPE_CONTAINER)

  /**
   * ContainerLayer backends need to override ComputeEffectiveTransforms
   * since the decision about whether to use a temporary surface for the
   * container is backend-specific. ComputeEffectiveTransforms must also set
   * mUseIntermediateSurface.
   */
  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override = 0;

  /**
   * Call this only after ComputeEffectiveTransforms has been invoked
   * on this layer.
   * Returns true if this will use an intermediate surface. This is largely
   * backend-dependent, but it affects the operation of GetEffectiveOpacity().
   */
  bool UseIntermediateSurface() { return mUseIntermediateSurface; }

  /**
   * Returns the rectangle covered by the intermediate surface,
   * in this layer's coordinate system.
   *
   * NOTE: Since this layer has an intermediate surface it follows
   *       that LayerPixel == RenderTargetPixel
   */
  RenderTargetIntRect GetIntermediateSurfaceRect()
  {
    NS_ASSERTION(mUseIntermediateSurface, "Must have intermediate surface");
    return RenderTargetIntRect::FromUnknownRect(GetLocalVisibleRegion().ToUnknownRegion().GetBounds());
  }

  /**
   * Returns true if this container has more than one non-empty child
   */
  bool HasMultipleChildren();

  /**
   * Returns true if this container supports children with component alpha.
   * Should only be called while painting a child of this layer.
   */
  bool SupportsComponentAlphaChildren() { return mSupportsComponentAlphaChildren; }

  /**
   * Returns true if aLayer or any layer in its parent chain has the opaque
   * content flag set.
   */
  static bool HasOpaqueAncestorLayer(Layer* aLayer);

  void SetChildrenChanged(bool aVal) {
    mChildrenChanged = aVal;
  }

  void SetEventRegionsOverride(EventRegionsOverride aVal) {
    if (mEventRegionsOverride == aVal) {
      return;
    }

    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) EventRegionsOverride", this));
    mEventRegionsOverride = aVal;
    Mutated();
  }

  EventRegionsOverride GetEventRegionsOverride() const {
    return mEventRegionsOverride;
  }

  void SetFilterChain(nsTArray<CSSFilter>&& aFilterChain) {
    mFilterChain = aFilterChain;
  }

  nsTArray<CSSFilter>& GetFilterChain() { return mFilterChain; }
  
  virtual void SetInvalidCompositeRect(const gfx::IntRect& aRect) {}

protected:
  friend class ReadbackProcessor;

  // Note that this is not virtual, and is based on the implementation of
  // ContainerLayer::RemoveChild, so it should only be called where you would
  // want to explicitly call the base class implementation of RemoveChild;
  // e.g., while (mFirstChild) ContainerLayer::RemoveChild(mFirstChild);
  void RemoveAllChildren();

  void DidInsertChild(Layer* aLayer);
  void DidRemoveChild(Layer* aLayer);

  bool AnyAncestorOrThisIs3DContextLeaf();

  void Collect3DContextLeaves(nsTArray<Layer*>& aToSort);

  // Collects child layers that do not extend 3D context. For ContainerLayers
  // that do extend 3D context, the 3D context leaves are collected.
  nsTArray<Layer*> CollectChildren() {
    nsTArray<Layer*> children;

    for (Layer* layer = GetFirstChild(); layer; layer = layer->GetNextSibling()) {
      ContainerLayer* container = layer->AsContainerLayer();

      if (container && container->Extend3DContext() &&
          !container->UseIntermediateSurface()) {
        container->Collect3DContextLeaves(children);
      } else {
        children.AppendElement(layer);
      }
    }

    return children;
  }

  ContainerLayer(LayerManager* aManager, void* aImplData);

  /**
   * A default implementation of ComputeEffectiveTransforms for use by OpenGL
   * and D3D.
   */
  void DefaultComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface);

  /**
   * A default implementation to compute and set the value for SupportsComponentAlphaChildren().
   *
   * If aNeedsSurfaceCopy is provided, then it is set to true if the caller needs to copy the background
   * up into the intermediate surface created, false otherwise.
   */
  void DefaultComputeSupportsComponentAlphaChildren(bool* aNeedsSurfaceCopy = nullptr);

  /**
   * Loops over the children calling ComputeEffectiveTransforms on them.
   */
  void ComputeEffectiveTransformsForChildren(const gfx::Matrix4x4& aTransformToSurface);

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent) override;

  /**
   * True for if the container start a new 3D context extended by one
   * or more children.
   */
  bool Creates3DContextWithExtendingChildren();

  Layer* mFirstChild;
  Layer* mLastChild;
  float mPreXScale;
  float mPreYScale;
  // The resolution scale inherited from the parent layer. This will already
  // be part of mTransform.
  float mInheritedXScale;
  float mInheritedYScale;
  // For layers corresponding to an nsDisplayResolution, the resolution of the
  // associated pres shell; for other layers, 1.0.
  float mPresShellResolution;
  // Whether the compositor should scale to mPresShellResolution.
  bool mScaleToResolution;
  bool mUseIntermediateSurface;
  bool mSupportsComponentAlphaChildren;
  bool mMayHaveReadbackChild;
  // This is updated by ComputeDifferences. This will be true if we need to invalidate
  // the intermediate surface.
  bool mChildrenChanged;
  EventRegionsOverride mEventRegionsOverride;
  nsTArray<CSSFilter> mFilterChain;
};

/**
 * A generic layer that references back to its display item.
 *
 * In order to not throw away information early in the pipeline from layout -> webrender,
 * we'd like a generic layer type that can represent all the nsDisplayItems instead of
 * creating a new layer type for each nsDisplayItem for Webrender. Another option
 * is to break down nsDisplayItems into smaller nsDisplayItems early in the pipeline.
 * The problem with this is that the whole pipeline would have to deal with more
 * display items, which is slower.
 *
 * An alternative is to create a DisplayItemLayer, but the wrinkle with this is that
 * it has a pointer to its nsDisplayItem. Managing the lifetime is key as display items
 * only live as long as their display list builder, which goes away at the end of a paint.
 * Layers however, are retained between paints.
 * It's ok to recycle a DisplayItemLayer for a different display item since its just a pointer.
 * Instead, when a layer transaction is completed, it is up to the layer manager to tell
 * DisplayItemLayers that the display item pointer is no longer valid.
 */
class DisplayItemLayer : public Layer {
  public:
    virtual DisplayItemLayer* AsDisplayItemLayer() override { return this; }
    void EndTransaction();

    MOZ_LAYER_DECL_NAME("DisplayItemLayer", TYPE_DISPLAYITEM)

    void SetDisplayItem(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder) {
      mItem = aItem;
      mBuilder = aBuilder;
    }

    nsDisplayItem* GetDisplayItem() { return mItem; }
    nsDisplayListBuilder* GetDisplayListBuilder() { return mBuilder; }

    virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override
    {
      gfx::Matrix4x4 idealTransform = GetLocalTransform() * aTransformToSurface;
      mEffectiveTransform = SnapTransformTranslation(idealTransform, nullptr);
      ComputeEffectiveTransformForMaskLayers(aTransformToSurface);
    }

  protected:
    DisplayItemLayer(LayerManager* aManager, void* aImplData)
      : Layer(aManager, aImplData)
      , mItem(nullptr)
  {}

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent) override;

  // READ COMMENT ABOVE TO ENSURE WE DON'T HAVE A DANGLING POINTER
  nsDisplayItem* mItem;
  nsDisplayListBuilder* mBuilder;
};

/**
 * A Layer which just renders a solid color in its visible region. It actually
 * can fill any area that contains the visible region, so if you need to
 * restrict the area filled, set a clip region on this layer.
 */
class ColorLayer : public Layer {
public:
  virtual ColorLayer* AsColorLayer() override { return this; }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the color of the layer.
   */
  virtual void SetColor(const gfx::Color& aColor)
  {
    if (mColor != aColor) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Color", this));
      mColor = aColor;
      Mutated();
    }
  }

  void SetBounds(const gfx::IntRect& aBounds)
  {
    if (!mBounds.IsEqualEdges(aBounds)) {
      mBounds = aBounds;
      Mutated();
    }
  }

  const gfx::IntRect& GetBounds()
  {
    return mBounds;
  }

  // This getter can be used anytime.
  virtual const gfx::Color& GetColor() { return mColor; }

  MOZ_LAYER_DECL_NAME("ColorLayer", TYPE_COLOR)

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override
  {
    gfx::Matrix4x4 idealTransform = GetLocalTransform() * aTransformToSurface;
    mEffectiveTransform = SnapTransformTranslation(idealTransform, nullptr);
    ComputeEffectiveTransformForMaskLayers(aTransformToSurface);
  }

protected:
  ColorLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData)
    , mColor()
  {}

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent) override;

  gfx::IntRect mBounds;
  gfx::Color mColor;
};

/**
 * A Layer which renders Glyphs.
 */
class TextLayer : public Layer {
public:
  virtual TextLayer* AsTextLayer() override { return this; }

  /**
   * CONSTRUCTION PHASE ONLY
   */
  void SetBounds(const gfx::IntRect& aBounds)
  {
    if (!mBounds.IsEqualEdges(aBounds)) {
      mBounds = aBounds;
      Mutated();
    }
  }

  const gfx::IntRect& GetBounds()
  {
    return mBounds;
  }

  void SetScaledFont(gfx::ScaledFont* aScaledFont) {
    if (aScaledFont != mFont) {
      mFont = aScaledFont;
      Mutated();
    }
  }

  const nsTArray<GlyphArray>& GetGlyphs() { return mGlyphs; }

  gfx::ScaledFont* GetScaledFont() { return mFont; }

  MOZ_LAYER_DECL_NAME("TextLayer", TYPE_TEXT)

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override
  {
    gfx::Matrix4x4 idealTransform = GetLocalTransform() * aTransformToSurface;
    mEffectiveTransform = SnapTransformTranslation(idealTransform, nullptr);
    ComputeEffectiveTransformForMaskLayers(aTransformToSurface);
  }

  virtual void SetGlyphs(nsTArray<GlyphArray>&& aGlyphs);
protected:
  TextLayer(LayerManager* aManager, void* aImplData);
  ~TextLayer();

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent) override;

  gfx::IntRect mBounds;
  nsTArray<GlyphArray> mGlyphs;
  RefPtr<gfx::ScaledFont> mFont;
};

/**
 * A Layer which renders a rounded rect.
 */
class BorderLayer : public Layer {
public:
  virtual BorderLayer* AsBorderLayer() override { return this; }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the color of the layer.
   */

  // Colors of each side as in css::Side
  virtual void SetColors(const BorderColors& aColors)
  {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Colors", this));
    PodCopy(&mColors[0], &aColors[0], 4);
    Mutated();
  }

  virtual void SetRect(const LayerRect& aRect)
  {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Rect", this));
    mRect = aRect;
    Mutated();
  }

  // Size of each rounded corner as in css::Corner, 0.0 means a
  // rectangular corner.
  virtual void SetCornerRadii(const BorderCorners& aCorners)
  {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Corners", this));
    PodCopy(&mCorners[0], &aCorners[0], 4);
    Mutated();
  }

  virtual void SetWidths(const BorderWidths& aWidths)
  {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Widths", this));
    PodCopy(&mWidths[0], &aWidths[0], 4);
    Mutated();
  }

  virtual void SetStyles(const BorderStyles& aBorderStyles)
  {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Widths", this));
    PodCopy(&mBorderStyles[0], &aBorderStyles[0], 4);
    Mutated();
  }

  MOZ_LAYER_DECL_NAME("BorderLayer", TYPE_BORDER)

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override
  {
    gfx::Matrix4x4 idealTransform = GetLocalTransform() * aTransformToSurface;
    mEffectiveTransform = SnapTransformTranslation(idealTransform, nullptr);
    ComputeEffectiveTransformForMaskLayers(aTransformToSurface);
  }

  const BorderColors& GetColors() { return mColors; }
  const LayerRect& GetRect() { return mRect; }
  const BorderCorners& GetCorners() { return mCorners; }
  const BorderWidths& GetWidths() { return mWidths; }

protected:
  BorderLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData)
  {}

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent) override;

  BorderColors mColors;
  LayerRect mRect;
  BorderCorners mCorners;
  BorderWidths mWidths;
  BorderStyles mBorderStyles;
};

/**
 * A Layer for HTML Canvas elements.  It's backed by either a
 * gfxASurface or a GLContext (for WebGL layers), and has some control
 * for intelligent updating from the source if necessary (for example,
 * if hardware compositing is not available, for reading from the GL
 * buffer into an image surface that we can layer composite.)
 *
 * After Initialize is called, the underlying canvas Surface/GLContext
 * must not be modified during a layer transaction.
 */
class CanvasLayer : public Layer {
public:
  struct Data {
    Data()
      : mBufferProvider(nullptr)
      , mGLContext(nullptr)
      , mRenderer(nullptr)
      , mFrontbufferGLTex(0)
      , mSize(0,0)
      , mHasAlpha(false)
      , mIsGLAlphaPremult(true)
      , mIsMirror(false)
    { }

    // One of these three must be specified for Canvas2D, but never more than one
    PersistentBufferProvider* mBufferProvider; // A BufferProvider for the Canvas contents
    mozilla::gl::GLContext* mGLContext; // or this, for GL.
    AsyncCanvasRenderer* mRenderer; // or this, for OffscreenCanvas

    // Frontbuffer override
    uint32_t mFrontbufferGLTex;

    // The size of the canvas content
    gfx::IntSize mSize;

    // Whether the canvas drawingbuffer has an alpha channel.
    bool mHasAlpha;

    // Whether mGLContext contains data that is alpha-premultiplied.
    bool mIsGLAlphaPremult;

    // Whether the canvas front buffer is already being rendered somewhere else.
    // When true, do not swap buffers or Morph() to another factory on mGLContext
    bool mIsMirror;
  };

  /**
   * CONSTRUCTION PHASE ONLY
   * Initialize this CanvasLayer with the given data.  The data must
   * have either mSurface or mGLContext initialized (but not both), as
   * well as mSize.
   *
   * This must only be called once.
   */
  virtual void Initialize(const Data& aData) = 0;

  void SetBounds(gfx::IntRect aBounds) { mBounds = aBounds; }

  /**
   * Check the data is owned by this layer is still valid for rendering
   */
  virtual bool IsDataValid(const Data& aData) { return true; }

  virtual CanvasLayer* AsCanvasLayer() override { return this; }

  /**
   * Notify this CanvasLayer that the canvas surface contents have
   * changed (or will change) before the next transaction.
   */
  void Updated() { mDirty = true; SetInvalidRectToVisibleRegion(); }

  /**
   * Notify this CanvasLayer that the canvas surface contents have
   * been painted since the last change.
   */
  void Painted() { mDirty = false; }

  /**
   * Returns true if the canvas surface contents have changed since the
   * last paint.
   */
  bool IsDirty()
  {
    // We can only tell if we are dirty if we're part of the
    // widget's retained layer tree.
    if (!mManager || !mManager->IsWidgetLayerManager()) {
      return true;
    }
    return mDirty;
  }

  /**
   * Register a callback to be called at the start of each transaction.
   */
  typedef void PreTransactionCallback(void* closureData);
  void SetPreTransactionCallback(PreTransactionCallback* callback, void* closureData)
  {
    mPreTransCallback = callback;
    mPreTransCallbackData = closureData;
  }

  const nsIntRect& GetBounds() const { return mBounds; }

protected:
  void FirePreTransactionCallback()
  {
    if (mPreTransCallback) {
      mPreTransCallback(mPreTransCallbackData);
    }
  }

public:
  /**
   * Register a callback to be called at the end of each transaction.
   */
  typedef void (* DidTransactionCallback)(void* aClosureData);
  void SetDidTransactionCallback(DidTransactionCallback aCallback, void* aClosureData)
  {
    mPostTransCallback = aCallback;
    mPostTransCallbackData = aClosureData;
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the filter used to resample this image (if necessary).
   */
  void SetSamplingFilter(gfx::SamplingFilter aSamplingFilter)
  {
    if (mSamplingFilter != aSamplingFilter) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Filter", this));
      mSamplingFilter = aSamplingFilter;
      Mutated();
    }
  }
  gfx::SamplingFilter GetSamplingFilter() const { return mSamplingFilter; }

  MOZ_LAYER_DECL_NAME("CanvasLayer", TYPE_CANVAS)

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override
  {
    // Snap our local transform first, and snap the inherited transform as well.
    // This makes our snapping equivalent to what would happen if our content
    // was drawn into a PaintedLayer (gfxContext would snap using the local
    // transform, then we'd snap again when compositing the PaintedLayer).
    mEffectiveTransform =
        SnapTransform(GetLocalTransform(), gfxRect(0, 0, mBounds.width, mBounds.height),
                      nullptr)*
        SnapTransformTranslation(aTransformToSurface, nullptr);
    ComputeEffectiveTransformForMaskLayers(aTransformToSurface);
  }

  bool GetIsAsyncRenderer() const
  {
    return !!mAsyncRenderer;
  }

protected:
  CanvasLayer(LayerManager* aManager, void* aImplData);
  virtual ~CanvasLayer();

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent) override;

  void FireDidTransactionCallback()
  {
    if (mPostTransCallback) {
      mPostTransCallback(mPostTransCallbackData);
    }
  }

  /**
   * 0, 0, canvaswidth, canvasheight
   */
  gfx::IntRect mBounds;
  PreTransactionCallback* mPreTransCallback;
  void* mPreTransCallbackData;
  DidTransactionCallback mPostTransCallback;
  void* mPostTransCallbackData;
  gfx::SamplingFilter mSamplingFilter;
  RefPtr<AsyncCanvasRenderer> mAsyncRenderer;

private:
  /**
   * Set to true in Updated(), cleared during a transaction.
   */
  bool mDirty;
};

/**
 * ContainerLayer that refers to a "foreign" layer tree, through an
 * ID.  Usage of RefLayer looks like
 *
 * Construction phase:
 *   allocate ID for layer subtree
 *   create RefLayer, SetReferentId(ID)
 *
 * Composition:
 *   look up subtree for GetReferentId()
 *   ConnectReferentLayer(subtree)
 *   compose
 *   ClearReferentLayer()
 *
 * Clients will usually want to Connect/Clear() on each transaction to
 * avoid difficulties managing memory across multiple layer subtrees.
 */
class RefLayer : public ContainerLayer {
  friend class LayerManager;

private:
  virtual bool InsertAfter(Layer* aChild, Layer* aAfter) override
  { MOZ_CRASH("GFX: RefLayer"); return false; }

  virtual bool RemoveChild(Layer* aChild) override
  { MOZ_CRASH("GFX: RefLayer"); return false; }

  virtual bool RepositionChild(Layer* aChild, Layer* aAfter) override
  { MOZ_CRASH("GFX: RefLayer"); return false; }

public:
  /**
   * CONSTRUCTION PHASE ONLY
   * Set the ID of the layer's referent.
   */
  void SetReferentId(uint64_t aId)
  {
    MOZ_ASSERT(aId != 0);
    if (mId != aId) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ReferentId", this));
      mId = aId;
      Mutated();
    }
  }
  /**
   * CONSTRUCTION PHASE ONLY
   * Connect this ref layer to its referent, temporarily.
   * ClearReferentLayer() must be called after composition.
   */
  void ConnectReferentLayer(Layer* aLayer)
  {
    MOZ_ASSERT(!mFirstChild && !mLastChild);
    MOZ_ASSERT(!aLayer->GetParent());
    if (aLayer->Manager() != Manager()) {
      // This can happen when e.g. rendering while dragging tabs
      // between windows - aLayer's manager may be the manager for the
      // old window's tab.  In that case, it will be changed before the
      // next render (see SetLayerManager).  It is simply easier to
      // ignore the rendering here than it is to pause it.
      NS_WARNING("ConnectReferentLayer failed - Incorrect LayerManager");
      return;
    }

    mFirstChild = mLastChild = aLayer;
    aLayer->SetParent(this);
  }

  /**
   * DRAWING PHASE ONLY
   * |aLayer| is the same as the argument to ConnectReferentLayer().
   */
  void DetachReferentLayer(Layer* aLayer)
  {
    mFirstChild = mLastChild = nullptr;
    aLayer->SetParent(nullptr);
  }

  // These getters can be used anytime.
  virtual RefLayer* AsRefLayer() override { return this; }

  virtual uint64_t GetReferentId() { return mId; }

  /**
   * DRAWING PHASE ONLY
   */
  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs) override;

  MOZ_LAYER_DECL_NAME("RefLayer", TYPE_REF)

protected:
  RefLayer(LayerManager* aManager, void* aImplData)
    : ContainerLayer(aManager, aImplData) , mId(0)
  {}

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent) override;

  // 0 is a special value that means "no ID".
  uint64_t mId;
};

void SetAntialiasingFlags(Layer* aLayer, gfx::DrawTarget* aTarget);

#ifdef MOZ_DUMP_PAINTING
void WriteSnapshotToDumpFile(Layer* aLayer, gfx::DataSourceSurface* aSurf);
void WriteSnapshotToDumpFile(LayerManager* aManager, gfx::DataSourceSurface* aSurf);
void WriteSnapshotToDumpFile(Compositor* aCompositor, gfx::DrawTarget* aTarget);
#endif

// A utility function used by different LayerManager implementations.
gfx::IntRect ToOutsideIntRect(const gfxRect &aRect);

} // namespace layers
} // namespace mozilla

#endif /* GFX_LAYERS_H */
