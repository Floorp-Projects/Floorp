/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICLAYERS_H
#define GFX_BASICLAYERS_H

#include <stdint.h>                     // for INT32_MAX, int32_t
#include "Layers.h"                     // for Layer (ptr only), etc
#include "gfxTypes.h"
#include "gfxContext.h"                 // for gfxContext
#include "mozilla/Attributes.h"         // for override
#include "mozilla/WidgetUtils.h"        // for ScreenRotation
#include "mozilla/layers/LayersTypes.h"  // for BufferMode, LayersBackend, etc
#include "nsAString.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for gfxContext::AddRef, etc
#include "nsRegion.h"                   // for nsIntRegion
#include "nscore.h"                     // for nsAString, etc

class nsIWidget;

namespace mozilla {
namespace layers {

class ImageFactory;
class ImageLayer;
class PaintLayerContext;
class ReadbackLayer;

/**
 * This is a cairo/Thebes-only, main-thread-only implementation of layers.
 * 
 * In each transaction, the client sets up the layer tree and then during
 * the drawing phase, each PaintedLayer is painted directly into the target
 * context (with appropriate clipping and Push/PopGroups performed
 * between layers).
 */
class BasicLayerManager final :
    public LayerManager
{
public:
  enum BasicLayerManagerType {
    BLM_WIDGET,
    BLM_OFFSCREEN,
    BLM_INACTIVE
  };
  /**
   * Construct a BasicLayerManager which will have no default
   * target context. SetDefaultTarget or BeginTransactionWithTarget
   * must be called for any rendering to happen. PaintedLayers will not
   * be retained.
   */
  explicit BasicLayerManager(BasicLayerManagerType aType);
  /**
   * Construct a BasicLayerManager which will have no default
   * target context. SetDefaultTarget or BeginTransactionWithTarget
   * must be called for any rendering to happen. PaintedLayers will be
   * retained; that is, we will try to retain the visible contents of
   * PaintedLayers as cairo surfaces. We create PaintedLayer buffers by
   * creating similar surfaces to the default target context, or to
   * aWidget's GetThebesSurface if there is no default target context, or
   * to the passed-in context if there is no widget and no default
   * target context.
   * 
   * This does not keep a strong reference to the widget, so the caller
   * must ensure that the widget outlives the layer manager or call
   * ClearWidget before the widget dies.
   */
  explicit BasicLayerManager(nsIWidget* aWidget);

protected:
  virtual ~BasicLayerManager();

public:
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

  virtual bool IsWidgetLayerManager() override { return mWidget != nullptr; }
  virtual bool IsInactiveLayerManager() override { return mType == BLM_INACTIVE; }

  virtual void BeginTransaction() override;
  virtual void BeginTransactionWithTarget(gfxContext* aTarget) override;
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override;
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) override;
  virtual bool ShouldAvoidComponentAlphaLayers() override { return IsWidgetLayerManager(); }

  void AbortTransaction();

  virtual void SetRoot(Layer* aLayer) override;

  virtual already_AddRefed<PaintedLayer> CreatePaintedLayer() override;
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() override;
  virtual already_AddRefed<ImageLayer> CreateImageLayer() override;
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() override;
  virtual already_AddRefed<ColorLayer> CreateColorLayer() override;
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer() override;
  virtual ImageFactory *GetImageFactory();

  virtual LayersBackend GetBackendType() override { return LayersBackend::LAYERS_BASIC; }
  virtual void GetBackendName(nsAString& name) override { name.AssignLiteral("Basic"); }

  bool InConstruction() { return mPhase == PHASE_CONSTRUCTION; }
#ifdef DEBUG
  bool InDrawing() { return mPhase == PHASE_DRAWING; }
  bool InForward() { return mPhase == PHASE_FORWARD; }
#endif
  bool InTransaction() { return mPhase != PHASE_NONE; }

  gfxContext* GetTarget() { return mTarget; }
  void SetTarget(gfxContext* aTarget) { mUsingDefaultTarget = false; mTarget = aTarget; }
  bool IsRetained() { return mWidget != nullptr; }

  virtual const char* Name() const override { return "Basic"; }

  // Clear the cached contents of this layer tree.
  virtual void ClearCachedResources(Layer* aSubtree = nullptr) override;

  void SetTransactionIncomplete() { mTransactionIncomplete = true; }
  bool IsTransactionIncomplete() { return mTransactionIncomplete; }

  already_AddRefed<gfxContext> PushGroupForLayer(gfxContext* aContext, Layer* aLayer,
                                                 const nsIntRegion& aRegion,
                                                 bool* aNeedsClipToVisibleRegion);

  virtual bool IsCompositingCheap() override { return false; }
  virtual int32_t GetMaxTextureSize() const override { return INT32_MAX; }
  bool CompositorMightResample() { return mCompositorMightResample; }

  virtual bool SupportsMixBlendModes(EnumSet<gfx::CompositionOp>& aMixBlendModes) override { return true; }

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
                  DrawPaintedLayerCallback aCallback,
                  void* aCallbackData);

  // Clear the contents of a layer
  void ClearLayer(Layer* aLayer);

  bool EndTransactionInternal(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT);

  void FlashWidgetUpdateArea(gfxContext* aContext);

  // Widget whose surface should be used as the basis for PaintedLayer
  // buffers.
  nsIWidget* mWidget;
  // The default context for BeginTransaction.
  nsRefPtr<gfxContext> mDefaultTarget;
  // The context to draw into.
  nsRefPtr<gfxContext> mTarget;
  // Image factory we use.
  nsRefPtr<ImageFactory> mFactory;

  BufferMode mDoubleBuffering;
  BasicLayerManagerType mType;
  bool mUsingDefaultTarget;
  bool mTransactionIncomplete;
  bool mCompositorMightResample;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_BASICLAYERS_H */
