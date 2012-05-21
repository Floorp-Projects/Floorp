/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICLAYERS_H
#define GFX_BASICLAYERS_H

#include "Layers.h"

#include "gfxContext.h"
#include "gfxCachedTempSurface.h"
#include "nsAutoRef.h"
#include "nsThreadUtils.h"

#include "mozilla/layers/ShadowLayers.h"

class nsIWidget;

namespace mozilla {
namespace layers {

class BasicShadowableLayer;
class ShadowThebesLayer;
class ShadowContainerLayer;
class ShadowImageLayer;
class ShadowCanvasLayer;
class ShadowColorLayer;
class ReadbackProcessor;

/**
 * This is a cairo/Thebes-only, main-thread-only implementation of layers.
 * 
 * In each transaction, the client sets up the layer tree and then during
 * the drawing phase, each ThebesLayer is painted directly into the target
 * context (with appropriate clipping and Push/PopGroups performed
 * between layers).
 */
class THEBES_API BasicLayerManager :
    public ShadowLayerManager
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
  virtual bool EndEmptyTransaction();
  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT);

  virtual void SetRoot(Layer* aLayer);

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();
  virtual already_AddRefed<ImageLayer> CreateImageLayer();
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();
  virtual already_AddRefed<ColorLayer> CreateColorLayer();
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer();
  virtual ImageFactory *GetImageFactory();

  virtual already_AddRefed<ShadowThebesLayer> CreateShadowThebesLayer()
  { return nsnull; }
  virtual already_AddRefed<ShadowContainerLayer> CreateShadowContainerLayer()
  { return nsnull; }
  virtual already_AddRefed<ShadowImageLayer> CreateShadowImageLayer()
  { return nsnull; }
  virtual already_AddRefed<ShadowColorLayer> CreateShadowColorLayer()
  { return nsnull; }
  virtual already_AddRefed<ShadowCanvasLayer> CreateShadowCanvasLayer()
  { return nsnull; }

  virtual LayersBackend GetBackendType() { return LAYERS_BASIC; }
  virtual void GetBackendName(nsAString& name) { name.AssignLiteral("Basic"); }

#ifdef DEBUG
  bool InConstruction() { return mPhase == PHASE_CONSTRUCTION; }
  bool InDrawing() { return mPhase == PHASE_DRAWING; }
  bool InForward() { return mPhase == PHASE_FORWARD; }
  bool InTransaction() { return mPhase != PHASE_NONE; }
#endif
  gfxContext* GetTarget() { return mTarget; }
  bool IsRetained() { return mWidget != nsnull; }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() const { return "Basic"; }
#endif // MOZ_LAYERS_HAVE_LOG

  // Clear the cached contents of this layer.
  void ClearCachedResources();

  void SetTransactionIncomplete() { mTransactionIncomplete = true; }
  bool IsTransactionIncomplete() { return mTransactionIncomplete; }

  already_AddRefed<gfxContext> PushGroupForLayer(gfxContext* aContext, Layer* aLayer,
                                                 const nsIntRegion& aRegion,
                                                 bool* aNeedsClipToVisibleRegion);
  already_AddRefed<gfxContext> PushGroupWithCachedSurface(gfxContext *aTarget,
                                                          gfxASurface::gfxContentType aContent);
  void PopGroupToSourceWithCachedSurface(gfxContext *aTarget, gfxContext *aPushed);

  virtual bool IsCompositingCheap() { return false; }
  virtual bool HasShadowManagerInternal() const { return false; }
  bool HasShadowManager() const { return HasShadowManagerInternal(); }

protected:
#ifdef DEBUG
  enum TransactionPhase {
    PHASE_NONE, PHASE_CONSTRUCTION, PHASE_DRAWING, PHASE_FORWARD
  };
  TransactionPhase mPhase;
#endif

  // Paints aLayer to mTarget.
  void PaintLayer(gfxContext* aTarget,
                  Layer* aLayer,
                  DrawThebesLayerCallback aCallback,
                  void* aCallbackData,
                  ReadbackProcessor* aReadback);

