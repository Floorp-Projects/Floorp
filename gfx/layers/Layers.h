/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERS_H
#define GFX_LAYERS_H

#include <stdint.h>                     // for uint32_t, uint64_t, uint8_t
#include <stdio.h>                      // for FILE
#include <sys/types.h>                  // for int32_t, int64_t
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for LayerMargin, LayerPoint
#include "gfxContext.h"                 // for GraphicsOperator
#include "gfxTypes.h"
#include "gfxColor.h"                   // for gfxRGBA
#include "GraphicsFilter.h"             // for GraphicsFilter
#include "gfxPoint.h"                   // for gfxPoint
#include "gfxRect.h"                    // for gfxRect
#include "gfx2DGlue.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2, etc
#include "mozilla/DebugOnly.h"          // for DebugOnly
#include "mozilla/EventForwards.h"      // for nsPaintEvent
#include "mozilla/RefPtr.h"             // for TemporaryRef
#include "mozilla/StyleAnimationValue.h" // for StyleAnimationValue, etc
#include "mozilla/TimeStamp.h"          // for TimeStamp, TimeDuration
#include "mozilla/gfx/BaseMargin.h"     // for BaseMargin
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "mozilla/gfx/UserData.h"       // for UserData, etc
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsAutoPtr, nsRefPtr, etc
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsCSSProperty.h"              // for nsCSSProperty
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for Layer::Release, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsSize.h"                     // for nsIntSize
#include "nsString.h"                   // for nsCString
#include "nsTArray.h"                   // for nsTArray
#include "nsTArrayForwardDeclare.h"     // for InfallibleTArray
#include "nscore.h"                     // for nsACString, nsAString
#include "prlog.h"                      // for PRLogModuleInfo
#include "gfx2DGlue.h"

class gfxContext;

extern uint8_t gLayerManagerLayerBuilder;

namespace mozilla {

class ComputedTimingFunction;
class FrameLayerBuilder;
class StyleAnimationValue;
class WebGLContext;

namespace gl {
class GLContext;
class SurfaceStream;
}

namespace gfx {
class DrawTarget;
}

namespace dom {
class OverfillCallback;
}

namespace layers {

class Animation;
class AnimationData;
class AsyncPanZoomController;
class ClientLayerManager;
class CommonLayerAttributes;
class Layer;
class ThebesLayer;
class ContainerLayer;
class ImageLayer;
class ColorLayer;
class ImageContainer;
class CanvasLayer;
class ReadbackLayer;
class ReadbackProcessor;
class RefLayer;
class LayerComposite;
class ShadowableLayer;
class ShadowLayerForwarder;
class LayerManagerComposite;
class SpecificLayerAttributes;
class SurfaceDescriptor;
class Compositor;
struct TextureFactoryIdentifier;
struct EffectMask;

namespace layerscope {
class LayersPacket;
}

#define MOZ_LAYER_DECL_NAME(n, e)                           \
  virtual const char* Name() const { return n; }            \
  virtual LayerType GetType() const { return e; }

/**
 * Base class for userdata objects attached to layers and layer managers.
 */
class LayerUserData {
public:
  virtual ~LayerUserData() {}
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

static void LayerManagerUserDataDestroy(void *data)
{
  delete static_cast<LayerUserData*>(data);
}

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
 * the Construction phase. When the client has finished constructing the layer
 * tree, it should call EndConstruction() to enter the drawing phase.
 * 2) Drawing: ThebesLayers are rendered into in this phase, in tree
 * order. When the client has finished drawing into the ThebesLayers, it should
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
  {
    InitLog();
  }

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

  virtual LayerManagerComposite* AsLayerManagerComposite()
  { return nullptr; }

  virtual ClientLayerManager* AsClientLayerManager()
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
  virtual void BeginTransaction() = 0;
  /**
   * Start a new transaction. Nested transactions are not allowed so
   * there must be no transaction currently in progress.
   * This transaction will render the contents of the layer tree to
   * the given target context. The rendering will be complete when
   * EndTransaction returns.
   */
  virtual void BeginTransactionWithTarget(gfxContext* aTarget) = 0;

  enum EndTransactionFlags {
    END_DEFAULT = 0,
    END_NO_IMMEDIATE_REDRAW = 1 << 0,  // Do not perform the drawing phase
    END_NO_COMPOSITE = 1 << 1, // Do not composite after drawing thebes layer contents.
    END_NO_REMOTE_COMPOSITE = 1 << 2 // Do not schedule a composition with a remote Compositor, if one exists.
  };

  FrameLayerBuilder* GetLayerBuilder() {
    return reinterpret_cast<FrameLayerBuilder*>(GetUserData(&gLayerManagerLayerBuilder));
  }

  /**
   * Attempts to end an "empty transaction". There must have been no
   * changes to the layer tree since the BeginTransaction().
   * It's possible for this to fail; ThebesLayers may need to be updated
   * due to VRAM data being lost, for example. In such cases this method
   * returns false, and the caller must proceed with a normal layer tree
   * update and EndTransaction.
   */
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) = 0;

