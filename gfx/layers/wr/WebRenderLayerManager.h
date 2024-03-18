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
#include "mozilla/layers/LayersTypes.h"  // for TransactionId, LayersBackend, CompositionPayload (ptr only), LayersBackend::...
#include "mozilla/layers/RenderRootStateManager.h"  // for RenderRootStateManager
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid, ScrollableLayerGuid::ViewID
#include "mozilla/layers/WebRenderCommandBuilder.h"  // for WebRenderCommandBuilder
#include "mozilla/layers/WebRenderScrollData.h"      // for WebRenderScrollData
#include "WindowRenderer.h"
#include "nsHashKeys.h"   // for nsRefPtrHashKey
#include "nsRegion.h"     // for nsIntRegion
#include "nsStringFwd.h"  // for nsCString, nsAString
#include "nsTArray.h"     // for nsTArray
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
class TransactionIdAllocator;
class LayerUserData;

class WebRenderLayerManager final : public WindowRenderer {
  typedef nsTHashSet<RefPtr<WebRenderUserData>> WebRenderUserDataRefTable;

 public:
  explicit WebRenderLayerManager(nsIWidget* aWidget);
  bool Initialize(PCompositorBridgeChild* aCBChild, wr::PipelineId aLayersId,
                  TextureFactoryIdentifier* aTextureFactoryIdentifier,
                  nsCString& aError);

  void Destroy() override;
  bool IsDestroyed() { return mDestroyed; }

  void DoDestroy(bool aIsSync);

 protected:
  virtual ~WebRenderLayerManager();

 public:
  KnowsCompositor* AsKnowsCompositor() override;
  WebRenderLayerManager* AsWebRender() override { return this; }
  CompositorBridgeChild* GetCompositorBridgeChild() override;

  // WebRender can handle images larger than the max texture size via tiling.
  int32_t GetMaxTextureSize() const override { return INT32_MAX; }

  bool BeginTransactionWithTarget(gfxContext* aTarget, const nsCString& aURL);
  bool BeginTransaction(const nsCString& aURL) override;
  bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override;
  void EndTransactionWithoutLayer(nsDisplayList* aDisplayList,
                                  nsDisplayListBuilder* aDisplayListBuilder,
                                  WrFiltersHolder&& aFilters,
                                  WebRenderBackgroundData* aBackground,
                                  const double aGeckoDLBuildTime);

  LayersBackend GetBackendType() override { return LayersBackend::LAYERS_WR; }
  void GetBackendName(nsAString& name) override;

  bool NeedsWidgetInvalidation() override { return false; }

  void DidComposite(TransactionId aTransactionId,
                    const mozilla::TimeStamp& aCompositeStart,
                    const mozilla::TimeStamp& aCompositeEnd);

  void ClearCachedResources();
  void ClearAnimationResources();
  void UpdateTextureFactoryIdentifier(
      const TextureFactoryIdentifier& aNewIdentifier);
  TextureFactoryIdentifier GetTextureFactoryIdentifier();

  void SetTransactionIdAllocator(TransactionIdAllocator* aAllocator);
  TransactionId GetLastTransactionId();

  void FlushRendering(wr::RenderReasons aReasons) override;
  void WaitOnTransactionProcessed() override;

  void SendInvalidRegion(const nsIntRegion& aRegion);

  void ScheduleComposite(wr::RenderReasons aReasons);

  void SetNeedsComposite(bool aNeedsComposite) {
    mNeedsComposite = aNeedsComposite;
  }
  bool NeedsComposite() const { return mNeedsComposite; }
  void SetIsFirstPaint() { mIsFirstPaint = true; }
  bool GetIsFirstPaint() const { return mIsFirstPaint; }
  void SetFocusTarget(const FocusTarget& aFocusTarget);

  already_AddRefed<PersistentBufferProvider> CreatePersistentBufferProvider(
      const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat,
      bool aWillReadFrequently) override;

  bool AsyncPanZoomEnabled() const;

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

  void TakeCompositionPayloads(nsTArray<CompositionPayload>& aPayloads);

  void GetFrameUniformity(FrameUniformityData* aOutData) override;

  void RegisterPayloads(const nsTArray<CompositionPayload>& aPayload) {
    mPayload.AppendElements(aPayload);
    MOZ_ASSERT(mPayload.Length() < 10000);
  }

  static void LayerUserDataDestroy(void* data);
  /**
   * This setter can be used anytime. The user data for all keys is
   * initially null. Ownership pases to the layer manager.
   */
  void SetUserData(void* aKey, LayerUserData* aData) {
    mUserData.Add(static_cast<gfx::UserDataKey*>(aKey), aData,
                  LayerUserDataDestroy);
  }
  /**
   * This can be used anytime. Ownership passes to the caller!
   */
  UniquePtr<LayerUserData> RemoveUserData(void* aKey);

  /**
   * This getter can be used anytime.
   */
  bool HasUserData(void* aKey) {
    return mUserData.Has(static_cast<gfx::UserDataKey*>(aKey));
  }
  /**
   * This getter can be used anytime. Ownership is retained by the layer
   * manager.
   */
  LayerUserData* GetUserData(void* aKey) const {
    return static_cast<LayerUserData*>(
        mUserData.Get(static_cast<gfx::UserDataKey*>(aKey)));
  }

  std::unordered_set<ScrollableLayerGuid::ViewID>
  ClearPendingScrollInfoUpdate();

#ifdef DEBUG
  gfxContext* GetTarget() const { return mTarget; }
#endif

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

  gfx::UserData mUserData;

  // This holds the scroll data that we need to send to the compositor for
  // APZ to do it's job
  WebRenderScrollData mScrollData;

  bool mNeedsComposite;
  bool mIsFirstPaint;
  bool mDestroyed;
  FocusTarget mFocusTarget;

  // The payload associated with currently pending painting work, for
  // client layer managers that typically means payload that is part of the
  // 'upcoming transaction', for HostLayerManagers this typically means
  // what has been included in received transactions to be presented on the
  // next composite.
  // IMPORTANT: Clients should take care to clear this or risk it slowly
  // growing out of control.
  nsTArray<CompositionPayload> mPayload;

  // When we're doing a transaction in order to draw to a non-default
  // target, the layers transaction is only performed in order to send
  // a PLayers:Update.  We save the original non-default target to
  // mTarget, and then perform the transaction. After the transaction ends,
  // we send a message to our remote side to capture the actual pixels
  // being drawn to the default target, and then copy those pixels
  // back to mTarget.
  gfxContext* mTarget;

  // See equivalent field in ClientLayerManager
  uint32_t mPaintSequenceNumber;
  // See equivalent field in ClientLayerManager
  APZTestData mApzTestData;

  TimeStamp mTransactionStart;
  nsCString mURL;
  WebRenderCommandBuilder mWebRenderCommandBuilder;

  RenderRootStateManager mStateManager;
  DisplayItemCache mDisplayItemCache;
  UniquePtr<wr::DisplayListBuilder> mDLBuilder;

  ScrollUpdatesMap mPendingScrollUpdates;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_WEBRENDERLAYERMANAGER_H */
