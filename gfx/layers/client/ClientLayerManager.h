/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CLIENTLAYERMANAGER_H
#define GFX_CLIENTLAYERMANAGER_H

#include <cstddef>                    // for size_t
#include <cstdint>                    // for uint32_t, int32_t
#include <new>                        // for operator new
#include <string>                     // for string
#include <utility>                    // for forward
#include "gfxContext.h"               // for gfxContext
#include "mozilla/AlreadyAddRefed.h"  // for already_AddRefed
#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT, MOZ_ASSERT_HELPER2
#include "mozilla/RefPtr.h"                  // for RefPtr
#include "mozilla/TimeStamp.h"               // for TimeStamp, BaseTimeDuration
#include "mozilla/WidgetUtils.h"             // for ScreenRotation
#include "mozilla/gfx/Point.h"               // for IntSize
#include "mozilla/gfx/Types.h"               // for SurfaceFormat
#include "mozilla/layers/APZTestData.h"      // for APZTestData, SequenceNumber
#include "mozilla/layers/CompositorTypes.h"  // for TextureFactoryIdentifier
#include "mozilla/layers/LayerManager.h"  // for LayerManager::DrawPaintedLayerCallback, DidCompositeObserver (ptr only), Laye...
#include "mozilla/layers/LayersTypes.h"  // for LayerHandle, LayersBackend, TransactionId, BufferMode, LayersBackend::LAYERS_...
#include "mozilla/layers/MemoryPressureObserver.h"  // for MemoryPressureListener, MemoryPressureObserver (ptr only), MemoryPressureReason
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid, ScrollableLayerGuid::ViewID
#include "mozilla/layers/ShadowLayers.h"  // for ShadowLayerForwarder, ShadowableLayer
#include "nsISupports.h"                  // for MOZ_COUNTED_DEFAULT_CTOR
#include "nsIThread.h"                    // for TimeDuration
#include "nsIWidget.h"                    // for nsIWidget
#include "nsRegion.h"                     // for nsIntRegion
#include "nsStringFwd.h"                  // for nsCString, nsAString
#include "nsTArray.h"                     // for nsTArray

// XXX Includes that could be avoided by moving function implementation to the
// cpp file.
#include "mozilla/StaticPrefs_apz.h"  // for apz_test_logging_enabled