  /**
   * Function called to draw the contents of each ThebesLayer.
   * aRegionToDraw contains the region that needs to be drawn.
   * This would normally be a subregion of the visible region.
   * The callee must draw all of aRegionToDraw. Drawing outside
   * aRegionToDraw will be clipped out or ignored.
   * The callee must draw all of aRegionToDraw.
   * This region is relative to 0,0 in the ThebesLayer.
   *
   * aRegionToInvalidate contains a region whose contents have been
   * changed by the layer manager and which must therefore be invalidated.
   * For example, this could be non-empty if a retained layer internally
   * switches from RGBA to RGB or back ... we might want to repaint it to
   * consistently use subpixel-AA or not.
   * This region is relative to 0,0 in the ThebesLayer.
   * aRegionToInvalidate may contain areas that are outside
   * aRegionToDraw; the callee must ensure that these areas are repainted
   * in the current layer manager transaction or in a later layer
   * manager transaction.
   *
   * aContext must not be used after the call has returned.
   * We guarantee that buffered contents in the visible
   * region are valid once drawing is complete.
   *
   * The origin of aContext is 0,0 in the ThebesLayer.
   */
  typedef void (* DrawThebesLayerCallback)(ThebesLayer* aLayer,
                                           gfxContext* aContext,
                                           const nsIntRegion& aRegionToDraw,
                                           DrawRegionClip aClip,
                                           const nsIntRegion& aRegionToInvalidate,
                                           void* aCallbackData);

  /**
   * Finish the construction phase of the transaction, perform the
   * drawing phase, and end the transaction.
   * During the drawing phase, all ThebesLayers in the tree are
   * drawn in tree order, exactly once each, except for those layers
   * where it is known that the visible region is empty.
   */
  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) = 0;

  /**
   * Schedule a composition with the remote Compositor, if one exists
   * for this LayerManager. Useful in conjunction with the END_NO_REMOTE_COMPOSITE
   * flag to EndTransaction.
   */
  virtual void Composite() {}

  virtual bool HasShadowManagerInternal() const { return false; }
  bool HasShadowManager() const { return HasShadowManagerInternal(); }

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
   * scrollable layer.
   * Can be called any time.
   */
  Layer* GetPrimaryScrollableLayer();

  /**
   * Returns a list of all descendant layers for which
   * GetFrameMetrics().IsScrollable() is true.
   */
  void GetScrollableLayers(nsTArray<Layer*>& aArray);

  /**
   * CONSTRUCTION PHASE ONLY
   * Called when a managee has mutated.
   * Subclasses overriding this method must first call their
   * superclass's impl
   */
#ifdef DEBUG
  // In debug builds, we check some properties of |aLayer|.
  virtual void Mutated(Layer* aLayer);
#else
  virtual void Mutated(Layer* aLayer) { }
#endif

  /**
   * Hints that can be used during Thebes layer creation to influence the type
   * or properties of the layer created.
   *
   * NONE: No hint.
   * SCROLLABLE: This layer may represent scrollable content.
   */
  enum ThebesLayerCreationHint {
    NONE, SCROLLABLE
  };

  /**
   * Returns true if aLayer is optimized for the given ThebesLayerCreationHint.
   */
  virtual bool IsOptimizedFor(ThebesLayer* aLayer,
                              ThebesLayerCreationHint aCreationHint)
  { return true; }

  /**
   * CONSTRUCTION PHASE ONLY
   * Create a ThebesLayer for this manager's layer tree.
   */
  virtual already_AddRefed<ThebesLayer> CreateThebesLayer() = 0;
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a ThebesLayer for this manager's layer tree, with a creation hint
   * parameter to help optimise the type of layer created.
   */
  virtual already_AddRefed<ThebesLayer> CreateThebesLayerWithHint(ThebesLayerCreationHint) {
    return CreateThebesLayer();
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
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer() { return nullptr; }
  /**
   * CONSTRUCTION PHASE ONLY
   * Create a RefLayer for this manager's layer tree.
   */
  virtual already_AddRefed<RefLayer> CreateRefLayer() { return nullptr; }


  /**
   * Can be called anytime, from any thread.
   *
   * Creates an Image container which forwards its images to the compositor within
   * layer transactions on the main thread.
   */
  static already_AddRefed<ImageContainer> CreateImageContainer();

  /**
   * Can be called anytime, from any thread.
   *
   * Tries to create an Image container which forwards its images to the compositor
   * asynchronously using the ImageBridge IPDL protocol. If the protocol is not
   * available, the returned ImageContainer will forward images within layer
   * transactions, just like if it was created with CreateImageContainer().
   */
  static already_AddRefed<ImageContainer> CreateAsynchronousImageContainer();

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
  virtual TemporaryRef<DrawTarget>
    CreateOptimalDrawTarget(const IntSize &aSize,
                            SurfaceFormat imageFormat);

  /**
   * Creates a DrawTarget for alpha masks which is optimized for inter-
   * operating with this layer manager. In contrast to CreateOptimalDrawTarget,
   * this surface is optimised for drawing alpha only and we assume that
   * drawing the mask is fairly simple.
   */
  virtual TemporaryRef<DrawTarget>
    CreateOptimalMaskDrawTarget(const IntSize &aSize);

  /**
   * Creates a DrawTarget for use with canvas which is optimized for
   * inter-operating with this layermanager.
   */
  virtual TemporaryRef<mozilla::gfx::DrawTarget>
    CreateDrawTarget(const mozilla::gfx::IntSize &aSize,
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
    mUserData.Add(static_cast<gfx::UserDataKey*>(aKey), aData, LayerManagerUserDataDestroy);
  }
  /**
   * This can be used anytime. Ownership passes to the caller!
   */
  nsAutoPtr<LayerUserData> RemoveUserData(void* aKey)
  {
    nsAutoPtr<LayerUserData> d(static_cast<LayerUserData*>(mUserData.Remove(static_cast<gfx::UserDataKey*>(aKey))));
    return d;
  }
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
   * Checks if we need to invalidate the OS widget to trigger
   * painting when updating this layer manager.
   */
  virtual bool NeedsWidgetInvalidation() { return true; }

  virtual const char* Name() const { return "???"; }

  /**
   * Dump information about this layer manager and its managed tree to
   * aStream.
   */
  void Dump(std::stringstream& aStream, const char* aPrefix="", bool aDumpHtml=false);
  /**
   * Dump information about just this layer manager itself to aStream
   */
  void DumpSelf(std::stringstream& aStream, const char* aPrefix="");
  void Dump();

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
  void PostPresent();

  void BeginTabSwitch();

  static bool IsLogEnabled();
  static PRLogModuleInfo* GetLog() { return sLog; }

  bool IsCompositingCheap(LayersBackend aBackend)
  {
    // LayersBackend::LAYERS_NONE is an error state, but in that case we should try to
    // avoid loading the compositor!
    return LayersBackend::LAYERS_BASIC != aBackend && LayersBackend::LAYERS_NONE != aBackend;
  }

  virtual bool IsCompositingCheap() { return true; }

  bool IsInTransaction() const { return mInTransaction; }
  virtual bool RequestOverfill(mozilla::dom::OverfillCallback* aCallback) { return true; }
  virtual void RunOverfillCallback(const uint32_t aOverfill) { }

  virtual void SetRegionToClear(const nsIntRegion& aRegion)
  {
    mRegionToClear = aRegion;
  }

  virtual bool SupportsMixBlendModes(EnumSet<gfx::CompositionOp>& aMixBlendModes)
  {
    return false;
  }

  bool SupportsMixBlendMode(gfx::CompositionOp aMixBlendMode)
  {
    EnumSet<gfx::CompositionOp> modes(aMixBlendMode);
    return SupportsMixBlendModes(modes);
  }

protected:
  nsRefPtr<Layer> mRoot;
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

  static void InitLog();
  static PRLogModuleInfo* sLog;
  uint64_t mId;
  bool mInTransaction;
private:
  struct FramesTimingRecording
  {
    // Stores state and data for frame intervals and paint times recording.
    // see LayerManager::StartFrameTimeRecording() at Layers.cpp for more details.
    FramesTimingRecording()
      : mIsPaused(true)
      , mNextIndex(0)
    {}
    bool mIsPaused;
    uint32_t mNextIndex;
    TimeStamp mLastFrameTime;
    nsTArray<float> mIntervals;
    uint32_t mLatestStartIndex;
    uint32_t mCurrentRunStartIndex;
  };
  FramesTimingRecording mRecording;

  TimeStamp mTabSwitchStart;
};

