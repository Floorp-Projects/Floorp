/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERS_H
#define GFX_LAYERS_H

#include "mozilla/DebugOnly.h"

#include "gfxTypes.h"
#include "gfxASurface.h"
#include "nsRegion.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"
#include "gfx3DMatrix.h"
#include "gfxColor.h"
#include "gfxPattern.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsStyleAnimation.h"
#include "LayersTypes.h"
#include "FrameMetrics.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/TimeStamp.h"

#if defined(DEBUG) || defined(PR_LOGGING)
#  include <stdio.h>            // FILE
#  include "prlog.h"
#  ifndef MOZ_LAYERS_HAVE_LOG
#    define MOZ_LAYERS_HAVE_LOG
#  endif
#  define MOZ_LAYERS_LOG(_args)                             \
  PR_LOG(LayerManager::GetLog(), PR_LOG_DEBUG, _args)
#else
struct PRLogModuleInfo;
#  define MOZ_LAYERS_LOG(_args)
#endif  // if defined(DEBUG) || defined(PR_LOGGING)

class gfxContext;
class nsPaintEvent;

extern uint8_t gLayerManagerLayerBuilder;

namespace mozilla {

class FrameLayerBuilder;

namespace gl {
class GLContext;
}

namespace css {
class ComputedTimingFunction;
}

namespace layers {

class Animation;
class AnimationData;
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
class ShadowLayer;
class ShadowableLayer;
class ShadowLayerForwarder;
class ShadowLayerManager;
class SpecificLayerAttributes;

#define MOZ_LAYER_DECL_NAME(n, e)                           \
  virtual const char* Name() const { return n; }            \
  virtual LayerType GetType() const { return e; }

/**
 * Base class for userdata objects attached to layers and layer managers.
 */
class THEBES_API LayerUserData {
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
class THEBES_API LayerManager {
  NS_INLINE_DECL_REFCOUNTING(LayerManager)

public:
  LayerManager()
    : mDestroyed(false)
    , mSnapEffectiveTransforms(true)
    , mId(0)
    , mInTransaction(false)
  {
    InitLog();
  }
  virtual ~LayerManager() {}

  /**
   * Release layers and resources held by this layer manager, and mark
   * it as destroyed.  Should do any cleanup necessary in preparation
   * for its widget going away.  After this call, only user data calls
   * are valid on the layer manager.
   */
  virtual void Destroy() { mDestroyed = true; mUserData.Destroy(); }
  bool IsDestroyed() { return mDestroyed; }

  virtual ShadowLayerForwarder* AsShadowForwarder()
  { return nullptr; }

  virtual ShadowLayerManager* AsShadowManager()
  { return nullptr; }

  /**
   * Returns true if this LayerManager is owned by an nsIWidget,
   * and is used for drawing into the widget.
   */
  virtual bool IsWidgetLayerManager() { return true; }

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
    END_NO_COMPOSITE = 1 << 1 // Do not composite after drawing thebes layer contents.
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

  virtual bool HasShadowManagerInternal() const { return false; }
  bool HasShadowManager() const { return HasShadowManagerInternal(); }

  bool IsSnappingEffectiveTransforms() { return mSnapEffectiveTransforms; } 

  /** 
   * Returns true if this LayerManager can properly support layers with
   * SURFACE_COMPONENT_ALPHA. This can include disabling component
   * alpha if required.
   */
  virtual bool AreComponentAlphaLayersEnabled() { return true; }

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
   * CONSTRUCTION PHASE ONLY
   * Create a ThebesLayer for this manager's layer tree.
   */
  virtual already_AddRefed<ThebesLayer> CreateThebesLayer() = 0;
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
   * Creates a surface which is optimized for inter-operating with this layer
   * manager.
   */
  virtual already_AddRefed<gfxASurface>
    CreateOptimalSurface(const gfxIntSize &aSize,
                         gfxASurface::gfxImageFormat imageFormat);
 
  /**
   * Creates a surface for alpha masks which is optimized for inter-operating
   * with this layer manager. In contrast to CreateOptimalSurface, this surface
   * is optimised for drawing alpha only and we assume that drawing the mask
   * is fairly simple.
   */
  virtual already_AddRefed<gfxASurface>
    CreateOptimalMaskSurface(const gfxIntSize &aSize);

  /**
   * Creates a DrawTarget which is optimized for inter-operating with this
   * layermanager.
   */
  virtual TemporaryRef<mozilla::gfx::DrawTarget>
    CreateDrawTarget(const mozilla::gfx::IntSize &aSize,
                     mozilla::gfx::SurfaceFormat aFormat);

