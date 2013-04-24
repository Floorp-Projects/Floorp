/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICLAYERS_H
#define GFX_BASICLAYERS_H

#include "Layers.h"

#include "gfxContext.h"
#include "gfxCachedTempSurface.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/WidgetUtils.h"
#include "nsAutoRef.h"
#include "nsThreadUtils.h"

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
class ImageFactory;
class PaintLayerContext;

/**
 * This is a cairo/Thebes-only, main-thread-only implementation of layers.
 * 
 * In each transaction, the client sets up the layer tree and then during
 * the drawing phase, each ThebesLayer is painted directly into the target
 * context (with appropriate clipping and Push/PopGroups performed
 * between layers).
 */
class THEBES_API BasicLayerManager :
    public LayerManager
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
  void SetDefaultTarget(gfxContext* aContext);
  virtual void SetDefaultTargetConfiguration(BufferMode aDoubleBuffering, ScreenRotation aRotation);
  gfxContext* GetDefaultTarget() { return mDefaultTarget; }

  nsIWidget* GetRetainerWidget() { return mWidget; }
  void ClearRetainerWidget() { mWidget = nullptr; }

  virtual bool IsWidgetLayerManager() { return mWidget != nullptr; }

  virtual void BeginTransaction();
  virtual void BeginTransactionWithTarget(gfxContext* aTarget);
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT);
  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT);
  virtual bool AreComponentAlphaLayersEnabled() { return HasShadowManager() || !IsWidgetLayerManager(); }

  void AbortTransaction();

  virtual void SetRoot(Layer* aLayer);

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();
  virtual already_AddRefed<ImageLayer> CreateImageLayer();
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();
  virtual already_AddRefed<ColorLayer> CreateColorLayer();
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer();
  virtual ImageFactory *GetImageFactory();

  virtual LayersBackend GetBackendType() { return LAYERS_BASIC; }
  virtual void GetBackendName(nsAString& name) { name.AssignLiteral("Basic"); }

#ifdef DEBUG
  bool InConstruction() { return mPhase == PHASE_CONSTRUCTION; }
  bool InDrawing() { return mPhase == PHASE_DRAWING; }
  bool InForward() { return mPhase == PHASE_FORWARD; }
#endif
  bool InTransaction() { return mPhase != PHASE_NONE; }

  gfxContext* GetTarget() { return mTarget; }
  void SetTarget(gfxContext* aTarget) { mUsingDefaultTarget = false; mTarget = aTarget; }
  bool IsRetained() { return mWidget != nullptr; }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() const { return "Basic"; }
#endif // MOZ_LAYERS_HAVE_LOG

  // Clear the cached contents of this layer tree.
  virtual void ClearCachedResources(Layer* aSubtree = nullptr) MOZ_OVERRIDE;

  void SetTransactionIncomplete() { mTransactionIncomplete = true; }
  bool IsTransactionIncomplete() { return mTransactionIncomplete; }

  already_AddRefed<gfxContext> PushGroupForLayer(gfxContext* aContext, Layer* aLayer,
                                                 const nsIntRegion& aRegion,
                                                 bool* aNeedsClipToVisibleRegion);
  already_AddRefed<gfxContext> PushGroupWithCachedSurface(gfxContext *aTarget,
                                                          gfxASurface::gfxContentType aContent);
  void PopGroupToSourceWithCachedSurface(gfxContext *aTarget, gfxContext *aPushed);

  virtual bool IsCompositingCheap() { return false; }
  virtual int32_t GetMaxTextureSize() const { return INT32_MAX; }
  bool CompositorMightResample() { return mCompositorMightResample; }
  bool HasShadowTarget() { return !!mShadowTarget; }