typedef InfallibleTArray<Animation> AnimationArray;

struct AnimData {
  InfallibleTArray<mozilla::StyleAnimationValue> mStartValues;
  InfallibleTArray<mozilla::StyleAnimationValue> mEndValues;
  InfallibleTArray<nsAutoPtr<mozilla::ComputedTimingFunction> > mFunctions;
};

/**
 * A Layer represents anything that can be rendered onto a destination
 * surface.
 */
class Layer {
  NS_INLINE_DECL_REFCOUNTING(Layer)

public:
  // Keep these in alphabetical order
  enum LayerType {
    TYPE_CANVAS,
    TYPE_COLOR,
    TYPE_CONTAINER,
    TYPE_IMAGE,
    TYPE_READBACK,
    TYPE_REF,
    TYPE_SHADOW,
    TYPE_THEBES
  };

  /**
   * Returns the LayerManager this Layer belongs to. Note that the layer
   * manager might be in a destroyed state, at which point it's only
   * valid to set/get user data from it.
   */
  LayerManager* Manager() { return mManager; }

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
    CONTENT_PRESERVE_3D = 0x08,
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
    CONTENT_DISABLE_SUBPIXEL_AA = 0x20
  };
  /**
   * CONSTRUCTION PHASE ONLY
   * This lets layout make some promises about what will be drawn into the
   * visible region of the ThebesLayer. This enables internal quality
   * and performance optimizations.
   */
  void SetContentFlags(uint32_t aFlags)
  {
    NS_ASSERTION((aFlags & (CONTENT_OPAQUE | CONTENT_COMPONENT_ALPHA)) !=
                 (CONTENT_OPAQUE | CONTENT_COMPONENT_ALPHA),
                 "Can't be opaque and require component alpha");
    if (mContentFlags != aFlags) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ContentFlags", this));
      mContentFlags = aFlags;
      Mutated();
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
  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    if (!mVisibleRegion.IsEqual(aRegion)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) VisibleRegion was %s is %s", this,
        mVisibleRegion.ToString().get(), aRegion.ToString().get()));
      mVisibleRegion = aRegion;
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the (sub)document metrics used to render the Layer subtree
   * rooted at this.
   */
  void SetFrameMetrics(const FrameMetrics& aFrameMetrics)
  {
    if (mFrameMetrics != aFrameMetrics) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) FrameMetrics", this));
      mFrameMetrics = aFrameMetrics;
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the ViewID of the ContainerLayer to which overscroll should be handed
   * off. A value of NULL_SCROLL_ID means that the default handoff-parent-finding
   * behaviour should be used (i.e. walk up the layer tree to find the next
   * scrollable ancestor layer).
   */
  void SetScrollHandoffParentId(FrameMetrics::ViewID aScrollParentId)
  {
    if (mScrollHandoffParentId != aScrollParentId) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ScrollHandoffParentId", this));
      mScrollHandoffParentId = aScrollParentId;
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
    if (mOpacity != aOpacity) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Opacity", this));
      mOpacity = aOpacity;
      Mutated();
    }
  }

  void SetMixBlendMode(gfx::CompositionOp aMixBlendMode)
  {
    if (mMixBlendMode != aMixBlendMode) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) MixBlendMode", this));
      mMixBlendMode = aMixBlendMode;
      Mutated();
    }
  }

  void DeprecatedSetMixBlendMode(gfxContext::GraphicsOperator aMixBlendMode)
  {
    SetMixBlendMode(gfx::CompositionOpForOp(aMixBlendMode));
  }

  void SetForceIsolatedGroup(bool aForceIsolatedGroup)
  {
    if(mForceIsolatedGroup != aForceIsolatedGroup) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ForceIsolatedGroup", this));
      mForceIsolatedGroup = aForceIsolatedGroup;
      Mutated();
    }
  }

  bool GetForceIsolatedGroup() const
  {
    return mForceIsolatedGroup;
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
  void SetClipRect(const nsIntRect* aRect)
  {
    if (mUseClipRect) {
      if (!aRect) {
        MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ClipRect was %d,%d,%d,%d is <none>", this,
                         mClipRect.x, mClipRect.y, mClipRect.width, mClipRect.height));
        mUseClipRect = false;
        Mutated();
      } else {
        if (!aRect->IsEqualEdges(mClipRect)) {
          MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ClipRect was %d,%d,%d,%d is %d,%d,%d,%d", this,
                           mClipRect.x, mClipRect.y, mClipRect.width, mClipRect.height,
                           aRect->x, aRect->y, aRect->width, aRect->height));
          mClipRect = *aRect;
          Mutated();
        }
      }
    } else {
      if (aRect) {
        MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ClipRect was <none> is %d,%d,%d,%d", this,
                         aRect->x, aRect->y, aRect->width, aRect->height));
        mUseClipRect = true;
        mClipRect = *aRect;
        Mutated();
      }
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
   * Tell this layer what its transform should be. The transformation
   * is applied when compositing the layer into its parent container.
   */
  void SetBaseTransform(const gfx::Matrix4x4& aMatrix)
  {
    NS_ASSERTION(!aMatrix.IsSingular(),
                 "Shouldn't be trying to draw with a singular matrix!");
    mPendingTransform = nullptr;
    if (mTransform == aMatrix) {
      return;
    }
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) BaseTransform", this));
    mTransform = aMatrix;
    Mutated();
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
    if (mPostXScale == aXScale && mPostYScale == aYScale) {
      return;
    }
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) PostScale", this));
    mPostXScale = aXScale;
    mPostYScale = aYScale;
    Mutated();
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * A layer is "fixed position" when it draws content from a content
   * (not chrome) document, the topmost content document has a root scrollframe
   * with a displayport, but the layer does not move when that displayport scrolls.
   */
  void SetIsFixedPosition(bool aFixedPosition)
  {
    if (mIsFixedPosition != aFixedPosition) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) IsFixedPosition", this));
      mIsFixedPosition = aFixedPosition;
      Mutated();
    }
  }

  // Call AddAnimation to add a new animation to this layer from layout code.
  // Caller must fill in all the properties of the returned animation.
  Animation* AddAnimation();
  // ClearAnimations clears animations on this layer.
  void ClearAnimations();
  // This is only called when the layer tree is updated. Do not call this from
  // layout code.  To add an animation to this layer, use AddAnimation.
  void SetAnimations(const AnimationArray& aAnimations);

  // These are a parallel to AddAnimation and clearAnimations, except
  // they add pending animations that apply only when the next
  // transaction is begun.  (See also
  // SetBaseTransformForNextTransaction.)
  Animation* AddAnimationForNextTransaction();
  void ClearAnimationsForNextTransaction();

  /**
   * CONSTRUCTION PHASE ONLY
   * If a layer is "fixed position", this determines which point on the layer
   * is considered the "anchor" point, that is, the point which remains in the
   * same position when compositing the layer tree with a transformation
   * (such as when asynchronously scrolling and zooming).
   */
  void SetFixedPositionAnchor(const LayerPoint& aAnchor)
  {
    if (mAnchor != aAnchor) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) FixedPositionAnchor", this));
      mAnchor = aAnchor;
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * If a layer represents a fixed position element or elements that are on
   * a document that has had fixed position element margins set on it, these
   * will be mirrored here. This allows for asynchronous animation of the
   * margins by reconciling the difference between this value and a value that
   * is updated more frequently.
   * If the left or top margins are negative, it means that the elements this
   * layer represents are auto-positioned, and so fixed position margins should
   * not have an effect on the corresponding axis.
   */
  void SetFixedPositionMargins(const LayerMargin& aMargins)
  {
    if (mMargins != aMargins) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) FixedPositionMargins", this));
      mMargins = aMargins;
      Mutated();
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
    if (!mStickyPositionData ||
        !mStickyPositionData->mOuter.IsEqualEdges(aOuter) ||
        !mStickyPositionData->mInner.IsEqualEdges(aInner)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) StickyPositionData", this));
      if (!mStickyPositionData) {
        mStickyPositionData = new StickyPositionData;
      }
      mStickyPositionData->mScrollId = aScrollId;
      mStickyPositionData->mOuter = aOuter;
      mStickyPositionData->mInner = aInner;
      Mutated();
    }
  }

  enum ScrollDirection {
    NONE,
    VERTICAL,
    HORIZONTAL
  };

  /**
   * CONSTRUCTION PHASE ONLY
   * If a layer is a scrollbar layer, |aScrollId| holds the scroll identifier
   * of the scrollable content that the scrollbar is for.
   */
  void SetScrollbarData(FrameMetrics::ViewID aScrollId, ScrollDirection aDir)
  {
    if (mScrollbarTargetId != aScrollId ||
        mScrollbarDirection != aDir) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ScrollbarData", this));
      mScrollbarTargetId = aScrollId;
      mScrollbarDirection = aDir;
      Mutated();
    }
  }

  void SetBackgroundColor(const gfxRGBA& aColor)
  {
    if (mBackgroundColor == aColor) {
      return;
    }

    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) BackgroundColor", this));
    mBackgroundColor = aColor;
    Mutated();
  }

  void SetContentDescription(const std::string& aContentDescription)
  {
    if (mContentDescription == aContentDescription) {
      return;
    }

    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ContentDescription", this));
    mContentDescription = aContentDescription;
    Mutated();
  }

  // These getters can be used anytime.
  float GetOpacity() { return mOpacity; }
  gfx::CompositionOp GetMixBlendMode() const { return mMixBlendMode; }
  const nsIntRect* GetClipRect() { return mUseClipRect ? &mClipRect : nullptr; }
  uint32_t GetContentFlags() { return mContentFlags; }
  const nsIntRegion& GetVisibleRegion() { return mVisibleRegion; }
  const FrameMetrics& GetFrameMetrics() const { return mFrameMetrics; }
  FrameMetrics::ViewID GetScrollHandoffParentId() const { return mScrollHandoffParentId; }
  const EventRegions& GetEventRegions() const { return mEventRegions; }
  ContainerLayer* GetParent() { return mParent; }
  Layer* GetNextSibling() { return mNextSibling; }
  const Layer* GetNextSibling() const { return mNextSibling; }
  Layer* GetPrevSibling() { return mPrevSibling; }
  const Layer* GetPrevSibling() const { return mPrevSibling; }
  virtual Layer* GetFirstChild() const { return nullptr; }
  virtual Layer* GetLastChild() const { return nullptr; }
  const gfx::Matrix4x4 GetTransform() const;
  const gfx::Matrix4x4& GetBaseTransform() const { return mTransform; }
  float GetPostXScale() const { return mPostXScale; }
  float GetPostYScale() const { return mPostYScale; }
  bool GetIsFixedPosition() { return mIsFixedPosition; }
  bool GetIsStickyPosition() { return mStickyPositionData; }
  LayerPoint GetFixedPositionAnchor() { return mAnchor; }
  const LayerMargin& GetFixedPositionMargins() { return mMargins; }
  FrameMetrics::ViewID GetStickyScrollContainerId() { return mStickyPositionData->mScrollId; }
  const LayerRect& GetStickyScrollRangeOuter() { return mStickyPositionData->mOuter; }
  const LayerRect& GetStickyScrollRangeInner() { return mStickyPositionData->mInner; }
  FrameMetrics::ViewID GetScrollbarTargetContainerId() { return mScrollbarTargetId; }
  ScrollDirection GetScrollbarDirection() { return mScrollbarDirection; }
  Layer* GetMaskLayer() const { return mMaskLayer; }
  gfxRGBA GetBackgroundColor() const { return mBackgroundColor; }
  const std::string& GetContentDescription() const { return mContentDescription; }


  // Note that all lengths in animation data are either in CSS pixels or app
  // units and must be converted to device pixels by the compositor.
  AnimationArray& GetAnimations() { return mAnimations; }
  InfallibleTArray<AnimData>& GetAnimationData() { return mAnimationData; }

  uint64_t GetAnimationGeneration() { return mAnimationGeneration; }
  void SetAnimationGeneration(uint64_t aCount) { mAnimationGeneration = aCount; }

  /**
   * Returns the local transform for this layer: either mTransform or,
   * for shadow layers, GetShadowTransform()
   */
  const gfx::Matrix4x4 GetLocalTransform();

  /**
   * Returns the local opacity for this layer: either mOpacity or,
   * for shadow layers, GetShadowOpacity()
   */
  const float GetLocalOpacity();

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
    if (mContentFlags & CONTENT_COMPONENT_ALPHA)
      return SurfaceMode::SURFACE_COMPONENT_ALPHA;
    return SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
  }

  /**
   * This setter can be used anytime. The user data for all keys is
   * initially null. Ownership pases to the layer manager.
   */
  void SetUserData(void* aKey, LayerUserData* aData)
  {
    mUserData.Add(static_cast<gfx::UserDataKey*>(aKey), aData, LayerManagerUserDataDestroy);
  }
  /**
   * This can be used anytime. Ownership passes to the caller!
   */
  nsAutoPtr<LayerUserData> RemoveUserData(void* aKey)
  {
    nsAutoPtr<LayerUserData> d(static_cast<LayerUserData*>(mUserData.Remove(static_cast<gfx::UserDataKey*>(aKey))));
    return d;
  }
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
   * Dynamic downcast to a Thebes layer. Returns null if this is not
   * a ThebesLayer.
   */
  virtual ThebesLayer* AsThebesLayer() { return nullptr; }

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
   * Dynamic cast to a LayerComposite.  Return null if this is not a
   * LayerComposite.  Can be used anytime.
   */
  virtual LayerComposite* AsLayerComposite() { return nullptr; }

  /**
   * Dynamic cast to a ShadowableLayer.  Return null if this is not a
   * ShadowableLayer.  Can be used anytime.
   */
  virtual ShadowableLayer* AsShadowableLayer() { return nullptr; }

  // These getters can be used anytime.  They return the effective
  // values that should be used when drawing this layer to screen,
  // accounting for this layer possibly being a shadow.
  const nsIntRect* GetEffectiveClipRect();
  const nsIntRegion& GetEffectiveVisibleRegion();

  /**
   * Returns the product of the opacities of this layer and all ancestors up
   * to and excluding the nearest ancestor that has UseIntermediateSurface() set.
   */
  float GetEffectiveOpacity();

  /**
   * Returns the blendmode of this layer.
   */
  gfx::CompositionOp GetEffectiveMixBlendMode();
  gfxContext::GraphicsOperator DeprecatedGetEffectiveMixBlendMode();

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
   * computes the effective transform for a mask layer, if this layer has one
   */
  void ComputeEffectiveTransformForMaskLayer(const gfx::Matrix4x4& aTransformToSurface);

  /**
   * Calculate the scissor rect required when rendering this layer.
   * Returns a rectangle relative to the intermediate surface belonging to the
   * nearest ancestor that has an intermediate surface, or relative to the root
   * viewport if no ancestor has an intermediate surface, corresponding to the
   * clip rect for this layer intersected with aCurrentScissorRect.
   * If no ancestor has an intermediate surface, the clip rect is transformed
   * by aWorldTransform before being combined with aCurrentScissorRect, if
   * aWorldTransform is non-null.
   */
  nsIntRect CalculateScissorRect(const nsIntRect& aCurrentScissorRect,
                                 const gfx::Matrix* aWorldTransform);

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
  void Dump(std::stringstream& aStream, const char* aPrefix="", bool aDumpHtml=false);
  /**
   * Dump information about just this layer manager itself to aStream.
   */
  void DumpSelf(std::stringstream& aStream, const char* aPrefix="");

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

  static bool IsLogEnabled() { return LayerManager::IsLogEnabled(); }

  /**
   * Returns the current area of the layer (in layer-space coordinates)
   * marked as needed to be recomposited.
   */
  const nsIntRegion& GetInvalidRegion() { return mInvalidRegion; }
  const void SetInvalidRegion(const nsIntRegion& aRect) { mInvalidRegion = aRect; }

  /**
   * Mark the entirety of the layer's visible region as being invalid.
   */
  void SetInvalidRectToVisibleRegion() { mInvalidRegion = GetVisibleRegion(); }

  /**
   * Adds to the current invalid rect.
   */
  void AddInvalidRect(const nsIntRect& aRect) { mInvalidRegion.Or(mInvalidRegion, aRect); }

  /**
   * Clear the invalid rect, marking the layer as being identical to what is currently
   * composited.
   */
  void ClearInvalidRect() { mInvalidRegion.SetEmpty(); }

  // These functions allow attaching an AsyncPanZoomController to this layer,
  // and can be used anytime.
  // A layer has an APZC only-if GetFrameMetrics().IsScrollable()
  void SetAsyncPanZoomController(AsyncPanZoomController *controller);
  AsyncPanZoomController* GetAsyncPanZoomController() const;

  void ApplyPendingUpdatesForThisTransaction();

