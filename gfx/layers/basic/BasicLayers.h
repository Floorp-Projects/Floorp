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

#ifndef GFX_BASICLAYERS_H
#define GFX_BASICLAYERS_H

#include "Layers.h"

#include "gfxContext.h"
#include "nsAutoRef.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

class BasicThebesLayer;

/**
 * This is a cairo/Thebes-only, main-thread-only implementation of layers.
 * Currently it only supports immediate mode but we will probably
 * extend it to support retained buffers. In other words, currently,
 * no buffers are retained between transactions.
 * 
 * In each transaction, the client sets up the layer tree and then during
 * the drawing phase, each ThebesLayer is painted directly into the target
 * context (with appropriate clipping and Push/PopGroups performed
 * between layers).
 */
class THEBES_API BasicLayerManager : public LayerManager {
public:
  /**
   * Construct a BasicLayerManager which will render to aContext when
   * BeginTransaction is called. This can be null, in which case
   * transactions started with BeginTransaction will not do any painting.
   */
  BasicLayerManager(gfxContext* aContext);
  virtual ~BasicLayerManager();

  /**
   * Set the default target context that will be used when BeginTransaction
   * is called. This can only be called outside a transaction.
   */
  void SetDefaultTarget(gfxContext* aContext);

  virtual void BeginTransaction();
  virtual void BeginTransactionWithTarget(gfxContext* aTarget);
  virtual void EndConstruction();
  virtual void EndTransaction();

  virtual void SetRoot(Layer* aLayer);

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();
  virtual already_AddRefed<ImageLayer> CreateImageLayer();
  virtual already_AddRefed<ImageContainer> CreateImageContainer();

#ifdef DEBUG
  PRBool InConstruction() { return mPhase == PHASE_CONSTRUCTION; }
  PRBool InDrawing() { return mPhase == PHASE_DRAWING; }
  PRBool IsBeforeInTree(Layer* aBefore, Layer* aLayer);
#endif
  // Prepares mTarget for painting into aLayer. Layers are painted
  // in tree order, so this method essentially traverses the layer tree
  // in preorder from mLastPainted to aLayer, doing whatever's needed
  // as we exit container layers and enter new container layers, and
  // drawing any non-ThebesLayers we encounter.
  void AdvancePaintingTo(BasicThebesLayer* aLayer);
  Layer* GetLastPainted() { return mLastPainted; }
  gfxContext* GetTarget() { return mTarget; }

private:
  // This gets called when we start painting aLayer. It can change
  // the state of mTarget by saving state, setting clipping and/or
  // pushing a group.
  void BeginPaintingLayer(Layer* aLayer);
  // This gets called when we finish painting aLayer. It can change
  // the state of mTarget by popping a group and/or restoring the state.
  void EndPaintingLayer();

  nsRefPtr<Layer> mRoot;
  // The default context for BeginTransaction.
  nsRefPtr<gfxContext> mDefaultTarget;
  // The context to draw into. This is always the context that
  // our ThebesLayers will return.
  nsRefPtr<gfxContext> mTarget;
  // The most recently painted layer during the drawing phase of
  // a transaction, or null if no layer has been painted in this
  // transaction.
  Layer* mLastPainted;

#ifdef DEBUG
  enum TransactionPhase { PHASE_NONE, PHASE_CONSTRUCTION, PHASE_DRAWING };
  TransactionPhase mPhase;
#endif
};

}
}

/**
 * We need to be able to hold a reference to a gfxASurface from Image
 * subclasses. This is potentially a problem since Images can be addrefed
 * or released off the main thread. We can ensure that we never AddRef
 * a gfxASurface off the main thread, but we might want to Release due
 * to an Image being destroyed off the main thread.
 * 
 * We use nsCountedRef<nsMainThreadSurfaceRef> to reference the
 * gfxASurface. When AddRefing, we assert that we're on the main thread.
 * When Releasing, if we're not on the main thread, we post an event to
 * the main thread to do the actual release.
 */
class nsMainThreadSurfaceRef;

NS_SPECIALIZE_TEMPLATE
class nsAutoRefTraits<nsMainThreadSurfaceRef> {
public:
  typedef gfxASurface* RawRef;

  /**
   * The XPCOM event that will do the actual release on the main thread.
   */
  class SurfaceReleaser : public nsRunnable {
  public:
    SurfaceReleaser(RawRef aRef) : mRef(aRef) {}
    NS_IMETHOD Run() {
      mRef->Release();
      return NS_OK;
    }
    RawRef mRef;
  };

  static RawRef Void() { return nsnull; }
  static void Release(RawRef aRawRef)
  {
    if (NS_IsMainThread()) {
      aRawRef->Release();
      return;
    }
    nsCOMPtr<nsIRunnable> runnable = new SurfaceReleaser(aRawRef);
    NS_DispatchToMainThread(runnable);
  }
  static void AddRef(RawRef aRawRef)
  {
    NS_ASSERTION(NS_IsMainThread(),
                 "Can only add a reference on the main thread");
    aRawRef->AddRef();
  }
};

#endif /* GFX_BASICLAYERS_H */
