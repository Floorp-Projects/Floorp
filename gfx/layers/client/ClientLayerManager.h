/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CLIENTLAYERMANAGER_H
#define GFX_CLIENTLAYERMANAGER_H

#include <stdint.h>                     // for int32_t
#include "Layers.h"
#include "gfxContext.h"                 // for gfxContext
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/LinkedList.h"         // For LinkedList
#include "mozilla/WidgetUtils.h"        // for ScreenRotation
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersTypes.h"  // for BufferMode, LayersBackend, etc
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ABORT_IF_FALSE
#include "nsISupportsImpl.h"            // for Layer::Release, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsTArray.h"                   // for nsTArray
#include "nscore.h"                     // for nsAString

class nsIWidget;

namespace mozilla {
namespace layers {

class ClientThebesLayer;
class CompositorChild;
class ImageLayer;
class PLayerChild;
class TextureClientPool;
class SimpleTextureClientPool;

class ClientLayerManager : public LayerManager
{
  typedef nsTArray<nsRefPtr<Layer> > LayerRefArray;

public:
  ClientLayerManager(nsIWidget* aWidget);
  virtual ~ClientLayerManager();

  virtual ShadowLayerForwarder* AsShadowForwarder()
  {
    return mForwarder;
  }

  virtual int32_t GetMaxTextureSize() const;

  virtual void SetDefaultTargetConfiguration(BufferMode aDoubleBuffering, ScreenRotation aRotation);
  virtual void BeginTransactionWithTarget(gfxContext* aTarget);
  virtual void BeginTransaction();
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT);
  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT);

  virtual LayersBackend GetBackendType() { return LayersBackend::LAYERS_CLIENT; }
  virtual LayersBackend GetCompositorBackendType() MOZ_OVERRIDE
  {
    return AsShadowForwarder()->GetCompositorBackendType();
  }
  virtual void GetBackendName(nsAString& name);
  virtual const char* Name() const { return "Client"; }

  virtual void SetRoot(Layer* aLayer);

  virtual void Mutated(Layer* aLayer);

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer();
  virtual already_AddRefed<ThebesLayer> CreateThebesLayerWithHint(ThebesLayerCreationHint aHint);
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer();
  virtual already_AddRefed<ImageLayer> CreateImageLayer();
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer();
  virtual already_AddRefed<ColorLayer> CreateColorLayer();
  virtual already_AddRefed<RefLayer> CreateRefLayer();

  TextureFactoryIdentifier GetTextureFactoryIdentifier()
  {
    return mForwarder->GetTextureFactoryIdentifier();
  }

  virtual void FlushRendering() MOZ_OVERRIDE;
  void SendInvalidRegion(const nsIntRegion& aRegion);

  virtual uint32_t StartFrameTimeRecording(int32_t aBufferSize) MOZ_OVERRIDE;

  virtual void StopFrameTimeRecording(uint32_t         aStartIndex,
                                      nsTArray<float>& aFrameIntervals) MOZ_OVERRIDE;

  virtual bool NeedsWidgetInvalidation() MOZ_OVERRIDE { return false; }

  ShadowableLayer* Hold(Layer* aLayer);

  bool HasShadowManager() const { return mForwarder->HasShadowManager(); }

  virtual bool IsCompositingCheap();
  virtual bool HasShadowManagerInternal() const { return HasShadowManager(); }

  virtual void SetIsFirstPaint() MOZ_OVERRIDE;

  TextureClientPool *GetTexturePool(gfx::SurfaceFormat aFormat);
  SimpleTextureClientPool *GetSimpleTileTexturePool(gfx::SurfaceFormat aFormat);

  // Drop cached resources and ask our shadow manager to do the same,
  // if we have one.
  virtual void ClearCachedResources(Layer* aSubtree = nullptr) MOZ_OVERRIDE;

  void SetRepeatTransaction() { mRepeatTransaction = true; }
  bool GetRepeatTransaction() { return mRepeatTransaction; }

  bool IsRepeatTransaction() { return mIsRepeatTransaction; }

  void SetTransactionIncomplete() { mTransactionIncomplete = true; }

  bool HasShadowTarget() { return !!mShadowTarget; }

  void SetShadowTarget(gfxContext *aTarget) { mShadowTarget = aTarget; }

  bool CompositorMightResample() { return mCompositorMightResample; } 
  
  DrawThebesLayerCallback GetThebesLayerCallback() const
  { return mThebesLayerCallback; }

  void* GetThebesLayerCallbackData() const
  { return mThebesLayerCallbackData; }

  CompositorChild *GetRemoteRenderer();