#ifdef DEBUG
  void SetDebugColorIndex(uint32_t aIndex) { mDebugColorIndex = aIndex; }
  uint32_t GetDebugColorIndex() { return mDebugColorIndex; }
#endif

  virtual LayerRenderState GetRenderState() { return LayerRenderState(); }


  void Mutated()
  {
    mManager->Mutated(this);
  }

  virtual int32_t GetMaxLayerSize() { return Manager()->GetMaxTextureSize(); }

  /**
   * Returns true if this layer's effective transform is not just
   * a translation by integers, or if this layer or some ancestor layer
   * is marked as having a transform that may change without a full layer
   * transaction.
   */
  bool MayResample();

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
  nsRefPtr<Layer> mMaskLayer;
  gfx::UserData mUserData;
  nsIntRegion mVisibleRegion;
  FrameMetrics mFrameMetrics;
  FrameMetrics::ViewID mScrollHandoffParentId;
  EventRegions mEventRegions;
  gfx::Matrix4x4 mTransform;
  // A mutation of |mTransform| that we've queued to be applied at the
  // end of the next transaction (if nothing else overrides it in the
  // meantime).
  nsAutoPtr<gfx::Matrix4x4> mPendingTransform;
  float mPostXScale;
  float mPostYScale;
  gfx::Matrix4x4 mEffectiveTransform;
  AnimationArray mAnimations;
  // See mPendingTransform above.
  nsAutoPtr<AnimationArray> mPendingAnimations;
  InfallibleTArray<AnimData> mAnimationData;
  float mOpacity;
  gfx::CompositionOp mMixBlendMode;
  bool mForceIsolatedGroup;
  nsIntRect mClipRect;
  nsIntRect mTileSourceRect;
  nsIntRegion mInvalidRegion;
  nsRefPtr<AsyncPanZoomController> mAPZC;
  uint32_t mContentFlags;
  bool mUseClipRect;
  bool mUseTileSourceRect;
  bool mIsFixedPosition;
  LayerPoint mAnchor;
  LayerMargin mMargins;
  struct StickyPositionData {
    FrameMetrics::ViewID mScrollId;
    LayerRect mOuter;
    LayerRect mInner;
  };
  nsAutoPtr<StickyPositionData> mStickyPositionData;
  FrameMetrics::ViewID mScrollbarTargetId;
  ScrollDirection mScrollbarDirection;
  DebugOnly<uint32_t> mDebugColorIndex;
  // If this layer is used for OMTA, then this counter is used to ensure we
  // stay in sync with the animation manager
  uint64_t mAnimationGeneration;
  // This is currently set and used only for scrollable container layers.
  gfxRGBA mBackgroundColor;
  // A description of the content element corresponding to this frame.
  // This is empty unless this is a scrollable ContainerLayer and the
  // apz.printtree pref is turned on.
  std::string mContentDescription;
};

