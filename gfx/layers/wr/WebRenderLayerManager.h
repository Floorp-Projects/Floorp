/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERLAYERMANAGER_H
#define GFX_WEBRENDERLAYERMANAGER_H

#include <cstddef>                    // for size_t
#include <cstdint>                    // for uint32_t, int32_t, INT32_MAX
#include <string>                     // for string
#include "Units.h"                    // for LayoutDeviceIntSize
#include "mozilla/AlreadyAddRefed.h"  // for already_AddRefed
#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT, MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"               // for MOZ_NON_OWNING_REF
#include "mozilla/RefPtr.h"                   // for RefPtr
#include "mozilla/StaticPrefs_apz.h"          // for apz_test_logging_enabled
#include "mozilla/TimeStamp.h"                // for TimeStamp
#include "mozilla/gfx/Point.h"                // for IntSize
#include "mozilla/gfx/Types.h"                // for SurfaceFormat
#include "mozilla/layers/APZTestData.h"       // for APZTestData
#include "mozilla/layers/CompositorTypes.h"   // for TextureFactoryIdentifier
#include "mozilla/layers/DisplayItemCache.h"  // for DisplayItemCache
#include "mozilla/layers/FocusTarget.h"       // for FocusTarget
#include "mozilla/layers/LayerManager.h"  // for DidCompositeObserver (ptr only), LayerManager::END_DEFAULT, LayerManager::En...
#include "mozilla/layers/LayersTypes.h"  // for TransactionId, LayersBackend, CompositionPayload (ptr only), LayersBackend::...
#include "mozilla/layers/RenderRootStateManager.h"  // for RenderRootStateManager
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid, ScrollableLayerGuid::ViewID
#include "mozilla/layers/WebRenderCommandBuilder.h"  // for WebRenderCommandBuilder
#include "mozilla/layers/WebRenderScrollData.h"      // for WebRenderScrollData
#include "nsHashKeys.h"                              // for nsRefPtrHashKey
#include "nsRegion.h"                                // for nsIntRegion
#include "nsStringFwd.h"                             // for nsCString, nsAString
#include "nsTArray.h"                                // for nsTArray
#include "nsTHashSet.h"

class gfxContext;
class nsIWidget;

namespace mozilla {

class nsDisplayList;
class nsDisplayListBuilder;
struct ActiveScrolledRoot;

namespace layers {

class CompositorBridgeChild;
class KnowsCompositor;
class Layer;
class PCompositorBridgeChild;
class WebRenderBridgeChild;
class WebRenderParentCommand;

class WebRenderLayerManager final : public LayerManager {
  typedef nsTArray<RefPtr<Layer>> LayerRefArray;
  typedef nsTHashSet<RefPtr<WebRenderUserData>> WebRenderUserDataRefTable;

 public:
  explicit WebRenderLayerManager(nsIWidget* aWidget);
  bool Initialize(PCompositorBridgeChild* aCBChild, wr::PipelineId aLayersId,
                  TextureFactoryIdentifier* aTextureFactoryIdentifier,
                  nsCString& aError);

  void Destroy() override;

  void DoDestroy(bool aIsSync);

 protected:
  virtual ~WebRenderLayerManager();

 public:
  KnowsCompositor* AsKnowsCompositor() override;
  WebRenderLayerManager* AsWebRender() override { return this; }
  WebRenderLayerManager* AsWebRenderLayerManager() override { return this; }
  CompositorBridgeChild* GetCompositorBridgeChild() override;

  // WebRender can handle images larger than the max texture size via tiling.
  int32_t GetMaxTextureSize() const override { return INT32_MAX; }

  bool BeginTransactionWithTarget(gfxContext* aTarget,
                                  const nsCString& aURL) override;
  bool BeginTransaction(const nsCString& aURL) override;
  bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override;
  void EndTransactionWithoutLayer(nsDisplayList* aDisplayList,
                                  nsDisplayListBuilder* aDisplayListBuilder,
                                  WrFiltersHolder&& aFilters,
                                  WebRenderBackgroundData* aBackground,
                                  const double aGeckoDLBuildTime);
  void EndTransaction(DrawPaintedLayerCallback aCallback, void* aCallbackData,
                      EndTransactionFlags aFlags = END_DEFAULT) override;

  LayersBackend GetBackendType() override { return LayersBackend::LAYERS_WR; }
  void GetBackendName(nsAString& name) override;
  const char* Name() const override { return "WebRender"; }

  void SetRoot(Layer* aLayer) override;

  already_AddRefed<PaintedLayer> CreatePaintedLayer() override {
    return nullptr;
  }
  already_AddRefed<ContainerLayer> CreateContainerLayer() override {
    return nullptr;
  }
  already_AddRefed<ImageLayer> CreateImageLayer() override { return nullptr; }
  already_AddRefed<ColorLayer> CreateColorLayer() override { return nullptr; }
  already_AddRefed<CanvasLayer> CreateCanvasLayer() override { return nullptr; }

  bool NeedsWidgetInvalidation() override { return false; }

  void SetLayersObserverEpoch(LayersObserverEpoch aEpoch) override;

  void DidComposite(TransactionId aTransactionId,
                    const mozilla::TimeStamp& aCompositeStart,
                    const mozilla::TimeStamp& aCompositeEnd) override;