protected:
  enum TransactionPhase {
    PHASE_NONE, PHASE_CONSTRUCTION, PHASE_DRAWING, PHASE_FORWARD
  };
  TransactionPhase mPhase;

  // This is the main body of the PaintLayer routine which will if it has
  // children, recurse into PaintLayer() otherwise it will paint using the
  // underlying Paint() method of the Layer. It will not do both.
  void PaintSelfOrChildren(PaintLayerContext& aPaintContext, gfxContext* aGroupTarget);

  // Paint the group onto the underlying target. This is used by PaintLayer to
  // flush the group to the underlying target.
  void FlushGroup(PaintLayerContext& aPaintContext, bool aNeedsClipToVisibleRegion);

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

  void FlashWidgetUpdateArea(gfxContext* aContext);

  // Widget whose surface should be used as the basis for ThebesLayer
  // buffers.
  nsIWidget* mWidget;
  // The default context for BeginTransaction.
  nsRefPtr<gfxContext> mDefaultTarget;
  // The context to draw into.
  nsRefPtr<gfxContext> mTarget;
  // When we're doing a transaction in order to draw to a non-default
  // target, the layers transaction is only performed in order to send
  // a PLayerTransaction:Update.  We save the original non-default target to
  // mShadowTarget, and then perform the transaction using
  // mDummyTarget as the render target.  After the transaction ends,
  // we send a message to our remote side to capture the actual pixels
  // being drawn to the default target, and then copy those pixels
  // back to mShadowTarget.
  nsRefPtr<gfxContext> mShadowTarget;
  nsRefPtr<gfxContext> mDummyTarget;
  // Image factory we use.
  nsRefPtr<ImageFactory> mFactory;

  // Cached surface for double buffering
  gfxCachedTempSurface mCachedSurface;

  BufferMode mDoubleBuffering;
  bool mUsingDefaultTarget;
  bool mCachedSurfaceInUse;
  bool mTransactionIncomplete;
  bool mCompositorMightResample;
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

  virtual int32_t GetMaxTextureSize() const;

  virtual void SetDefaultTargetConfiguration(BufferMode aDoubleBuffering, ScreenRotation aRotation) MOZ_OVERRIDE;
  virtual void BeginTransactionWithTarget(gfxContext* aTarget);
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT);
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
  virtual already_AddRefed<RefLayer> CreateRefLayer();

  ShadowableLayer* Hold(Layer* aLayer);

  bool HasShadowManager() const { return ShadowLayerForwarder::HasShadowManager(); }

  virtual bool IsCompositingCheap();
  virtual bool HasShadowManagerInternal() const { return HasShadowManager(); }

  virtual void SetIsFirstPaint() MOZ_OVERRIDE;

  // Drop cached resources and ask our shadow manager to do the same,
  // if we have one.
  virtual void ClearCachedResources(Layer* aSubtree = nullptr) MOZ_OVERRIDE;

  void SetRepeatTransaction() { mRepeatTransaction = true; }
  bool GetRepeatTransaction() { return mRepeatTransaction; }

  bool IsRepeatTransaction() { return mIsRepeatTransaction; }

  /**
   * Called for each iteration of a progressive tile update. Fills
   * aViewport, aScaleX and aScaleY with the current scale and viewport
   * being used to composite the layers in this manager, to determine what area
   * intersects with the target render rectangle. aDrawingCritical will be
   * true if the current drawing operation is using the critical displayport.
   * Returns true if the update should continue, or false if it should be
   * cancelled.
   * This is only called if gfxPlatform::UseProgressiveTilePainting() returns
   * true.
   */
  bool ProgressiveUpdateCallback(bool aHasPendingNewThebesContent,
                                 gfx::Rect& aViewport,
                                 float& aScaleX,
                                 float& aScaleY,
                                 bool aDrawingCritical);

private:
  /**
   * Forward transaction results to the parent context.
   */
  void ForwardTransaction();

  // The bounds of |mTarget| in device pixels.
  nsIntRect mTargetBounds;

  LayerRefArray mKeepAlive;

  // Sometimes we draw to targets that don't natively support
  // landscape/portrait orientation.  When we need to implement that
  // ourselves, |mTargetRotation| describes the induced transform we
  // need to apply when compositing content to our target.
  ScreenRotation mTargetRotation;

  // Used to repeat the transaction right away (to avoid rebuilding
  // a display list) to support progressive drawing.
  bool mRepeatTransaction;
  bool mIsRepeatTransaction;
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

  virtual void Disconnect()
  {
    // This is an "emergency Disconnect()", called when the compositing
    // process has died.  |mShadow| and our Shmem buffers are
    // automatically managed by IPDL, so we don't need to explicitly
    // free them here (it's hard to get that right on emergency
    // shutdown anyway).
    mShadow = nullptr;
  }

  virtual BasicShadowableThebesLayer* AsThebes() { return nullptr; }
};

void
PaintContext(gfxPattern* aPattern,
             const nsIntRegion& aVisible,
             float aOpacity,
             gfxContext* aContext,
             Layer* aMaskLayer);

}
}

#endif /* GFX_BASICLAYERS_H */
