/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GFX_LAYERS_H
#define GFX_LAYERS_H

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

#include "mozilla/gfx/2D.h"

#if defined(DEBUG) || defined(PR_LOGGING)
#  include <stdio.h>            // FILE
#  include "prlog.h"
#  define MOZ_LAYERS_HAVE_LOG
#  define MOZ_LAYERS_LOG(_args)                             \
  PR_LOG(LayerManager::GetLog(), PR_LOG_DEBUG, _args)
#else
struct PRLogModuleInfo;
#  define MOZ_LAYERS_LOG(_args)
#endif  // if defined(DEBUG) || defined(PR_LOGGING)

class gfxContext;
class nsPaintEvent;

namespace mozilla {
namespace gl {
class GLContext;
}

namespace layers {

class Layer;
class ThebesLayer;
class ContainerLayer;
class ImageLayer;
class ColorLayer;
class ImageContainer;
class CanvasLayer;
class ReadbackLayer;
class ReadbackProcessor;
class ShadowLayer;
class ShadowLayerForwarder;
class ShadowLayerManager;
class SpecificLayerAttributes;

/**
 * The viewport and displayport metrics for the painted frame at the
 * time of a layer-tree transaction.  These metrics are especially
 * useful for shadow layers, because the metrics values are updated
 * atomically with new pixels.
 */
struct THEBES_API FrameMetrics {
public:
  // We use IDs to identify frames across processes.
  typedef PRUint64 ViewID;
  static const ViewID NULL_SCROLL_ID;   // This container layer does not scroll.
  static const ViewID ROOT_SCROLL_ID;   // This is the root scroll frame.
  static const ViewID START_SCROLL_ID;  // This is the ID that scrolling subframes
                                        // will begin at.

  FrameMetrics()
    : mViewport(0, 0, 0, 0)
    , mContentSize(0, 0)
    , mViewportScrollOffset(0, 0)
    , mScrollId(NULL_SCROLL_ID)
  {}

  // Default copy ctor and operator= are fine

  bool operator==(const FrameMetrics& aOther) const
  {
    return (mViewport.IsEqualEdges(aOther.mViewport) &&
            mViewportScrollOffset == aOther.mViewportScrollOffset &&
            mDisplayPort.IsEqualEdges(aOther.mDisplayPort) &&
            mScrollId == aOther.mScrollId);
  }
  bool operator!=(const FrameMetrics& aOther) const
  { 
    return !operator==(aOther);
  }

  bool IsDefault() const
  {
    return (FrameMetrics() == *this);
  }

  bool IsRootScrollable() const
  {
    return mScrollId == ROOT_SCROLL_ID;
  }

  bool IsScrollable() const
  {
    return mScrollId != NULL_SCROLL_ID;
  }

  // These are all in layer coordinate space.
  nsIntRect mViewport;
  nsIntSize mContentSize;
  nsIntPoint mViewportScrollOffset;
  nsIntRect mDisplayPort;
  ViewID mScrollId;
};

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

/**
 * Helper class to manage user data for layers and LayerManagers.
 */
class THEBES_API LayerUserDataSet {
public:
  LayerUserDataSet() : mKey(nsnull) {}

  void Set(void* aKey, LayerUserData* aValue)
  {
    NS_ASSERTION(!mKey || mKey == aKey,
                 "Multiple LayerUserData objects not supported");
    mKey = aKey;
    mValue = aValue;
  }
  /**
   * This can be used anytime. Ownership passes to the caller!
   */
  LayerUserData* Remove(void* aKey)
  {
    if (mKey == aKey) {
      mKey = nsnull;
      LayerUserData* d = mValue.forget();
      return d;
    }
    return nsnull;
  }
  /**
   * This getter can be used anytime.
   */
  bool Has(void* aKey)
  {
    return mKey == aKey;
  }
  /**
   * This getter can be used anytime. Ownership is retained by this object.
   */
  LayerUserData* Get(void* aKey)
  {
    return mKey == aKey ? mValue.get() : nsnull;
  }

  /**
   * Clear out current user data.
   */
  void Clear()
  {
    mKey = nsnull;
    mValue = nsnull;
  }

private:
  void* mKey;
  nsAutoPtr<LayerUserData> mValue;
};

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
  enum LayersBackend {
    LAYERS_NONE = 0,
    LAYERS_BASIC,
    LAYERS_OPENGL,
    LAYERS_D3D9,
    LAYERS_D3D10,
    LAYERS_LAST
  };