  void ClearCachedResources(Layer* aSubtree = nullptr) override;
  void UpdateTextureFactoryIdentifier(
      const TextureFactoryIdentifier& aNewIdentifier) override;
  TextureFactoryIdentifier GetTextureFactoryIdentifier() override;

  void SetTransactionIdAllocator(TransactionIdAllocator* aAllocator) override;
  TransactionId GetLastTransactionId() override;

  void AddDidCompositeObserver(DidCompositeObserver* aObserver) override;
  void RemoveDidCompositeObserver(DidCompositeObserver* aObserver) override;

  void FlushRendering() override;
  void WaitOnTransactionProcessed() override;

  void SendInvalidRegion(const nsIntRegion& aRegion) override;

  void ScheduleComposite() override;

  void SetNeedsComposite(bool aNeedsComposite) override {
    mNeedsComposite = aNeedsComposite;
  }
  bool NeedsComposite() const override { return mNeedsComposite; }
  void SetIsFirstPaint() override { mIsFirstPaint = true; }
  bool GetIsFirstPaint() const override { return mIsFirstPaint; }
  void SetFocusTarget(const FocusTarget& aFocusTarget) override;

  already_AddRefed<PersistentBufferProvider> CreatePersistentBufferProvider(
      const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat) override;

  bool AsyncPanZoomEnabled() const override;

  // adds an imagekey to a list of keys that will be discarded on the next
  // transaction or destruction
  void DiscardImages();
  void DiscardLocalImages();

  void ClearAsyncAnimations();
  void WrReleasedImages(const nsTArray<wr::ExternalImageKeyPair>& aPairs);

  WebRenderBridgeChild* WrBridge() const { return mWrChild; }

  // See equivalent function in ClientLayerManager
  void LogTestDataForCurrentPaint(ScrollableLayerGuid::ViewID aScrollId,
                                  const std::string& aKey,
                                  const std::string& aValue) {
    MOZ_ASSERT(StaticPrefs::apz_test_logging_enabled(), "don't call me");
    mApzTestData.LogTestDataForPaint(mPaintSequenceNumber, aScrollId, aKey,
                                     aValue);
  }
  void LogAdditionalTestData(const std::string& aKey,
                             const std::string& aValue) {
    MOZ_ASSERT(StaticPrefs::apz_test_logging_enabled(), "don't call me");
    mApzTestData.RecordAdditionalData(aKey, aValue);
  }

  // See equivalent function in ClientLayerManager
  const APZTestData& GetAPZTestData() const { return mApzTestData; }

  WebRenderCommandBuilder& CommandBuilder() { return mWebRenderCommandBuilder; }
  WebRenderUserDataRefTable* GetWebRenderUserDataTable() {
    return mWebRenderCommandBuilder.GetWebRenderUserDataTable();
  }
  WebRenderScrollData& GetScrollData() { return mScrollData; }

  void WrUpdated();
  nsIWidget* GetWidget() { return mWidget; }

  uint32_t StartFrameTimeRecording(int32_t aBufferSize) override;
  void StopFrameTimeRecording(uint32_t aStartIndex,
                              nsTArray<float>& aFrameIntervals) override;

  RenderRootStateManager* GetRenderRootStateManager() { return &mStateManager; }

  virtual void PayloadPresented(const TimeStamp& aTimeStamp) override;

  void TakeCompositionPayloads(nsTArray<CompositionPayload>& aPayloads);

  void GetFrameUniformity(FrameUniformityData* aOutData) override;

 private:
  /**
   * Take a snapshot of the parent context, and copy
   * it into mTarget.
   */
  void MakeSnapshotIfRequired(LayoutDeviceIntSize aSize);

 private:
  nsIWidget* MOZ_NON_OWNING_REF mWidget;

  RefPtr<WebRenderBridgeChild> mWrChild;

  RefPtr<TransactionIdAllocator> mTransactionIdAllocator;
  TransactionId mLatestTransactionId;

  nsTArray<DidCompositeObserver*> mDidCompositeObservers;

  // This holds the scroll data that we need to send to the compositor for
  // APZ to do it's job
  WebRenderScrollData mScrollData;

  bool mNeedsComposite;
  bool mIsFirstPaint;
  FocusTarget mFocusTarget;

  // When we're doing a transaction in order to draw to a non-default
  // target, the layers transaction is only performed in order to send
  // a PLayers:Update.  We save the original non-default target to
  // mTarget, and then perform the transaction. After the transaction ends,
  // we send a message to our remote side to capture the actual pixels
  // being drawn to the default target, and then copy those pixels
  // back to mTarget.
  RefPtr<gfxContext> mTarget;

  // See equivalent field in ClientLayerManager
  uint32_t mPaintSequenceNumber;
  // See equivalent field in ClientLayerManager
  APZTestData mApzTestData;

  TimeStamp mTransactionStart;
  nsCString mURL;
  WebRenderCommandBuilder mWebRenderCommandBuilder;

  wr::DisplayListCapacity mLastDisplayListSize;
  RenderRootStateManager mStateManager;
  DisplayItemCache mDisplayItemCache;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_WEBRENDERLAYERMANAGER_H */