  virtual bool CanUseCanvasLayerForSize(const gfxIntSize &aSize) { return true; }

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
  LayerUserData* GetUserData(void* aKey)
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

  // We always declare the following logging symbols, because it's
  // extremely tricky to conditionally declare them.  However, for
  // ifndef MOZ_LAYERS_HAVE_LOG builds, they only have trivial
  // definitions in Layers.cpp.
  virtual const char* Name() const { return "???"; }

  /**
   * Dump information about this layer manager and its managed tree to
   * aFile, which defaults to stderr.
   */
  void Dump(FILE* aFile=NULL, const char* aPrefix="", bool aDumpHtml=false);
  /**
   * Dump information about just this layer manager itself to aFile,
   * which defaults to stderr.
   */
  void DumpSelf(FILE* aFile=NULL, const char* aPrefix="");

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

  void StartFrameTimeRecording();
  void SetPaintStartTime(TimeStamp& aTime);
  void StopFrameTimeRecording(nsTArray<float>& aFrameTimes, nsTArray<float>& aProcessingTimes);

  void PostPresent();

  void BeginTabSwitch();

  static bool IsLogEnabled();
  static PRLogModuleInfo* GetLog() { return sLog; }

  bool IsCompositingCheap(LayersBackend aBackend)
  { return LAYERS_BASIC != aBackend; }

  virtual bool IsCompositingCheap() { return true; }

  bool IsInTransaction() const { return mInTransaction; }

protected:
  nsRefPtr<Layer> mRoot;
  gfx::UserData mUserData;
  bool mDestroyed;
  bool mSnapEffectiveTransforms;

  // Print interesting information about this into aTo.  Internally
  // used to implement Dump*() and Log*().
  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix);

  static void InitLog();
  static PRLogModuleInfo* sLog;
  uint64_t mId;
  bool mInTransaction;
private:
  TimeStamp mLastFrameTime;
  TimeStamp mPaintStartTime;
  nsTArray<float> mFrameIntervals;
  nsTArray<float> mPaintTimes;
  TimeStamp mTabSwitchStart;
};

class ThebesLayer;
typedef InfallibleTArray<Animation> AnimationArray;

struct AnimData {
  InfallibleTArray<nsStyleAnimation::Value> mStartValues;
  InfallibleTArray<nsStyleAnimation::Value> mEndValues;
  InfallibleTArray<mozilla::css::ComputedTimingFunction*> mFunctions;
};

/**
 * A Layer represents anything that can be rendered onto a destination
 * surface.
 */
class THEBES_API Layer {
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

