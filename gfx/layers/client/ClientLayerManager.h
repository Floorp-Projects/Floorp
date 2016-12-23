/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CLIENTLAYERMANAGER_H
#define GFX_CLIENTLAYERMANAGER_H

#include <stdint.h>                     // for int32_t
#include "Layers.h"
#include "gfxContext.h"                 // for gfxContext
#include "mozilla/Attributes.h"         // for override
#include "mozilla/LinkedList.h"         // For LinkedList
#include "mozilla/WidgetUtils.h"        // for ScreenRotation
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersTypes.h"  // for BufferMode, LayersBackend, etc
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder, etc
#include "mozilla/layers/APZTestData.h" // for APZTestData
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsIObserver.h"                // for nsIObserver
#include "nsISupportsImpl.h"            // for Layer::Release, etc
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsTArray.h"                   // for nsTArray
#include "nscore.h"                     // for nsAString
#include "mozilla/layers/TransactionIdAllocator.h"
#include "nsIWidget.h"                  // For plugin window configuration information structs

namespace mozilla {
namespace layers {

class ClientPaintedLayer;
class CompositorBridgeChild;
class ImageLayer;
class PLayerChild;
class FrameUniformityData;

class ClientLayerManager final : public LayerManager
{
  typedef nsTArray<RefPtr<Layer> > LayerRefArray;

public:
  explicit ClientLayerManager(nsIWidget* aWidget);

  virtual void Destroy() override;

protected:
  virtual ~ClientLayerManager();

public:
  virtual ShadowLayerForwarder* AsShadowForwarder() override
  {
    return mForwarder;
  }

  virtual ClientLayerManager* AsClientLayerManager() override
  {
    return this;
  }

  virtual int32_t GetMaxTextureSize() const override;

  virtual void SetDefaultTargetConfiguration(BufferMode aDoubleBuffering, ScreenRotation aRotation);
  virtual bool BeginTransactionWithTarget(gfxContext* aTarget) override;
  virtual bool BeginTransaction() override;
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override;
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) override;

  virtual LayersBackend GetBackendType() override { return LayersBackend::LAYERS_CLIENT; }
  virtual LayersBackend GetCompositorBackendType() override
  {
    return AsShadowForwarder()->GetCompositorBackendType();
  }
  virtual void GetBackendName(nsAString& name) override;
  virtual const char* Name() const override { return "Client"; }

  virtual void SetRoot(Layer* aLayer) override;

  virtual void Mutated(Layer* aLayer) override;

  virtual already_AddRefed<PaintedLayer> CreatePaintedLayer() override;
  virtual already_AddRefed<PaintedLayer> CreatePaintedLayerWithHint(PaintedLayerCreationHint aHint) override;
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() override;
  virtual already_AddRefed<ImageLayer> CreateImageLayer() override;
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() override;
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer() override;
  virtual already_AddRefed<ColorLayer> CreateColorLayer() override;
  virtual already_AddRefed<TextLayer> CreateTextLayer() override;
  virtual already_AddRefed<BorderLayer> CreateBorderLayer() override;
  virtual already_AddRefed<RefLayer> CreateRefLayer() override;

  void UpdateTextureFactoryIdentifier(const TextureFactoryIdentifier& aNewIdentifier);
  TextureFactoryIdentifier GetTextureFactoryIdentifier()
  {
    return AsShadowForwarder()->GetTextureFactoryIdentifier();
  }

  virtual void FlushRendering() override;
  void SendInvalidRegion(const nsIntRegion& aRegion);

  virtual uint32_t StartFrameTimeRecording(int32_t aBufferSize) override;

  virtual void StopFrameTimeRecording(uint32_t         aStartIndex,
                                      nsTArray<float>& aFrameIntervals) override;

  virtual bool NeedsWidgetInvalidation() override { return false; }

  ShadowableLayer* Hold(Layer* aLayer);

  bool HasShadowManager() const { return mForwarder->HasShadowManager(); }

  virtual bool IsCompositingCheap() override;
  virtual bool HasShadowManagerInternal() const override { return HasShadowManager(); }

  virtual void SetIsFirstPaint() override;