namespace mozilla {
namespace layers {

class CanvasLayer;
class ColorLayer;
class ContainerLayer;
class ClientPaintedLayer;
class CompositorBridgeChild;
class FocusTarget;
class FrameUniformityData;
class ImageLayer;
class Layer;
class PCompositorBridgeChild;
class PaintTiming;
class PaintedLayer;
class PersistentBufferProvider;
class ReadbackLayer;
class ReadbackProcessor;
class RefLayer;
class TransactionIdAllocator;

class ClientLayerManager final : public LayerManager,
                                 public MemoryPressureListener {
  typedef nsTArray<RefPtr<Layer> > LayerRefArray;

 public:
  explicit ClientLayerManager(nsIWidget* aWidget);
  bool Initialize(PCompositorBridgeChild* aCBChild, bool aShouldAccelerate,
                  TextureFactoryIdentifier* aTextureFactoryIdentifier);
  void Destroy() override;

 protected:
  virtual ~ClientLayerManager();

 public:
  ShadowLayerForwarder* AsShadowForwarder() override { return mForwarder; }

  KnowsCompositor* AsKnowsCompositor() override { return mForwarder; }

  ClientLayerManager* AsClientLayerManager() override { return this; }

  int32_t GetMaxTextureSize() const override;

  void SetDefaultTargetConfiguration(BufferMode aDoubleBuffering,
                                     ScreenRotation aRotation);
  bool BeginTransactionWithTarget(gfxContext* aTarget,
                                  const nsCString& aURL) override;
  bool BeginTransaction(const nsCString& aURL) override;
  bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override;
  void EndTransaction(DrawPaintedLayerCallback aCallback, void* aCallbackData,
                      EndTransactionFlags aFlags = END_DEFAULT) override;

  LayersBackend GetBackendType() override {
    return LayersBackend::LAYERS_CLIENT;
  }
  LayersBackend GetCompositorBackendType() override {
    return AsShadowForwarder()->GetCompositorBackendType();
  }
  void GetBackendName(nsAString& name) override;
  const char* Name() const override { return "Client"; }

  void SetRoot(Layer* aLayer) override;

  void Mutated(Layer* aLayer) override;
  void MutatedSimple(Layer* aLayer) override;

  already_AddRefed<PaintedLayer> CreatePaintedLayer() override;
  already_AddRefed<PaintedLayer> CreatePaintedLayerWithHint(
      PaintedLayerCreationHint aHint) override;
  already_AddRefed<ContainerLayer> CreateContainerLayer() override;
  already_AddRefed<ImageLayer> CreateImageLayer() override;
  already_AddRefed<CanvasLayer> CreateCanvasLayer() override;
  already_AddRefed<ReadbackLayer> CreateReadbackLayer() override;
  already_AddRefed<ColorLayer> CreateColorLayer() override;
  already_AddRefed<RefLayer> CreateRefLayer() override;

  void UpdateTextureFactoryIdentifier(
      const TextureFactoryIdentifier& aNewIdentifier) override;
  TextureFactoryIdentifier GetTextureFactoryIdentifier() override {
    return AsShadowForwarder()->GetTextureFactoryIdentifier();
  }

  void FlushRendering() override;
  void WaitOnTransactionProcessed() override;
  void SendInvalidRegion(const nsIntRegion& aRegion) override;

  uint32_t StartFrameTimeRecording(int32_t aBufferSize) override;

  void StopFrameTimeRecording(uint32_t aStartIndex,
                              nsTArray<float>& aFrameIntervals) override;

  bool NeedsWidgetInvalidation() override { return false; }

  ShadowableLayer* Hold(Layer* aLayer);

  bool HasShadowManager() const { return mForwarder->HasShadowManager(); }

  bool IsCompositingCheap() override;
  bool HasShadowManagerInternal() const override { return HasShadowManager(); }

  void SetIsFirstPaint() override;
  bool GetIsFirstPaint() const override {
    return mForwarder->GetIsFirstPaint();
  }

  void SetFocusTarget(const FocusTarget& aFocusTarget) override;

  // Drop cached resources and ask our shadow manager to do the same,
  // if we have one.
  void ClearCachedResources(Layer* aSubtree = nullptr) override;

  void OnMemoryPressure(MemoryPressureReason aWhy) override;

  void SetRepeatTransaction() { mRepeatTransaction = true; }
  bool GetRepeatTransaction() { return mRepeatTransaction; }

  bool IsRepeatTransaction() { return mIsRepeatTransaction; }

  void SetTransactionIncomplete() { mTransactionIncomplete = true; }
  void SetQueuedAsyncPaints() { mQueuedAsyncPaints = true; }

  bool HasShadowTarget() { return !!mShadowTarget; }

  void SetShadowTarget(gfxContext* aTarget) { mShadowTarget = aTarget; }

  bool CompositorMightResample() { return mCompositorMightResample; }

  DrawPaintedLayerCallback GetPaintedLayerCallback() const {
    return mPaintedLayerCallback;
  }

  void* GetPaintedLayerCallbackData() const {
    return mPaintedLayerCallbackData;
  }

  CompositorBridgeChild* GetRemoteRenderer();

  CompositorBridgeChild* GetCompositorBridgeChild() override;

  bool InConstruction() { return mPhase == PHASE_CONSTRUCTION; }
#ifdef DEBUG
  bool InDrawing() { return mPhase == PHASE_DRAWING; }
  bool InForward() { return mPhase == PHASE_FORWARD; }
#endif
  bool InTransaction() { return mPhase != PHASE_NONE; }

  void SetNeedsComposite(bool aNeedsComposite) override {
    mNeedsComposite = aNeedsComposite;
  }
  bool NeedsComposite() const override { return mNeedsComposite; }

  void ScheduleComposite() override;
  void GetFrameUniformity(FrameUniformityData* aFrameUniformityData) override;

  void DidComposite(TransactionId aTransactionId,
                    const mozilla::TimeStamp& aCompositeStart,
                    const mozilla::TimeStamp& aCompositeEnd) override;

  bool AreComponentAlphaLayersEnabled() override;

  // Log APZ test data for the current paint. We supply the paint sequence
  // number ourselves, and take care of calling APZTestData::StartNewPaint()
  // when a new paint is started.
  void LogTestDataForCurrentPaint(ScrollableLayerGuid::ViewID aScrollId,
                                  const std::string& aKey,
                                  const std::string& aValue) {
    MOZ_ASSERT(StaticPrefs::apz_test_logging_enabled(), "don't call me");
    mApzTestData.LogTestDataForPaint(mPaintSequenceNumber, aScrollId, aKey,
                                     aValue);
  }

  // Log APZ test data for a repaint request. The sequence number must be
  // passed in from outside, and APZTestData::StartNewRepaintRequest() needs
  // to be called from the outside as well when a new repaint request is
  // started.
  void StartNewRepaintRequest(SequenceNumber aSequenceNumber);

  // TODO(botond): When we start using this and write a wrapper similar to
  // nsLayoutUtils::LogTestDataForPaint(), make sure that wrapper checks
  // StaticPrefs::apz_test_logging_enabled().
  void LogTestDataForRepaintRequest(SequenceNumber aSequenceNumber,
                                    ScrollableLayerGuid::ViewID aScrollId,
                                    const std::string& aKey,
                                    const std::string& aValue) {
    MOZ_ASSERT(StaticPrefs::apz_test_logging_enabled(), "don't call me");
    mApzTestData.LogTestDataForRepaintRequest(aSequenceNumber, aScrollId, aKey,
                                              aValue);
  }
  void LogAdditionalTestData(const std::string& aKey,
                             const std::string& aValue) {
    MOZ_ASSERT(StaticPrefs::apz_test_logging_enabled(), "don't call me");
    mApzTestData.RecordAdditionalData(aKey, aValue);
  }

  // Get the content-side APZ test data for reading. For writing, use the
  // LogTestData...() functions.
  const APZTestData& GetAPZTestData() const { return mApzTestData; }

  // Get a copy of the compositor-side APZ test data for our layers ID.
  void GetCompositorSideAPZTestData(APZTestData* aData) const;

  void SetTransactionIdAllocator(TransactionIdAllocator* aAllocator) override;

  TransactionId GetLastTransactionId() override { return mLatestTransactionId; }

  float RequestProperty(const nsAString& aProperty) override;

  bool AsyncPanZoomEnabled() const override;

  void SetLayersObserverEpoch(LayersObserverEpoch aEpoch) override;

  void AddDidCompositeObserver(DidCompositeObserver* aObserver) override;
  void RemoveDidCompositeObserver(DidCompositeObserver* aObserver) override;

  already_AddRefed<PersistentBufferProvider> CreatePersistentBufferProvider(
      const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat) override;

  static PaintTiming* MaybeGetPaintTiming(LayerManager* aManager) {
    if (!aManager) {
      return nullptr;
    }
    if (ClientLayerManager* lm = aManager->AsClientLayerManager()) {
      return &lm->AsShadowForwarder()->GetPaintTiming();
    }
    return nullptr;
  }

 protected:
  enum TransactionPhase {
    PHASE_NONE,
    PHASE_CONSTRUCTION,
    PHASE_DRAWING,
    PHASE_FORWARD
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

  void HandleMemoryPressureLayer(Layer* aLayer);

  bool EndTransactionInternal(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData, EndTransactionFlags);

  void FlushAsyncPaints();

  LayerRefArray mKeepAlive;

  nsIWidget* mWidget;

  /* PaintedLayer callbacks; valid at the end of a transaciton,
   * while rendering */
  DrawPaintedLayerCallback mPaintedLayerCallback;
  void* mPaintedLayerCallbackData;

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
  TransactionId mLatestTransactionId;
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
  bool mQueuedAsyncPaints;
  bool mNotifyingWidgetListener;

  // An incrementing sequence number for paints.
  // Incremented in BeginTransaction(), but not for repeat transactions.
  uint32_t mPaintSequenceNumber;

  APZTestData mApzTestData;

  RefPtr<ShadowLayerForwarder> mForwarder;
  mozilla::TimeStamp mTransactionStart;
  nsCString mURL;

  nsTArray<DidCompositeObserver*> mDidCompositeObservers;

  RefPtr<MemoryPressureObserver> mMemoryPressureObserver;
};

class ClientLayer : public ShadowableLayer {
 public:
  MOZ_COUNTED_DEFAULT_CTOR(ClientLayer)

  ~ClientLayer();

  // Shrink memory usage.
  // Called when "memory-pressure" is observed.
  virtual void HandleMemoryPressure() {}

  virtual void RenderLayer() = 0;
  virtual void RenderLayerWithReadback(ReadbackProcessor* aReadback) {
    RenderLayer();
  }

  virtual ClientPaintedLayer* AsThebes() { return nullptr; }

  static ClientLayer* ToClientLayer(Layer* aLayer);

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

// Create a LayerHandle for aLayer, if we're forwarding our layer tree
// to a parent process.  Record the new layer creation in the current
// open transaction as a side effect.
template <typename CreatedMethod>
void CreateShadowFor(ClientLayer* aLayer, ClientLayerManager* aMgr,
                     CreatedMethod aMethod) {
  LayerHandle shadow = aMgr->AsShadowForwarder()->ConstructShadowFor(aLayer);
  if (!shadow) {
    return;
  }

  aLayer->SetShadow(aMgr->AsShadowForwarder(), shadow);
  (aMgr->AsShadowForwarder()->*aMethod)(aLayer);
  aMgr->Hold(aLayer->AsLayer());
}

#define CREATE_SHADOW(_type) \
  CreateShadowFor(layer, this, &ShadowLayerForwarder::Created##_type##Layer)

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_CLIENTLAYERMANAGER_H */