/**
 * A Layer which we can draw into using Thebes. It is a conceptually
 * infinite surface, but each ThebesLayer has an associated "valid region"
 * of contents that it is currently storing, which is finite. ThebesLayer
 * implementations can store content between paints.
 *
 * ThebesLayers are rendered into during the drawing phase of a transaction.
 *
 * Currently the contents of a ThebesLayer are in the device output color
 * space.
 */
class ThebesLayer : public Layer {
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
   * mEffectiveTransform to get the ideal transform for this ThebesLayer.
   * When this is true, ComputeEffectiveTransforms will compute the residual
   * and ensure that the layer is invalidated whenever the residual changes.
   * When it's false, a change in the residual will not trigger invalidation
   * and GetResidualTranslation will return 0,0.
   * So when the residual is to be ignored, set this to false for better
   * performance.
   */
  void SetAllowResidualTranslation(bool aAllow) { mAllowResidualTranslation = aAllow; }

  /**
   * Can be used anytime
   */
  const nsIntRegion& GetValidRegion() const { return mValidRegion; }

  virtual ThebesLayer* AsThebesLayer() { return this; }

  MOZ_LAYER_DECL_NAME("ThebesLayer", TYPE_THEBES)

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface)
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
      NS_ASSERTION(-0.5 <= mResidualTranslation.x && mResidualTranslation.x < 0.5 &&
                   -0.5 <= mResidualTranslation.y && mResidualTranslation.y < 0.5,
                   "Residual translation out of range");
      mValidRegion.SetEmpty();
    }
    ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
  }

  LayerManager::ThebesLayerCreationHint GetCreationHint() const { return mCreationHint; }

  bool UsedForReadback() { return mUsedForReadback; }
  void SetUsedForReadback(bool aUsed) { mUsedForReadback = aUsed; }
  /**
   * Returns the residual translation. Apply this translation when drawing
   * into the ThebesLayer so that when mEffectiveTransform is applied afterwards
   * by layer compositing, the results exactly match the "ideal transform"
   * (the product of the transform of this layer and its ancestors).
   * Returns 0,0 unless SetAllowResidualTranslation(true) has been called.
   * The residual translation components are always in the range [-0.5, 0.5).
   */
  gfxPoint GetResidualTranslation() const { return mResidualTranslation; }