  /**
   * Pass through call to the forwarder for nsPresContext's
   * CollectPluginGeometryUpdates. Passes widget configuration information
   * to the compositor for transmission to the chrome process. This
   * configuration gets set when the window paints.
   */
  void StorePluginWidgetConfigurations(const nsTArray<nsIWidget::Configuration>&
                                       aConfigurations) override;

  // Drop cached resources and ask our shadow manager to do the same,
  // if we have one.
  virtual void ClearCachedResources(Layer* aSubtree = nullptr) override;

  void HandleMemoryPressure();

  void SetRepeatTransaction() { mRepeatTransaction = true; }
  bool GetRepeatTransaction() { return mRepeatTransaction; }

  bool IsRepeatTransaction() { return mIsRepeatTransaction; }

  void SetTransactionIncomplete() { mTransactionIncomplete = true; }

  bool HasShadowTarget() { return !!mShadowTarget; }

  void SetShadowTarget(gfxContext* aTarget) { mShadowTarget = aTarget; }

  bool CompositorMightResample() { return mCompositorMightResample; } 
  
  DrawPaintedLayerCallback GetPaintedLayerCallback() const
  { return mPaintedLayerCallback; }

  void* GetPaintedLayerCallbackData() const
  { return mPaintedLayerCallbackData; }

  CompositorBridgeChild* GetRemoteRenderer();

  CompositorBridgeChild* GetCompositorBridgeChild();

  // Disable component alpha layers with the software compositor.
  virtual bool ShouldAvoidComponentAlphaLayers() override { return !IsCompositingCheap(); }

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

  virtual void Composite() override;
  virtual void GetFrameUniformity(FrameUniformityData* aFrameUniformityData) override;
  virtual bool RequestOverfill(mozilla::dom::OverfillCallback* aCallback) override;
  virtual void RunOverfillCallback(const uint32_t aOverfill) override;

  void DidComposite(uint64_t aTransactionId,
                    const mozilla::TimeStamp& aCompositeStart,
                    const mozilla::TimeStamp& aCompositeEnd);

  virtual bool AreComponentAlphaLayersEnabled() override;

  // Log APZ test data for the current paint. We supply the paint sequence
  // number ourselves, and take care of calling APZTestData::StartNewPaint()
  // when a new paint is started.
  void LogTestDataForCurrentPaint(FrameMetrics::ViewID aScrollId,
                                  const std::string& aKey,
                                  const std::string& aValue)
  {
    mApzTestData.LogTestDataForPaint(mPaintSequenceNumber, aScrollId, aKey, aValue);
  }

  // Log APZ test data for a repaint request. The sequence number must be
  // passed in from outside, and APZTestData::StartNewRepaintRequest() needs
  // to be called from the outside as well when a new repaint request is started.
  void StartNewRepaintRequest(SequenceNumber aSequenceNumber);

  // TODO(botond): When we start using this and write a wrapper similar to
  // nsLayoutUtils::LogTestDataForPaint(), make sure that wrapper checks
  // gfxPrefs::APZTestLoggingEnabled().
  void LogTestDataForRepaintRequest(SequenceNumber aSequenceNumber,
                                    FrameMetrics::ViewID aScrollId,
                                    const std::string& aKey,
                                    const std::string& aValue)
  {
    mApzTestData.LogTestDataForRepaintRequest(aSequenceNumber, aScrollId, aKey, aValue);
  }

  // Get the content-side APZ test data for reading. For writing, use the
  // LogTestData...() functions.
  const APZTestData& GetAPZTestData() const {
    return mApzTestData;
  }

  // Get a copy of the compositor-side APZ test data for our layers ID.
  void GetCompositorSideAPZTestData(APZTestData* aData) const;

  void SetTransactionIdAllocator(TransactionIdAllocator* aAllocator) { mTransactionIdAllocator = aAllocator; }

  float RequestProperty(const nsAString& aProperty) override;

  bool AsyncPanZoomEnabled() const override;

  void SetNextPaintSyncId(int32_t aSyncId);

  void SetLayerObserverEpoch(uint64_t aLayerObserverEpoch);

  class DidCompositeObserver {
  public:
    virtual void DidComposite() = 0;
  };

  void AddDidCompositeObserver(DidCompositeObserver* aObserver);
  void RemoveDidCompositeObserver(DidCompositeObserver* aObserver);

