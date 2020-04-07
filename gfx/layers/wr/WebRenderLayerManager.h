/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERLAYERMANAGER_H
#define GFX_WEBRENDERLAYERMANAGER_H

#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "Layers.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/layers/APZTestData.h"
#include "mozilla/layers/FocusTarget.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/SharedSurfacesChild.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/TransactionIdAllocator.h"
#include "mozilla/layers/WebRenderCommandBuilder.h"
#include "mozilla/layers/WebRenderScrollData.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsDisplayList.h"

class nsIWidget;

namespace mozilla {

struct ActiveScrolledRoot;

namespace layers {

class CompositorBridgeChild;
class KnowsCompositor;
class PCompositorBridgeChild;
class WebRenderBridgeChild;
class WebRenderParentCommand;

class WebRenderLayerManager final : public LayerManager {
  typedef nsTArray<RefPtr<Layer>> LayerRefArray;
  typedef nsTHashtable<nsRefPtrHashKey<WebRenderUserData>>
      WebRenderUserDataRefTable;

 public:
  explicit WebRenderLayerManager(nsIWidget* aWidget);
  bool Initialize(PCompositorBridgeChild* aCBChild, wr::PipelineId aLayersId,
                  TextureFactoryIdentifier* aTextureFactoryIdentifier);

  void Destroy() override;

  void DoDestroy(bool aIsSync);

 protected:
  virtual ~WebRenderLayerManager();

 public:
  KnowsCompositor* AsKnowsCompositor() override;
  WebRenderLayerManager* AsWebRenderLayerManager() override { return this; }
  CompositorBridgeChild* GetCompositorBridgeChild() override;

  // WebRender can handle images larger than the max texture size via tiling.
  int32_t GetMaxTextureSize() const override { return INT32_MAX; }

  bool BeginTransactionWithTarget(gfxContext* aTarget,
                                  const nsCString& aURL) override;
  bool BeginTransaction(const nsCString& aURL) override;
  bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override;
  void EndTransactionWithoutLayer(
      nsDisplayList* aDisplayList, nsDisplayListBuilder* aDisplayListBuilder,
      WrFiltersHolder&& aFilters = WrFiltersHolder(),
      WebRenderBackgroundData* aBackground = nullptr);
  void EndTransaction(DrawPaintedLayerCallback aCallback, void* aCallbackData,
                      EndTransactionFlags aFlags = END_DEFAULT) override;

  LayersBackend GetBackendType() override { return LayersBackend::LAYERS_WR; }
  void GetBackendName(nsAString& name) override {
    name.AssignLiteral("WebRender");
  }
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

  RenderRootStateManager* GetRenderRootStateManager(
      wr::RenderRoot aRenderRoot) {
    return &mStateManagers[aRenderRoot];
  }

  virtual void PayloadPresented() override;

  void TakeCompositionPayloads(nsTArray<CompositionPayload>& aPayloads);

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

  wr::RenderRootArray<size_t> mLastDisplayListSizes;
  wr::RenderRootArray<RenderRootStateManager> mStateManagers;
  DisplayItemCache mDisplayItemCache;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_WEBRENDERLAYERMANAGER_H */