  virtual ~Layer();

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
     * If this is set then this layer is part of a preserve-3d group, and should
     * be sorted with sibling layers that are also part of the same group.
     */
    CONTENT_PRESERVE_3D = 0x04,
    /**
     * This indicates that the transform may be changed on during an empty
     * transaction where there is no possibility of redrawing the content, so the
     * implementation should be ready for that.
     */
    CONTENT_MAY_CHANGE_TRANSFORM = 0x08
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
      mVisibleRegion = aRegion;
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
      mOpacity = aOpacity;
      Mutated();
    }
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
        mUseClipRect = false;
        Mutated();
      } else {
        if (!aRect->IsEqualEdges(mClipRect)) {
          mClipRect = *aRect;
          Mutated();
        }
      }
    } else {
      if (aRect) {
        Mutated();
        mUseClipRect = true;
        if (!aRect->IsEqualEdges(mClipRect)) {
          mClipRect = *aRect;
        }
      }
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set a clip rect which will be applied to this layer as it is
   * composited to the destination. The coordinates are relative to
   * the parent layer (i.e. the contents of this layer
   * are transformed before this clip rect is applied).
   * For the root layer, the coordinates are relative to the widget,
   * in device pixels.
   * The provided rect is intersected with any existing clip rect.
   */
  void IntersectClipRect(const nsIntRect& aRect)
  {
    if (mUseClipRect) {
      mClipRect.IntersectRect(mClipRect, aRect);
    } else {
      mUseClipRect = true;
      mClipRect = aRect;
    }
    Mutated();
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
      gfxMatrix maskTransform;
      bool maskIs2D = aMaskLayer->GetTransform().CanDraw2D(&maskTransform);
      NS_ASSERTION(maskIs2D, "Mask layer has invalid transform.");
    }
#endif

    if (mMaskLayer != aMaskLayer) {
      mMaskLayer = aMaskLayer;
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Tell this layer what its transform should be. The transformation
   * is applied when compositing the layer into its parent container.
   * XXX Currently only transformations corresponding to 2D affine transforms
   * are supported.
   */
  void SetBaseTransform(const gfx3DMatrix& aMatrix)
  {
    NS_ASSERTION(!aMatrix.IsSingular(), 
                 "Shouldn't be trying to draw with a singular matrix!");
    mPendingTransform = nullptr;
    if (mTransform == aMatrix) {
      return;
    }
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
  void SetBaseTransformForNextTransaction(const gfx3DMatrix& aMatrix)
  {
    mPendingTransform = new gfx3DMatrix(aMatrix);
  }

  void SetPostScale(float aXScale, float aYScale)
  {
    if (mPostXScale == aXScale && mPostYScale == aYScale) {
      return;
    }
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
  void SetIsFixedPosition(bool aFixedPosition) { mIsFixedPosition = aFixedPosition; }

  // Call AddAnimation to add a new animation to this layer from layout code.
  // Caller must add segments to the returned animation.
  Animation* AddAnimation(mozilla::TimeStamp aStart, mozilla::TimeDuration aDuration,
                          float aIterations, int aDirection,
                          nsCSSProperty aProperty, const AnimationData& aData);
  // ClearAnimations clears animations on this layer.
  void ClearAnimations();
  // This is only called when the layer tree is updated. Do not call this from
  // layout code.  To add an animation to this layer, use AddAnimation.
  void SetAnimations(const AnimationArray& aAnimations);

  /**
   * CONSTRUCTION PHASE ONLY
   * If a layer is "fixed position", this determines which point on the layer
   * is considered the "anchor" point, that is, the point which remains in the
   * same position when compositing the layer tree with a transformation
   * (such as when asynchronously scrolling and zooming).
   */
  void SetFixedPositionAnchor(const gfxPoint& aAnchor) { mAnchor = aAnchor; }

  // These getters can be used anytime.
  float GetOpacity() { return mOpacity; }
  const nsIntRect* GetClipRect() { return mUseClipRect ? &mClipRect : nullptr; }
  uint32_t GetContentFlags() { return mContentFlags; }
  const nsIntRegion& GetVisibleRegion() { return mVisibleRegion; }
  ContainerLayer* GetParent() { return mParent; }
  Layer* GetNextSibling() { return mNextSibling; }
  Layer* GetPrevSibling() { return mPrevSibling; }
  virtual Layer* GetFirstChild() { return nullptr; }
  virtual Layer* GetLastChild() { return nullptr; }
  const gfx3DMatrix GetTransform();
  const gfx3DMatrix& GetBaseTransform() { return mTransform; }
  float GetPostXScale() { return mPostXScale; }
  float GetPostYScale() { return mPostYScale; }
  bool GetIsFixedPosition() { return mIsFixedPosition; }
  gfxPoint GetFixedPositionAnchor() { return mAnchor; }
  Layer* GetMaskLayer() { return mMaskLayer; }

  AnimationArray& GetAnimations() { return mAnimations; }
  InfallibleTArray<AnimData>& GetAnimationData() { return mAnimationData; }

  uint64_t GetAnimationGeneration() { return mAnimationGeneration; }
  void SetAnimationGeneration(uint64_t aCount) { mAnimationGeneration = aCount; }

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

  enum SurfaceMode {
    SURFACE_OPAQUE,
    SURFACE_SINGLE_CHANNEL_ALPHA,
    SURFACE_COMPONENT_ALPHA
  };
  SurfaceMode GetSurfaceMode()
  {
    if (CanUseOpaqueSurface())
      return SURFACE_OPAQUE;
    if (mContentFlags & CONTENT_COMPONENT_ALPHA)
      return SURFACE_COMPONENT_ALPHA;
    return SURFACE_SINGLE_CHANNEL_ALPHA;
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
  LayerUserData* GetUserData(void* aKey)
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
   * Dynamic cast to a ShadowLayer.  Return null if this is not a
   * ShadowLayer.  Can be used anytime.
   */
  virtual ShadowLayer* AsShadowLayer() { return nullptr; }

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
   * This returns the effective transform computed by
   * ComputeEffectiveTransforms. Typically this is a transform that transforms
   * this layer all the way to some intermediate surface or destination
   * surface. For non-BasicLayers this will be a transform to the nearest
   * ancestor with UseIntermediateSurface() (or to the root, if there is no
   * such ancestor), but for BasicLayers it's different.
   */
  const gfx3DMatrix& GetEffectiveTransform() const { return mEffectiveTransform; }

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
  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface) = 0;
    
  /**
   * computes the effective transform for a mask layer, if this layer has one
   */
  void ComputeEffectiveTransformForMaskLayer(const gfx3DMatrix& aTransformToSurface);

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
                                 const gfxMatrix* aWorldTransform);

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
   * aFile, which defaults to stderr.
   */
  void Dump(FILE* aFile=NULL, const char* aPrefix="", bool aDumpHtml=false);
  /**
   * Dump information about just this layer manager itself to aFile,
   * which defaults to stderr.
   */
  void DumpSelf(FILE* aFile=NULL, const char* aPrefix="");

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

  static bool IsLogEnabled() { return LayerManager::IsLogEnabled(); }

  /**
   * Returns the current area of the layer (in layer-space coordinates)
   * marked as needed to be recomposited.
   */
  const nsIntRegion& GetInvalidRegion() { return mInvalidRegion; }

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

  void ApplyPendingUpdatesForThisTransaction();

#ifdef DEBUG
  void SetDebugColorIndex(uint32_t aIndex) { mDebugColorIndex = aIndex; }
  uint32_t GetDebugColorIndex() { return mDebugColorIndex; }
#endif

protected:
  Layer(LayerManager* aManager, void* aImplData);

  void Mutated() { mManager->Mutated(this); }

  // Print interesting information about this into aTo.  Internally
  // used to implement Dump*() and Log*().  If subclasses have
  // additional interesting properties, they should override this with
  // an implementation that first calls the base implementation then
  // appends additional info to aTo.
  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix);

  /**
   * Returns the local transform for this layer: either mTransform or,
   * for shadow layers, GetShadowTransform()
   */
  const gfx3DMatrix GetLocalTransform();

  /**
   * Returns the local opacity for this layer: either mOpacity or,
   * for shadow layers, GetShadowOpacity()
   */
  const float GetLocalOpacity();

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
  gfx3DMatrix SnapTransformTranslation(const gfx3DMatrix& aTransform,
                                       gfxMatrix* aResidualTransform);
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
  gfx3DMatrix SnapTransform(const gfx3DMatrix& aTransform,
                            const gfxRect& aSnapRect,
                            gfxMatrix* aResidualTransform);

  /**
   * Returns true if this layer's effective transform is not just
   * a translation by integers, or if this layer or some ancestor layer
   * is marked as having a transform that may change without a full layer
   * transaction.
   */
  bool MayResample();

  LayerManager* mManager;
  ContainerLayer* mParent;
  Layer* mNextSibling;
  Layer* mPrevSibling;
  void* mImplData;
  nsRefPtr<Layer> mMaskLayer;
  gfx::UserData mUserData;
  nsIntRegion mVisibleRegion;
  gfx3DMatrix mTransform;
  // A mutation of |mTransform| that we've queued to be applied at the
  // end of the next transaction (if nothing else overrides it in the
  // meantime).
  nsAutoPtr<gfx3DMatrix> mPendingTransform;
  float mPostXScale;
  float mPostYScale;
  gfx3DMatrix mEffectiveTransform;
  AnimationArray mAnimations;
  InfallibleTArray<AnimData> mAnimationData;
  float mOpacity;
  nsIntRect mClipRect;
  nsIntRect mTileSourceRect;
  nsIntRegion mInvalidRegion;
  uint32_t mContentFlags;
  bool mUseClipRect;
  bool mUseTileSourceRect;
  bool mIsFixedPosition;
  gfxPoint mAnchor;
  DebugOnly<uint32_t> mDebugColorIndex;
  // If this layer is used for OMTA, then this counter is used to ensure we
  // stay in sync with the animation manager
  uint64_t mAnimationGeneration;
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
class THEBES_API ThebesLayer : public Layer {
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

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    gfx3DMatrix idealTransform = GetLocalTransform()*aTransformToSurface;
    gfxMatrix residual;
    mEffectiveTransform = SnapTransformTranslation(idealTransform,
        mAllowResidualTranslation ? &residual : nullptr);
    // The residual can only be a translation because SnapTransformTranslation
    // only changes the transform if it's a translation
    NS_ASSERTION(!residual.HasNonTranslation(),
                 "Residual transform can only be a translation");
    if (!residual.GetTranslation().WithinEpsilonOf(mResidualTranslation, 1e-3f)) {
      mResidualTranslation = residual.GetTranslation();
      NS_ASSERTION(-0.5 <= mResidualTranslation.x && mResidualTranslation.x < 0.5 &&
                   -0.5 <= mResidualTranslation.y && mResidualTranslation.y < 0.5,
                   "Residual translation out of range");
      mValidRegion.SetEmpty();
    }
    ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
  }

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
  ThebesLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData)
    , mValidRegion()
    , mUsedForReadback(false)
    , mAllowResidualTranslation(false)
  {
    mContentFlags = 0; // Clear NO_TEXT, NO_TEXT_OVER_TRANSPARENT
  }

  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix);

  /**
   * ComputeEffectiveTransforms snaps the ideal transform to get mEffectiveTransform.
   * mResidualTranslation is the translation that should be applied *before*
   * mEffectiveTransform to get the ideal transform.
   */
  gfxPoint mResidualTranslation;
  nsIntRegion mValidRegion;
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
class THEBES_API ContainerLayer : public Layer {
public:
  /**
   * CONSTRUCTION PHASE ONLY
   * Insert aChild into the child list of this container. aChild must
   * not be currently in any child list or the root for the layer manager.
   * If aAfter is non-null, it must be a child of this container and
   * we insert after that layer. If it's null we insert at the start.
   */
  virtual void InsertAfter(Layer* aChild, Layer* aAfter) = 0;
  /**
   * CONSTRUCTION PHASE ONLY
   * Remove aChild from the child list of this container. aChild must
   * be a child of this container.
   */
  virtual void RemoveChild(Layer* aChild) = 0;
  /**
   * CONSTRUCTION PHASE ONLY
   * Reposition aChild from the child list of this container. aChild must
   * be a child of this container.
   * If aAfter is non-null, it must be a child of this container and we
   * reposition after that layer. If it's null, we reposition at the start.
   */
  virtual void RepositionChild(Layer* aChild, Layer* aAfter) = 0;

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the (sub)document metrics used to render the Layer subtree
   * rooted at this.
   */
  void SetFrameMetrics(const FrameMetrics& aFrameMetrics)
  {
    if (mFrameMetrics != aFrameMetrics) {
      mFrameMetrics = aFrameMetrics;
      Mutated();
    }
  }