  virtual already_AddRefed<PersistentBufferProvider>
  CreatePersistentBufferProvider(const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat) override;

protected:
  enum TransactionPhase {
    PHASE_NONE, PHASE_CONSTRUCTION, PHASE_DRAWING, PHASE_FORWARD
  };
  TransactionPhase mPhase;

private:
  // Listen memory-pressure event for ClientLayerManager
  class MemoryPressureObserver final : public nsIObserver
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    explicit MemoryPressureObserver(ClientLayerManager* aClientLayerManager)
      : mClientLayerManager(aClientLayerManager)
    {
      RegisterMemoryPressureEvent();
    }

    void Destroy();

  private:
    virtual ~MemoryPressureObserver() {}
    void RegisterMemoryPressureEvent();
    void UnregisterMemoryPressureEvent();

    ClientLayerManager* mClientLayerManager;
  };

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

  void HandleMemoryPressureLayer(Layer* aLayer);

  bool EndTransactionInternal(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags);

  bool DependsOnStaleDevice() const;

  LayerRefArray mKeepAlive;

  nsIWidget* mWidget;

  /* PaintedLayer callbacks; valid at the end of a transaciton,
   * while rendering */
  DrawPaintedLayerCallback mPaintedLayerCallback;
  void *mPaintedLayerCallbackData;

  // When we're doing a transaction in order to draw to a non-default
  // target, the layers transaction is only performed in order to send
  // a PLayers:Update.  We save the original non-default target to
  // mShadowTarget, and then perform the transaction using
  // mDummyTarget as the render target.  After the transaction ends,
  // we send a message to our remote side to capture the actual pixels
  // being drawn to the default target, and then copy those pixels
  // back to mShadowTarget.
  RefPtr<gfxContext> mShadowTarget;

  RefPtr<TransactionIdAllocator> mTransactionIdAllocator;
  uint64_t mLatestTransactionId;
  TimeDuration mLastPaintTime;

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

  // An incrementing sequence number for paints.
  // Incremented in BeginTransaction(), but not for repeat transactions.
  uint32_t mPaintSequenceNumber;

  APZTestData mApzTestData;

  RefPtr<ShadowLayerForwarder> mForwarder;
  AutoTArray<dom::OverfillCallback*,0> mOverfillCallbacks;
  mozilla::TimeStamp mTransactionStart;

  nsTArray<DidCompositeObserver*> mDidCompositeObservers;

  RefPtr<MemoryPressureObserver> mMemoryPressureObserver;
  uint64_t mDeviceCounter;
};

class ClientLayer : public ShadowableLayer
{
public:
  ClientLayer()
  {
    MOZ_COUNT_CTOR(ClientLayer);
  }

  ~ClientLayer();

  virtual void ClearCachedResources() { }

  // Shrink memory usage.
  // Called when "memory-pressure" is observed.
  virtual void HandleMemoryPressure() { }

  virtual void RenderLayer() = 0;
  virtual void RenderLayerWithReadback(ReadbackProcessor *aReadback) { RenderLayer(); }

  virtual ClientPaintedLayer* AsThebes() { return nullptr; }

  static inline ClientLayer *
  ToClientLayer(Layer* aLayer)
  {
    return static_cast<ClientLayer*>(aLayer->ImplData());
  }

  template <typename LayerType>
  static inline void RenderMaskLayers(LayerType* aLayer) {
    if (aLayer->GetMaskLayer()) {
      ToClientLayer(aLayer->GetMaskLayer())->RenderLayer();
    }
    for (size_t i = 0; i < aLayer->GetAncestorMaskLayerCount(); i++) {
      ToClientLayer(aLayer->GetAncestorMaskLayerAt(i))->RenderLayer();
    }
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
  LayerHandle shadow = aMgr->AsShadowForwarder()->ConstructShadowFor(aLayer);
  if (!shadow) {
    return;
  }

  aLayer->SetShadow(aMgr->AsShadowForwarder(), shadow);
  (aMgr->AsShadowForwarder()->*aMethod)(aLayer);
  aMgr->Hold(aLayer->AsLayer());
}

#define CREATE_SHADOW(_type)                                       \
  CreateShadowFor(layer, this,                                     \
                  &ShadowLayerForwarder::Created ## _type ## Layer)


} // namespace layers
} // namespace mozilla

#endif /* GFX_CLIENTLAYERMANAGER_H */