  /**
   * Called for each iteration of a progressive tile update. Fills
   * aCompositionBounds and aZoom with the current scale and composition bounds
   * being used to composite the layers in this manager, to determine what area
   * intersects with the target composition bounds.
   * aDrawingCritical will be true if the current drawing operation is using
   * the critical displayport.
   * Returns true if the update should continue, or false if it should be
   * cancelled.
   * This is only called if gfxPlatform::UseProgressiveTilePainting() returns
   * true.
   */
  bool ProgressiveUpdateCallback(bool aHasPendingNewThebesContent,
                                 ParentLayerRect& aCompositionBounds,
                                 CSSToParentLayerScale& aZoom,
                                 bool aDrawingCritical);

  bool InConstruction() { return mPhase == PHASE_CONSTRUCTION; }
#ifdef DEBUG
  bool InDrawing() { return mPhase == PHASE_DRAWING; }
  bool InForward() { return mPhase == PHASE_FORWARD; }
#endif
  bool InTransaction() { return mPhase != PHASE_NONE; }

  void SetNeedsComposite(bool aNeedsComposite)
  {
    mNeedsComposite = aNeedsComposite;
  }
  bool NeedsComposite() const { return mNeedsComposite; }

  virtual void Composite() MOZ_OVERRIDE;

protected:
  enum TransactionPhase {
    PHASE_NONE, PHASE_CONSTRUCTION, PHASE_DRAWING, PHASE_FORWARD
  };
  TransactionPhase mPhase;

private:
  /**
   * Forward transaction results to the parent context.
   */
  void ForwardTransaction(bool aScheduleComposite);

  /**
   * Take a snapshot of the parent context, and copy
   * it into mShadowTarget.
   */
  void MakeSnapshotIfRequired();

  void ClearLayer(Layer* aLayer);

  bool EndTransactionInternal(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags);

  // The bounds of |mTarget| in device pixels.
  nsIntRect mTargetBounds;

  LayerRefArray mKeepAlive;

  nsIWidget* mWidget;
  
  /* Thebes layer callbacks; valid at the end of a transaciton,
   * while rendering */
  DrawThebesLayerCallback mThebesLayerCallback;
  void *mThebesLayerCallbackData;

  // When we're doing a transaction in order to draw to a non-default
  // target, the layers transaction is only performed in order to send
  // a PLayers:Update.  We save the original non-default target to
  // mShadowTarget, and then perform the transaction using
  // mDummyTarget as the render target.  After the transaction ends,
  // we send a message to our remote side to capture the actual pixels
  // being drawn to the default target, and then copy those pixels
  // back to mShadowTarget.
  nsRefPtr<gfxContext> mShadowTarget;

  // Sometimes we draw to targets that don't natively support
  // landscape/portrait orientation.  When we need to implement that
  // ourselves, |mTargetRotation| describes the induced transform we
  // need to apply when compositing content to our target.
  ScreenRotation mTargetRotation;

  // Used to repeat the transaction right away (to avoid rebuilding
  // a display list) to support progressive drawing.
  bool mRepeatTransaction;
  bool mIsRepeatTransaction;
  bool mTransactionIncomplete;
  bool mCompositorMightResample;
  bool mNeedsComposite;

  RefPtr<ShadowLayerForwarder> mForwarder;
  nsAutoTArray<RefPtr<TextureClientPool>,2> mTexturePools;

  // indexed by gfx::SurfaceFormat
  nsTArray<RefPtr<SimpleTextureClientPool> > mSimpleTilePools;
};

class ClientLayer : public ShadowableLayer
{
public:
  ClientLayer()
  {
    MOZ_COUNT_CTOR(ClientLayer);
  }

  ~ClientLayer();

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

  virtual void ClearCachedResources() { }

  virtual void RenderLayer() = 0;

  virtual ClientThebesLayer* AsThebes() { return nullptr; }

  static inline ClientLayer *
  ToClientLayer(Layer* aLayer)
  {
    return static_cast<ClientLayer*>(aLayer->ImplData());
  }
};

// Create a shadow layer (PLayerChild) for aLayer, if we're forwarding
// our layer tree to a parent process.  Record the new layer creation
// in the current open transaction as a side effect.
template<typename CreatedMethod> void
CreateShadowFor(ClientLayer* aLayer,
                ClientLayerManager* aMgr,
                CreatedMethod aMethod)
{
  PLayerChild* shadow = aMgr->AsShadowForwarder()->ConstructShadowFor(aLayer);
  // XXX error handling
  NS_ABORT_IF_FALSE(shadow, "failed to create shadow");

  aLayer->SetShadow(shadow);
  (aMgr->AsShadowForwarder()->*aMethod)(aLayer);
  aMgr->Hold(aLayer->AsLayer());
}

#define CREATE_SHADOW(_type)                                       \
  CreateShadowFor(layer, this,                                     \
                  &ShadowLayerForwarder::Created ## _type ## Layer)


}
}

#endif /* GFX_CLIENTLAYERMANAGER_H */