  void SetPreScale(float aXScale, float aYScale)
  {
    if (mPreXScale == aXScale && mPreYScale == aYScale) {
      return;
    }
    mPreXScale = aXScale;
    mPreYScale = aYScale;
    Mutated();
  }

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs);

  void SortChildrenBy3DZOrder(nsTArray<Layer*>& aArray);

  // These getters can be used anytime.

  virtual ContainerLayer* AsContainerLayer() { return this; }

  virtual Layer* GetFirstChild() { return mFirstChild; }
  virtual Layer* GetLastChild() { return mLastChild; }
  const FrameMetrics& GetFrameMetrics() { return mFrameMetrics; }
  float GetPreXScale() { return mPreXScale; }
  float GetPreYScale() { return mPreYScale; }

  MOZ_LAYER_DECL_NAME("ContainerLayer", TYPE_CONTAINER)

  /**
   * ContainerLayer backends need to override ComputeEffectiveTransforms
   * since the decision about whether to use a temporary surface for the
   * container is backend-specific. ComputeEffectiveTransforms must also set
   * mUseIntermediateSurface.
   */
  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface) = 0;

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

protected:
  friend class ReadbackProcessor;

  void DidInsertChild(Layer* aLayer);
  void DidRemoveChild(Layer* aLayer);

  ContainerLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData),
      mFirstChild(nullptr),
      mLastChild(nullptr),
      mPreXScale(1.0f),
      mPreYScale(1.0f),
      mUseIntermediateSurface(false),
      mSupportsComponentAlphaChildren(false),
      mMayHaveReadbackChild(false)
  {
    mContentFlags = 0; // Clear NO_TEXT, NO_TEXT_OVER_TRANSPARENT
  }

  /**
   * A default implementation of ComputeEffectiveTransforms for use by OpenGL
   * and D3D.
   */
  void DefaultComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface);

  /**
   * Loops over the children calling ComputeEffectiveTransforms on them.
   */
  void ComputeEffectiveTransformsForChildren(const gfx3DMatrix& aTransformToSurface);

  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix);

  Layer* mFirstChild;
  Layer* mLastChild;
  FrameMetrics mFrameMetrics;
  float mPreXScale;
  float mPreYScale;
  bool mUseIntermediateSurface;
  bool mSupportsComponentAlphaChildren;
  bool mMayHaveReadbackChild;
};