protected:
  ThebesLayer(LayerManager* aManager, void* aImplData,
              LayerManager::ThebesLayerCreationHint aCreationHint = LayerManager::NONE)
    : Layer(aManager, aImplData)
    , mValidRegion()
    , mCreationHint(aCreationHint)
    , mUsedForReadback(false)
    , mAllowResidualTranslation(false)
  {
    mContentFlags = 0; // Clear NO_TEXT, NO_TEXT_OVER_TRANSPARENT
  }

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent);

  /**
   * ComputeEffectiveTransforms snaps the ideal transform to get mEffectiveTransform.
   * mResidualTranslation is the translation that should be applied *before*
   * mEffectiveTransform to get the ideal transform.
   */
  gfxPoint mResidualTranslation;
  nsIntRegion mValidRegion;
  /**
   * The creation hint that was used when constructing this layer.
   */
  const LayerManager::ThebesLayerCreationHint mCreationHint;
  /**
   * Set when this ThebesLayer is participating in readback, i.e. some
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

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs);

  void SortChildrenBy3DZOrder(nsTArray<Layer*>& aArray);

  // These getters can be used anytime.

  virtual ContainerLayer* AsContainerLayer() { return this; }
  virtual const ContainerLayer* AsContainerLayer() const { return this; }

  virtual Layer* GetFirstChild() const { return mFirstChild; }
  virtual Layer* GetLastChild() const { return mLastChild; }
  float GetPreXScale() const { return mPreXScale; }
  float GetPreYScale() const { return mPreYScale; }
  float GetInheritedXScale() const { return mInheritedXScale; }
  float GetInheritedYScale() const { return mInheritedYScale; }

  MOZ_LAYER_DECL_NAME("ContainerLayer", TYPE_CONTAINER)

  /**
   * ContainerLayer backends need to override ComputeEffectiveTransforms
   * since the decision about whether to use a temporary surface for the
   * container is backend-specific. ComputeEffectiveTransforms must also set
   * mUseIntermediateSurface.
   */
  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) = 0;

  /**
   * Call this only after ComputeEffectiveTransforms has been invoked
   * on this layer.
   * Returns true if this will use an intermediate surface. This is largely
   * backend-dependent, but it affects the operation of GetEffectiveOpacity().
   */
  bool UseIntermediateSurface() { return mUseIntermediateSurface; }

  /**
   * Returns the rectangle covered by the intermediate surface,
   * in this layer's coordinate system
   */
  nsIntRect GetIntermediateSurfaceRect()
  {
    NS_ASSERTION(mUseIntermediateSurface, "Must have intermediate surface");
    return mVisibleRegion.GetBounds();
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

protected:
  friend class ReadbackProcessor;

  void DidInsertChild(Layer* aLayer);
  void DidRemoveChild(Layer* aLayer);

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

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent);

  Layer* mFirstChild;
  Layer* mLastChild;
  float mPreXScale;
  float mPreYScale;
  // The resolution scale inherited from the parent layer. This will already
  // be part of mTransform.
  float mInheritedXScale;
  float mInheritedYScale;
  bool mUseIntermediateSurface;
  bool mSupportsComponentAlphaChildren;
  bool mMayHaveReadbackChild;
};