  LayerManager() : mDestroyed(PR_FALSE), mSnapEffectiveTransforms(PR_TRUE)
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
  virtual void Destroy() { mDestroyed = PR_TRUE; mUserData.Clear(); }
  bool IsDestroyed() { return mDestroyed; }

  virtual ShadowLayerForwarder* AsShadowForwarder()
  { return nsnull; }

  virtual ShadowLayerManager* AsShadowManager()
  { return nsnull; }

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
  /**
   * Attempts to end an "empty transaction". There must have been no
   * changes to the layer tree since the BeginTransaction().
   * It's possible for this to fail; ThebesLayers may need to be updated
   * due to VRAM data being lost, for example. In such cases this method
   * returns false, and the caller must proceed with a normal layer tree
   * update and EndTransaction.
   */
  virtual bool EndEmptyTransaction() = 0;

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

  enum EndTransactionFlags {
    END_DEFAULT = 0,
    END_NO_IMMEDIATE_REDRAW = 1 << 0  // Do not perform the drawing phase
  };

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

  bool IsSnappingEffectiveTransforms() { return mSnapEffectiveTransforms; } 

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
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer() { return nsnull; }

  /**
   * Can be called anytime
   */
  virtual already_AddRefed<ImageContainer> CreateImageContainer() = 0;

  /**
   * Type of layer manager his is. This is to be used sparsely in order to
   * avoid a lot of Layers backend specific code. It should be used only when
   * Layers backend specific functionality is necessary.
   */
  virtual LayersBackend GetBackendType() = 0;
 
  /**
   * Creates a layer which is optimized for inter-operating with this layer
   * manager.
   */
  virtual already_AddRefed<gfxASurface>
    CreateOptimalSurface(const gfxIntSize &aSize,
                         gfxASurface::gfxImageFormat imageFormat);

  /**
   * Creates a DrawTarget which is optimized for inter-operating with this
   * layermanager.
   */
  virtual TemporaryRef<mozilla::gfx::DrawTarget>
    CreateDrawTarget(const mozilla::gfx::IntSize &aSize,
                     mozilla::gfx::SurfaceFormat aFormat);

  virtual bool CanUseCanvasLayerForSize(const gfxIntSize &aSize) { return PR_TRUE; }

  /**
   * Return the name of the layer manager's backend.
   */
  virtual void GetBackendName(nsAString& aName) = 0;

  /**
   * This setter can be used anytime. The user data for all keys is
   * initially null. Ownership pases to the layer manager.
   */
  void SetUserData(void* aKey, LayerUserData* aData)
  { mUserData.Set(aKey, aData); }
  /**
   * This can be used anytime. Ownership passes to the caller!
   */
  nsAutoPtr<LayerUserData> RemoveUserData(void* aKey)
  { nsAutoPtr<LayerUserData> d(mUserData.Remove(aKey)); return d; }
  /**
   * This getter can be used anytime.
   */
  bool HasUserData(void* aKey)
  { return mUserData.Has(aKey); }
  /**
   * This getter can be used anytime. Ownership is retained by the layer
   * manager.
   */
  LayerUserData* GetUserData(void* aKey)
  { return mUserData.Get(aKey); }

  // We always declare the following logging symbols, because it's
  // extremely tricky to conditionally declare them.  However, for
  // ifndef MOZ_LAYERS_HAVE_LOG builds, they only have trivial
  // definitions in Layers.cpp.
  virtual const char* Name() const { return "???"; }

  /**
   * Dump information about this layer manager and its managed tree to
   * aFile, which defaults to stderr.
   */
  void Dump(FILE* aFile=NULL, const char* aPrefix="");
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

  static bool IsLogEnabled();
  static PRLogModuleInfo* GetLog() { return sLog; }

  bool IsCompositingCheap(LayerManager::LayersBackend aBackend)
  { return LAYERS_BASIC != aBackend; }

  virtual bool IsCompositingCheap() { return true; }

protected:
  nsRefPtr<Layer> mRoot;
  LayerUserDataSet mUserData;
  bool mDestroyed;
  bool mSnapEffectiveTransforms;

  // Print interesting information about this into aTo.  Internally
  // used to implement Dump*() and Log*().
  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix);

  static void InitLog();
  static PRLogModuleInfo* sLog;
};