/**
 * A Layer which just renders a solid color in its visible region. It actually
 * can fill any area that contains the visible region, so if you need to
 * restrict the area filled, set a clip region on this layer.
 */
class THEBES_API ColorLayer : public Layer {
public:
  virtual ColorLayer* AsColorLayer() { return this; }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the color of the layer.
   */
  virtual void SetColor(const gfxRGBA& aColor)
  {
    mColor = aColor;
  }

  // This getter can be used anytime.
  virtual const gfxRGBA& GetColor() { return mColor; }

  MOZ_LAYER_DECL_NAME("ColorLayer", TYPE_COLOR)

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    gfx3DMatrix idealTransform = GetLocalTransform()*aTransformToSurface;
    mEffectiveTransform = SnapTransformTranslation(idealTransform, nullptr);
    ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
  }

protected:
  ColorLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData),
      mColor(0.0, 0.0, 0.0, 0.0)
  {}

  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix);

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
class THEBES_API CanvasLayer : public Layer {
public:
  struct Data {
    Data()
      : mSurface(nullptr), mGLContext(nullptr)
      , mDrawTarget(nullptr), mGLBufferIsPremultiplied(false)
    { }

    /* One of these two must be specified, but never both */
    gfxASurface* mSurface;  // a gfx Surface for the canvas contents
    mozilla::gl::GLContext* mGLContext; // a GL PBuffer Context
    mozilla::gfx::DrawTarget *mDrawTarget; // a DrawTarget for the canvas contents