/**
 * A Layer which just renders a solid color in its visible region. It actually
 * can fill any area that contains the visible region, so if you need to
 * restrict the area filled, set a clip region on this layer.
 */
class ColorLayer : public Layer {
public:
  virtual ColorLayer* AsColorLayer() { return this; }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the color of the layer.
   */
  virtual void SetColor(const gfxRGBA& aColor)
  {
    if (mColor != aColor) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Color", this));
      mColor = aColor;
      Mutated();
    }
  }

  void SetBounds(const nsIntRect& aBounds)
  {
    if (!mBounds.IsEqualEdges(aBounds)) {
      mBounds = aBounds;
      Mutated();
    }
  }

  const nsIntRect& GetBounds()
  {
    return mBounds;
  }

  // This getter can be used anytime.
  virtual const gfxRGBA& GetColor() { return mColor; }

  MOZ_LAYER_DECL_NAME("ColorLayer", TYPE_COLOR)

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface)
  {
    gfx::Matrix4x4 idealTransform = GetLocalTransform() * aTransformToSurface;
    mEffectiveTransform = SnapTransformTranslation(idealTransform, nullptr);
    ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
  }

protected:
  ColorLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData),
      mColor(0.0, 0.0, 0.0, 0.0)
  {}

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent);

  nsIntRect mBounds;
  gfxRGBA mColor;
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
      : mDrawTarget(nullptr)
      , mGLContext(nullptr)
      , mStream(nullptr)
      , mTexID(0)
      , mSize(0,0)
      , mHasAlpha(false)
      , mIsGLAlphaPremult(true)
    { }

    // One of these two must be specified for Canvas2D, but never both
    mozilla::gfx::DrawTarget* mDrawTarget; // a DrawTarget for the canvas contents
    mozilla::gl::GLContext* mGLContext; // or this, for GL.

    // Canvas/SkiaGL uses this
    mozilla::gl::SurfaceStream* mStream;

    // ID of the texture backing the canvas layer (defaults to 0)
    uint32_t mTexID;

    // The size of the canvas content
    nsIntSize mSize;

    // Whether the canvas drawingbuffer has an alpha channel.
    bool mHasAlpha;

    // Whether mGLContext contains data that is alpha-premultiplied.
    bool mIsGLAlphaPremult;
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

  /**
   * Check the data is owned by this layer is still valid for rendering
   */
  virtual bool IsDataValid(const Data& aData) { return true; }

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
  void SetFilter(GraphicsFilter aFilter)
  {
    if (mFilter != aFilter) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Filter", this));
      mFilter = aFilter;
      Mutated();
    }
  }
  GraphicsFilter GetFilter() const { return mFilter; }

  MOZ_LAYER_DECL_NAME("CanvasLayer", TYPE_CANVAS)

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface)
  {
    // Snap our local transform first, and snap the inherited transform as well.
    // This makes our snapping equivalent to what would happen if our content
    // was drawn into a ThebesLayer (gfxContext would snap using the local
    // transform, then we'd snap again when compositing the ThebesLayer).
    mEffectiveTransform =
        SnapTransform(GetLocalTransform(), gfxRect(0, 0, mBounds.width, mBounds.height),
                      nullptr)*
        SnapTransformTranslation(aTransformToSurface, nullptr);
    ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
  }

