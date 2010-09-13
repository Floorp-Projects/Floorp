/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "gfxCachedTempSurface.h"
#include "nsAutoRef.h"
#include "nsThreadUtils.h"

#ifdef MOZ_IPC
#include "mozilla/layers/ShadowLayers.h"
#endif

class nsIWidget;

namespace mozilla {
namespace layers {

class BasicShadowableLayer;
class ShadowThebesLayer;
class ShadowImageLayer;
class ShadowCanvasLayer;

/**
 * This is a cairo/Thebes-only, main-thread-only implementation of layers.
 * 
 * In each transaction, the client sets up the layer tree and then during
 * the drawing phase, each ThebesLayer is painted directly into the target
 * context (with appropriate clipping and Push/PopGroups performed
 * between layers).
 */
class THEBES_API BasicLayerManager :
#ifdef MOZ_IPC
    public ShadowLayerManager
#else
    public LayerManager
#endif
{
public:
  /**
   * Construct a BasicLayerManager which will have no default
   * target context. SetDefaultTarget or BeginTransactionWithTarget
   * must be called for any rendering to happen. ThebesLayers will not
   * be retained.
   */
  BasicLayerManager();
  /**
   * Construct a BasicLayerManager which will have no default
   * target context. SetDefaultTarget or BeginTransactionWithTarget
   * must be called for any rendering to happen. ThebesLayers will be
   * retained; that is, we will try to retain the visible contents of
   * ThebesLayers as cairo surfaces. We create ThebesLayer buffers by
   * creating similar surfaces to the default target context, or to
   * aWidget's GetThebesSurface if there is no default target context, or
   * to the passed-in context if there is no widget and no default
   * target context.
   * 
   * This does not keep a strong reference to the widget, so the caller
   * must ensure that the widget outlives the layer manager or call
   * ClearWidget before the widget dies.
   */
  BasicLayerManager(nsIWidget* aWidget);
  virtual ~BasicLayerManager();

  /**
   * Set the default target context that will be used when BeginTransaction
   * is called. This can only be called outside a transaction.
   * 
   * aDoubleBuffering can request double-buffering for drawing to the
   * default target. When BUFFERED, the layer manager avoids blitting
   * temporary results to aContext and then overpainting them with final
   * results, by using a temporary buffer when necessary. In BUFFERED
   * mode we always completely overwrite the contents of aContext's
   * destination surface (within the clip region) using OPERATOR_SOURCE.
   */
  enum BufferMode {
    BUFFER_NONE,
    BUFFER_BUFFERED
  };
  void SetDefaultTarget(gfxContext* aContext, BufferMode aDoubleBuffering);
  gfxContext* GetDefaultTarget() { return mDefaultTarget; }

  nsIWidget* GetRetainerWidget() { return mWidget; }
  void ClearRetainerWidget() { mWidget = nsnull; }

  virtual void BeginTransaction();
  virtual void BeginTransactionWithTarget(gfxContext* aTarget);
  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData);

  virtual void SetRoot(Layer* aLayer);

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();
  virtual already_AddRefed<ImageLayer> CreateImageLayer();
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();
  virtual already_AddRefed<ImageContainer> CreateImageContainer();
  virtual already_AddRefed<ColorLayer> CreateColorLayer();
  virtual already_AddRefed<ShadowThebesLayer> CreateShadowThebesLayer()
  { return NULL; }
  virtual already_AddRefed<ShadowImageLayer> CreateShadowImageLayer()
  { return NULL; }
  virtual already_AddRefed<ShadowCanvasLayer> CreateShadowCanvasLayer()
  { return NULL; }

  virtual LayersBackend GetBackendType() { return LAYERS_BASIC; }
  virtual void GetBackendName(nsAString& name) { name.AssignLiteral("Basic"); }

#ifdef DEBUG
  PRBool InConstruction() { return mPhase == PHASE_CONSTRUCTION; }
  PRBool InDrawing() { return mPhase == PHASE_DRAWING; }
  PRBool InForward() { return mPhase == PHASE_FORWARD; }
  PRBool InTransaction() { return mPhase != PHASE_NONE; }
#endif
  gfxContext* GetTarget() { return mTarget; }
  PRBool IsRetained() { return mWidget != nsnull; }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() const { return "Basic"; }
#endif // MOZ_LAYERS_HAVE_LOG

  // Clear the cached contents of this layer.
  void ClearCachedResources();

protected:
#ifdef DEBUG
  enum TransactionPhase {
    PHASE_NONE, PHASE_CONSTRUCTION, PHASE_DRAWING, PHASE_FORWARD
  };
  TransactionPhase mPhase;
#endif

private:
  // Paints aLayer to mTarget.
  void PaintLayer(Layer* aLayer,
                  DrawThebesLayerCallback aCallback,
                  void* aCallbackData,
                  float aOpacity);

  // Clear the contents of a layer
  void ClearLayer(Layer* aLayer);

  already_AddRefed<gfxContext> PushGroupWithCachedSurface(gfxContext *aTarget,
                                                          gfxASurface::gfxContentType aContent,
                                                          gfxPoint *aSavedOffset);
  void PopGroupWithCachedSurface(gfxContext *aTarget,
                                 const gfxPoint& aSavedOffset);

  // Widget whose surface should be used as the basis for ThebesLayer
  // buffers.
  nsIWidget* mWidget;
  // The default context for BeginTransaction.
  nsRefPtr<gfxContext> mDefaultTarget;
  // The context to draw into.
  nsRefPtr<gfxContext> mTarget;

  // Cached surface for double buffering
  gfxCachedTempSurface mCachedSurface;

  BufferMode   mDoubleBuffering;
  PRPackedBool mUsingDefaultTarget;
};
 

#ifdef MOZ_IPC
class BasicShadowLayerManager : public BasicLayerManager,
                                public ShadowLayerForwarder
{
  typedef nsTArray<nsRefPtr<Layer> > LayerRefArray;

public:
  BasicShadowLayerManager(nsIWidget* aWidget);
  virtual ~BasicShadowLayerManager();

  virtual void BeginTransactionWithTarget(gfxContext* aTarget);
  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData);

  virtual void SetRoot(Layer* aLayer);

  virtual void Mutated(Layer* aLayer);

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();
  virtual already_AddRefed<ImageLayer> CreateImageLayer();
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();
  virtual already_AddRefed<ColorLayer> CreateColorLayer();
  virtual already_AddRefed<ShadowThebesLayer> CreateShadowThebesLayer();
  virtual already_AddRefed<ShadowImageLayer> CreateShadowImageLayer();
  virtual already_AddRefed<ShadowCanvasLayer> CreateShadowCanvasLayer();

  virtual const char* Name() const { return "BasicShadowLayerManager"; }

  ShadowableLayer* Hold(Layer* aLayer);

private:
  LayerRefArray mKeepAlive;
};
#endif  // MOZ_IPC

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