class ThebesLayer;

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
    TYPE_SHADOW,
    TYPE_THEBES
  };

  virtual ~Layer() {}

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
    CONTENT_COMPONENT_ALPHA = 0x02
  };
  /**
   * CONSTRUCTION PHASE ONLY
   * This lets layout make some promises about what will be drawn into the
   * visible region of the ThebesLayer. This enables internal quality
   * and performance optimizations.
   */
  void SetContentFlags(PRUint32 aFlags)
  {
    NS_ASSERTION((aFlags & (CONTENT_OPAQUE | CONTENT_COMPONENT_ALPHA)) !=
                 (CONTENT_OPAQUE | CONTENT_COMPONENT_ALPHA),
                 "Can't be opaque and require component alpha");
    mContentFlags = aFlags;
    Mutated();
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
    mVisibleRegion = aRegion;
    Mutated();
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the opacity which will be applied to this layer as it
   * is composited to the destination.
   */
  void SetOpacity(float aOpacity)
  {
    mOpacity = aOpacity;
    Mutated();
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
    mUseClipRect = aRect != nsnull;
    if (aRect) {
      mClipRect = *aRect;
    }
    Mutated();
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
      mUseClipRect = PR_TRUE;
      mClipRect = aRect;
    }
    Mutated();
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Tell this layer what its transform should be. The transformation
   * is applied when compositing the layer into its parent container.
   * XXX Currently only transformations corresponding to 2D affine transforms
   * are supported.
   */
  void SetTransform(const gfx3DMatrix& aMatrix)
  {
    mTransform = aMatrix;
    Mutated();
  }

  /**
   * CONSTRUCTION PHASE ONLY
   *
   * Define a subrect of this layer that will be used as the source
   * image for tiling this layer's visible region.  The coordinates
   * are in the un-transformed space of this layer (i.e. the visible
   * region of this this layer is tiled before being transformed).
   * The visible region is tiled "outwards" from the source rect; that
   * is, the source rect is drawn "in place", then repeated to cover
   * the layer's visible region.
   *
   * The interpretation of the source rect varies depending on
   * underlying layer type.  For ImageLayers and CanvasLayers, it
   * doesn't make sense to set a source rect not fully contained by
   * the bounds of their underlying images.  For ThebesLayers, thebes
   * content may need to be rendered to fill the source rect.  For
   * ColorLayers, a source rect for tiling doesn't make sense at all.
   *
   * If aRect is null no tiling will be performed. 
   *
   * NB: this interface is only implemented for BasicImageLayers, and
   * then only for source rects the same size as the layers'
   * underlying images.
   */
  void SetTileSourceRect(const nsIntRect* aRect)
  {
    mUseTileSourceRect = aRect != nsnull;
    if (aRect) {
      mTileSourceRect = *aRect;
    }
    Mutated();
  }

  void SetIsFixedPosition(bool aFixedPosition) { mIsFixedPosition = aFixedPosition; }

  // These getters can be used anytime.
  float GetOpacity() { return mOpacity; }
  const nsIntRect* GetClipRect() { return mUseClipRect ? &mClipRect : nsnull; }
  PRUint32 GetContentFlags() { return mContentFlags; }
  const nsIntRegion& GetVisibleRegion() { return mVisibleRegion; }
  ContainerLayer* GetParent() { return mParent; }
  Layer* GetNextSibling() { return mNextSibling; }
  Layer* GetPrevSibling() { return mPrevSibling; }
  virtual Layer* GetFirstChild() { return nsnull; }
  virtual Layer* GetLastChild() { return nsnull; }
  const gfx3DMatrix& GetTransform() { return mTransform; }
  const nsIntRect* GetTileSourceRect() { return mUseTileSourceRect ? &mTileSourceRect : nsnull; }
  bool GetIsFixedPosition() { return mIsFixedPosition; }

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
  { mUserData.Set(aKey, aData); }
  /**
   * This can be used anytime. Ownership passes to the caller!
   */
  nsAutoPtr<LayerUserData> RemoveUserData(void* aKey)
  { nsAutoPtr<LayerUserData> d(mUserData.Remove(aKey)); return d; }
  /**
   * This getter can be used anytime.
   */
  bool HasUserData(void* aKey)
  { return mUserData.Has(aKey); }
  /**
   * This getter can be used anytime. Ownership is retained by the layer
   * manager.
   */
  LayerUserData* GetUserData(void* aKey)
  { return mUserData.Get(aKey); }

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
  virtual ThebesLayer* AsThebesLayer() { return nsnull; }

  /**
   * Dynamic cast to a ContainerLayer. Returns null if this is not
   * a ContainerLayer.
   */
  virtual ContainerLayer* AsContainerLayer() { return nsnull; }

  /**
   * Dynamic cast to a ShadowLayer.  Return null if this is not a
   * ShadowLayer.  Can be used anytime.
   */
  virtual ShadowLayer* AsShadowLayer() { return nsnull; }

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
  void Dump(FILE* aFile=NULL, const char* aPrefix="");
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

protected:
  Layer(LayerManager* aManager, void* aImplData) :
    mManager(aManager),
    mParent(nsnull),
    mNextSibling(nsnull),
    mPrevSibling(nsnull),
    mImplData(aImplData),
    mOpacity(1.0),
    mContentFlags(0),
    mUseClipRect(PR_FALSE),
    mUseTileSourceRect(PR_FALSE),
    mIsFixedPosition(PR_FALSE)
    {}

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
  const gfx3DMatrix& GetLocalTransform();

  /**
   * Computes a tweaked version of aTransform that snaps a point or a rectangle
   * to pixel boundaries. Snapping is only performed if this layer's
   * layer manager has enabled snapping (which is the default).
   * @param aSnapRect a rectangle whose edges should be snapped to pixel
   * boundaries in the destination surface. If the rectangle is empty,
   * then the snapping process should preserve the scale factors of the
   * transform matrix
   * @param aResidualTransform a transform to apply before mEffectiveTransform
   * in order to get the results to completely match aTransform
   */
  gfx3DMatrix SnapTransform(const gfx3DMatrix& aTransform,
                            const gfxRect& aSnapRect,
                            gfxMatrix* aResidualTransform);

  LayerManager* mManager;
  ContainerLayer* mParent;
  Layer* mNextSibling;
  Layer* mPrevSibling;
  void* mImplData;
  LayerUserDataSet mUserData;
  nsIntRegion mVisibleRegion;
  gfx3DMatrix mTransform;
  gfx3DMatrix mEffectiveTransform;
  float mOpacity;
  nsIntRect mClipRect;
  nsIntRect mTileSourceRect;
  PRUint32 mContentFlags;
  bool mUseClipRect;
  bool mUseTileSourceRect;
  bool mIsFixedPosition;
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
    // The default implementation just snaps 0,0 to pixels.
    gfx3DMatrix idealTransform = GetLocalTransform()*aTransformToSurface;
    gfxMatrix residual;
    mEffectiveTransform = SnapTransform(idealTransform, gfxRect(0, 0, 0, 0),
        mAllowResidualTranslation ? &residual : nsnull);
    // The residual can only be a translation because ThebesLayer snapping
    // only aligns a single point with the pixel grid; scale factors are always
    // preserved exactly
    NS_ASSERTION(!residual.HasNonTranslation(),
                 "Residual transform can only be a translation");
    if (residual.GetTranslation() != mResidualTranslation) {
      mResidualTranslation = residual.GetTranslation();
      NS_ASSERTION(-0.5 <= mResidualTranslation.x && mResidualTranslation.x < 0.5 &&
                   -0.5 <= mResidualTranslation.y && mResidualTranslation.y < 0.5,
                   "Residual translation out of range");
      mValidRegion.SetEmpty();
    }
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
   * Set the (sub)document metrics used to render the Layer subtree
   * rooted at this.
   */
  void SetFrameMetrics(const FrameMetrics& aFrameMetrics)
  {
    mFrameMetrics = aFrameMetrics;
    Mutated();
  }

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs);

  // These getters can be used anytime.

  virtual ContainerLayer* AsContainerLayer() { return this; }

  virtual Layer* GetFirstChild() { return mFirstChild; }
  virtual Layer* GetLastChild() { return mLastChild; }
  const FrameMetrics& GetFrameMetrics() { return mFrameMetrics; }

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
      mFirstChild(nsnull),
      mLastChild(nsnull),
      mUseIntermediateSurface(PR_FALSE),
      mSupportsComponentAlphaChildren(PR_FALSE),
      mMayHaveReadbackChild(PR_FALSE)
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
    // Snap 0,0 to pixel boundaries, no extra internal transform.
    gfx3DMatrix idealTransform = GetLocalTransform()*aTransformToSurface;
    mEffectiveTransform = SnapTransform(idealTransform, gfxRect(0, 0, 0, 0), nsnull);
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
      : mSurface(nsnull), mGLContext(nsnull)
      , mDrawTarget(nsnull), mGLBufferIsPremultiplied(PR_FALSE)
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
  void Updated() { mDirty = PR_TRUE; }

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
                      nsnull)*
        SnapTransform(aTransformToSurface, gfxRect(0, 0, 0, 0), nsnull);
  }

protected:
  CanvasLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData),
      mCallback(nsnull), mCallbackData(nsnull), mFilter(gfxPattern::FILTER_GOOD),
      mDirty(PR_FALSE) {}

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
  /**
   * Set to true in Updated(), cleared during a transaction.
   */
  bool mDirty;
};

}
}

#endif /* GFX_LAYERS_H */