protected:
  CanvasLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData)
    , mPreTransCallback(nullptr)
    , mPreTransCallbackData(nullptr)
    , mPostTransCallback(nullptr)
    , mPostTransCallbackData(nullptr)
    , mFilter(GraphicsFilter::FILTER_GOOD)
    , mDirty(false)
  {}

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent);

  void FireDidTransactionCallback()
  {
    if (mPostTransCallback) {
      mPostTransCallback(mPostTransCallbackData);
    }
  }

  /**
   * 0, 0, canvaswidth, canvasheight
   */
  nsIntRect mBounds;
  PreTransactionCallback* mPreTransCallback;
  void* mPreTransCallbackData;
  DidTransactionCallback mPostTransCallback;
  void* mPostTransCallbackData;
  GraphicsFilter mFilter;

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
  virtual bool InsertAfter(Layer* aChild, Layer* aAfter) MOZ_OVERRIDE
  { MOZ_CRASH(); return false; }

  virtual bool RemoveChild(Layer* aChild)
  { MOZ_CRASH(); return false; }

  virtual bool RepositionChild(Layer* aChild, Layer* aAfter)
  { MOZ_CRASH(); return false; }

  using Layer::SetFrameMetrics;

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
    MOZ_ASSERT(aLayer->Manager() == Manager());

    mFirstChild = mLastChild = aLayer;
    aLayer->SetParent(this);
  }

  /**
   * DRAWING PHASE ONLY
   * |aLayer| is the same as the argument to ConnectReferentLayer().
   */
  void DetachReferentLayer(Layer* aLayer)
  {
    MOZ_ASSERT(aLayer == mFirstChild && mFirstChild == mLastChild);
    MOZ_ASSERT(aLayer->GetParent() == this);

    mFirstChild = mLastChild = nullptr;
    aLayer->SetParent(nullptr);
  }

  // These getters can be used anytime.
  virtual RefLayer* AsRefLayer() { return this; }

  virtual int64_t GetReferentId() { return mId; }

  /**
   * DRAWING PHASE ONLY
   */
  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs);

  MOZ_LAYER_DECL_NAME("RefLayer", TYPE_REF)

protected:
  RefLayer(LayerManager* aManager, void* aImplData)
    : ContainerLayer(aManager, aImplData) , mId(0)
  {}

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  virtual void DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent);

  Layer* mTempReferent;
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
nsIntRect ToOutsideIntRect(const gfxRect &aRect);

}
}

#endif /* GFX_LAYERS_H */