    /* The size of the canvas content */
    nsIntSize mSize;

    /* Whether the GLContext contains premultiplied alpha
     * values in the framebuffer or not.  Defaults to FALSE.
     */
    bool mGLBufferIsPremultiplied;
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
   * Register a callback to be called at the end of each transaction.
   */
  typedef void (* DidTransactionCallback)(void* aClosureData);
  void SetDidTransactionCallback(DidTransactionCallback aCallback, void* aClosureData)
  {
    mCallback = aCallback;
    mCallbackData = aClosureData;
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the filter used to resample this image (if necessary).
   */
  void SetFilter(gfxPattern::GraphicsFilter aFilter) { mFilter = aFilter; }
  gfxPattern::GraphicsFilter GetFilter() const { return mFilter; }

  MOZ_LAYER_DECL_NAME("CanvasLayer", TYPE_CANVAS)

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
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
    : Layer(aManager, aImplData),
      mCallback(nullptr), mCallbackData(nullptr), mFilter(gfxPattern::FILTER_GOOD),
      mDirty(false) {}

  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix);

  void FireDidTransactionCallback()
  {
    if (mCallback) {
      mCallback(mCallbackData);
    }
  }

  /**
   * 0, 0, canvaswidth, canvasheight
   */
  nsIntRect mBounds;
  DidTransactionCallback mCallback;
  void* mCallbackData;
  gfxPattern::GraphicsFilter mFilter;

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
class THEBES_API RefLayer : public ContainerLayer {
  friend class LayerManager;

private:
  virtual void InsertAfter(Layer* aChild, Layer* aAfter)
  { MOZ_NOT_REACHED("no"); }

  virtual void RemoveChild(Layer* aChild)
  { MOZ_NOT_REACHED("no"); }

  virtual void RepositionChild(Layer* aChild, Layer* aAfter)
  { MOZ_NOT_REACHED("no"); }

  using ContainerLayer::SetFrameMetrics;

public:
  /**
   * CONSTRUCTION PHASE ONLY
   * Set the ID of the layer's referent.
   */
  void SetReferentId(uint64_t aId)
  {
    MOZ_ASSERT(aId != 0);
    if (mId != aId) {
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

  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix);

  Layer* mTempReferent;
  // 0 is a special value that means "no ID".
  uint64_t mId;
};

#ifdef MOZ_DUMP_PAINTING
void WriteSnapshotToDumpFile(Layer* aLayer, gfxASurface* aSurf);
void WriteSnapshotToDumpFile(LayerManager* aManager, gfxASurface* aSurf);
#endif

}
}

#endif /* GFX_LAYERS_H */