  // Clear the contents of a layer
  void ClearLayer(Layer* aLayer);

  bool EndTransactionInternal(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT);

  // Widget whose surface should be used as the basis for ThebesLayer
  // buffers.
  nsIWidget* mWidget;
  // The default context for BeginTransaction.
  nsRefPtr<gfxContext> mDefaultTarget;
  // The context to draw into.
  nsRefPtr<gfxContext> mTarget;
  // Image factory we use.
  nsRefPtr<ImageFactory> mFactory;

  // Cached surface for double buffering
  gfxCachedTempSurface mCachedSurface;

  BufferMode   mDoubleBuffering;
  bool mUsingDefaultTarget;
  bool mCachedSurfaceInUse;
  bool         mTransactionIncomplete;
};
 

class BasicShadowLayerManager : public BasicLayerManager,
                                public ShadowLayerForwarder
{
  typedef nsTArray<nsRefPtr<Layer> > LayerRefArray;

public:
  BasicShadowLayerManager(nsIWidget* aWidget);
  virtual ~BasicShadowLayerManager();

  virtual ShadowLayerForwarder* AsShadowForwarder()
  {
    return this;
  }
  virtual ShadowLayerManager* AsShadowManager()
  {
    return this;
  }

  virtual void BeginTransactionWithTarget(gfxContext* aTarget);
  virtual bool EndEmptyTransaction();
  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT);

  virtual void SetRoot(Layer* aLayer);

  virtual void Mutated(Layer* aLayer);

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();
  virtual already_AddRefed<ImageLayer> CreateImageLayer();
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();
  virtual already_AddRefed<ColorLayer> CreateColorLayer();
  virtual already_AddRefed<ShadowThebesLayer> CreateShadowThebesLayer();
  virtual already_AddRefed<ShadowContainerLayer> CreateShadowContainerLayer();
  virtual already_AddRefed<ShadowImageLayer> CreateShadowImageLayer();
  virtual already_AddRefed<ShadowColorLayer> CreateShadowColorLayer();
  virtual already_AddRefed<ShadowCanvasLayer> CreateShadowCanvasLayer();

  ShadowableLayer* Hold(Layer* aLayer);

  bool HasShadowManager() const { return ShadowLayerForwarder::HasShadowManager(); }

  virtual bool IsCompositingCheap();
  virtual bool HasShadowManagerInternal() const { return HasShadowManager(); }

  virtual void SetIsFirstPaint() MOZ_OVERRIDE;

private:
  /**
   * Forward transaction results to the parent context.
   */
  void ForwardTransaction();

  LayerRefArray mKeepAlive;
};

class BasicShadowableThebesLayer;
class BasicShadowableLayer : public ShadowableLayer
{
public:
  BasicShadowableLayer()
  {
    MOZ_COUNT_CTOR(BasicShadowableLayer);
  }

  ~BasicShadowableLayer();

  void SetShadow(PLayerChild* aShadow)
  {
    NS_ABORT_IF_FALSE(!mShadow, "can't have two shadows (yet)");
    mShadow = aShadow;
  }

  virtual void SetBackBuffer(const SurfaceDescriptor& aBuffer)
  {
    NS_RUNTIMEABORT("if this default impl is called, |aBuffer| leaks");
  }
  
  virtual void SetBackBufferYUVImage(gfxSharedImageSurface* aYBuffer,
                                     gfxSharedImageSurface* aUBuffer,
                                     gfxSharedImageSurface* aVBuffer)
  {
    NS_RUNTIMEABORT("if this default impl is called, |aBuffer| leaks");
  }

  virtual void Disconnect()
  {
    // This is an "emergency Disconnect()", called when the compositing
    // process has died.  |mShadow| and our Shmem buffers are
    // automatically managed by IPDL, so we don't need to explicitly
    // free them here (it's hard to get that right on emergency
    // shutdown anyway).
    mShadow = nsnull;
  }

  virtual BasicShadowableThebesLayer* AsThebes() { return nsnull; }
};


}
}

#endif /* GFX_BASICLAYERS_H */
