/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsRegion.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"
#include "gfx3DMatrix.h"

class gfxContext;
class nsPaintEvent;

namespace mozilla {
namespace layers {

class Layer;
class ThebesLayer;
class ContainerLayer;
class ImageLayer;
class ImageContainer;

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
    LAYERS_BASIC = 0,
    LAYERS_OPENGL
  };

  virtual ~LayerManager() {}

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
   * Finish the construction phase of the transaction and enter the
   * drawing phase.
   */
  virtual void EndConstruction() = 0;
  /**
   * Complete the transaction.
   */
  virtual void EndTransaction() = 0;

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the root layer.
   */
  virtual void SetRoot(Layer* aLayer) = 0;

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
   * Can be called anytime
   */
  virtual already_AddRefed<ImageContainer> CreateImageContainer() = 0;

  /**
   * Type of layer manager his is. This is to be used sparsely in order to
   * avoid a lot of Layers backend specific code. It should be used only when
   * Layers backend specific functionality is necessary.
   */
  virtual LayersBackend GetBackendType() = 0;
};

/**
 * A Layer represents anything that can be rendered onto a destination
 * surface.
 */
class THEBES_API Layer {
  NS_INLINE_DECL_REFCOUNTING(Layer)  

public:
  virtual ~Layer() {}

  /**
   * Returns the LayoutManager this Layer belongs to. Cannot be null.
   */
  LayerManager* Manager() { return mManager; }

  /**
   * CONSTRUCTION PHASE ONLY
   * If this is called with aOpaque set to true, the caller is promising
   * that by the end of this transaction the entire visible region
   * (as specified by SetVisibleRegion) will be filled with opaque
   * content. This enables some internal quality and performance
   * optimizations.
   */
  void SetIsOpaqueContent(PRBool aOpaque) { mIsOpaqueContent = aOpaque; }
  /**
   * CONSTRUCTION PHASE ONLY
   * Tell this layer which region will be visible. It is the responsibility
   * of the caller to ensure that content outside this region does not
   * contribute to the final visible window. This can be an
   * overapproximation to the true visible region.
   */
  virtual void SetVisibleRegion(const nsIntRegion& aRegion) {}

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the opacity which will be applied to this layer as it
   * is composited to the destination.
   */
  void SetOpacity(float aOpacity) { mOpacity = aOpacity; }

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
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Tell this layer what its transform should be. The transformation
   * is applied when compositing the layer into its parent container.
   * XXX Currently only transformations corresponding to 2D affine transforms
   * are supported.
   */
  void SetTransform(const gfx3DMatrix& aMatrix) { mTransform = aMatrix; }

  // These getters can be used anytime.
  float GetOpacity() { return mOpacity; }
  const nsIntRect* GetClipRect() { return mUseClipRect ? &mClipRect : nsnull; }
  PRBool IsOpaqueContent() { return mIsOpaqueContent; }
  ContainerLayer* GetParent() { return mParent; }
  Layer* GetNextSibling() { return mNextSibling; }
  Layer* GetPrevSibling() { return mPrevSibling; }
  virtual Layer* GetFirstChild() { return nsnull; }
  const gfx3DMatrix& GetTransform() { return mTransform; }

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

protected:
  Layer(LayerManager* aManager, void* aImplData) :
    mManager(aManager),
    mParent(nsnull),
    mNextSibling(nsnull),
    mPrevSibling(nsnull),
    mImplData(aImplData),
    mOpacity(1.0),
    mUseClipRect(PR_FALSE),
    mIsOpaqueContent(PR_FALSE)
    {}

  LayerManager* mManager;
  ContainerLayer* mParent;
  Layer* mNextSibling;
  Layer* mPrevSibling;
  void* mImplData;
  gfx3DMatrix mTransform;
  float mOpacity;
  nsIntRect mClipRect;
  PRPackedBool mUseClipRect;
  PRPackedBool mIsOpaqueContent;
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
   * DRAWING PHASE ONLY
   * Start drawing into the layer. On return, aRegionToDraw contains the
   * region that needs to be drawn in by the caller. This would normally
   * be a subregion of the visible region. Drawing is not necessarily
   * clipped to aRegionToDraw.
   * 
   * No other layer operations are allowed until we call EndDrawing on this
   * layer. During the drawing phase, all ThebesLayers in the tree must be
   * drawn in tree order, exactly once each, except for those layers
   * where it is known that the visible region is empty. (Calling
   * BeginDrawing on non-visible layers is allowed, but aRegionToDraw
   * will return empty.)
   * 
   * When an empty region is returned in aRegionToDraw, BeginDrawing
   * may return a null context.
   * 
   * The layer system will hold a reference to the returned gfxContext*
   * until EndDrawing is called. The returned gfxContext must not be used
   * after EndDrawing is called.
   */
  virtual gfxContext* BeginDrawing(nsIntRegion* aRegionToDraw) = 0;
  /**
   * DRAWING PHASE ONLY
   * We've finished drawing into this layer. At this point the caller
   * must have drawn all of aRegionToDraw that was returned by
   * BeginDrawing, and we guarantee that buffered contents in the visible
   * region are now valid.
   */
  virtual void EndDrawing() = 0;

  /**
   * DRAWING PHASE ONLY
   * Copy the aRegion contents from aSource into this layer, offsetting
   * them by aDelta. The validity is also copied, so invalid areas in
   * aSource will make corresponding areas of this layer invalid. You
   * must not call this after BeginDrawing/EndDrawing on this layer.
   * 
   * aSource must be this layer or a layer after this layer in a
   * preorder traversal of the layer tree.
   */
  virtual void CopyFrom(ThebesLayer* aSource,
                        const nsIntRegion& aRegion,
                        const nsIntPoint& aDelta) = 0;

protected:
  ThebesLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData) {}
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

  // This getter can be used anytime.
  virtual Layer* GetFirstChild() { return mFirstChild; }

protected:
  ContainerLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData),
      mFirstChild(nsnull)
  {}

  Layer* mFirstChild;
};

}
}

#endif /* GFX_LAYERS_H */
