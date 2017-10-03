/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERLAYERMANAGER_H
#define GFX_WEBRENDERLAYERMANAGER_H

#include <unordered_set>
#include <vector>

#include "gfxPrefs.h"
#include "Layers.h"
#include "mozilla/MozPromise.h"
#include "mozilla/layers/APZTestData.h"
#include "mozilla/layers/FocusTarget.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/TransactionIdAllocator.h"
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

class WebRenderLayerManager final : public LayerManager
{
  typedef nsTArray<RefPtr<Layer> > LayerRefArray;
  typedef nsTHashtable<nsRefPtrHashKey<WebRenderUserData>> WebRenderUserDataRefTable;

public:
  explicit WebRenderLayerManager(nsIWidget* aWidget);
  bool Initialize(PCompositorBridgeChild* aCBChild, wr::PipelineId aLayersId, TextureFactoryIdentifier* aTextureFactoryIdentifier);

  virtual void Destroy() override;

  void DoDestroy(bool aIsSync);

protected:
  virtual ~WebRenderLayerManager();

public:
  virtual KnowsCompositor* AsKnowsCompositor() override;
  WebRenderLayerManager* AsWebRenderLayerManager() override { return this; }
  virtual CompositorBridgeChild* GetCompositorBridgeChild() override;

  virtual int32_t GetMaxTextureSize() const override;

  virtual bool BeginTransactionWithTarget(gfxContext* aTarget) override;
  virtual bool BeginTransaction() override;
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override;
  Maybe<wr::ImageKey> CreateImageKey(nsDisplayItem* aItem,
                                     ImageContainer* aContainer,
                                     mozilla::wr::DisplayListBuilder& aBuilder,
                                     mozilla::wr::IpcResourceUpdateQueue& aResources,
                                     const StackingContextHelper& aSc,
                                     gfx::IntSize& aSize);
  bool PushImage(nsDisplayItem* aItem,
                 ImageContainer* aContainer,
                 mozilla::wr::DisplayListBuilder& aBuilder,
                 mozilla::wr::IpcResourceUpdateQueue& aResources,
                 const StackingContextHelper& aSc,
                 const LayerRect& aRect);

  already_AddRefed<WebRenderFallbackData>
  GenerateFallbackData(nsDisplayItem* aItem,
                       wr::DisplayListBuilder& aBuilder,
                       wr::IpcResourceUpdateQueue& aResourceUpdates,
                       const StackingContextHelper& aSc,
                       nsDisplayListBuilder* aDisplayListBuilder,
                       LayerRect& aImageRect);

  Maybe<wr::WrImageMask> BuildWrMaskImage(nsDisplayItem* aItem,
                                          wr::DisplayListBuilder& aBuilder,
                                          wr::IpcResourceUpdateQueue& aResources,
                                          const StackingContextHelper& aSc,
                                          nsDisplayListBuilder* aDisplayListBuilder,
                                          const LayerRect& aBounds);
  bool PushItemAsImage(nsDisplayItem* aItem,
                       wr::DisplayListBuilder& aBuilder,
                       wr::IpcResourceUpdateQueue& aResources,
                       const StackingContextHelper& aSc,
                       nsDisplayListBuilder* aDisplayListBuilder);
  void CreateWebRenderCommandsFromDisplayList(nsDisplayList* aDisplayList,
                                              nsDisplayListBuilder* aDisplayListBuilder,
                                              const StackingContextHelper& aSc,
                                              wr::DisplayListBuilder& aBuilder,
                                              wr::IpcResourceUpdateQueue& aResources);
  void EndTransactionWithoutLayer(nsDisplayList* aDisplayList,
                                  nsDisplayListBuilder* aDisplayListBuilder);
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) override;

  virtual LayersBackend GetBackendType() override { return LayersBackend::LAYERS_WR; }
  virtual void GetBackendName(nsAString& name) override { name.AssignLiteral("WebRender"); }
  virtual const char* Name() const override { return "WebRender"; }

  virtual void SetRoot(Layer* aLayer) override;

  already_AddRefed<PaintedLayer> CreatePaintedLayer() override { return nullptr; }
  already_AddRefed<ContainerLayer> CreateContainerLayer() override { return nullptr; }
  already_AddRefed<ImageLayer> CreateImageLayer() override { return nullptr; }
  already_AddRefed<ColorLayer> CreateColorLayer() override { return nullptr; }
  already_AddRefed<TextLayer> CreateTextLayer() override { return nullptr; }
  already_AddRefed<BorderLayer> CreateBorderLayer() override { return nullptr; }
  already_AddRefed<CanvasLayer> CreateCanvasLayer() override { return nullptr; }

  virtual bool NeedsWidgetInvalidation() override { return false; }

  virtual void SetLayerObserverEpoch(uint64_t aLayerObserverEpoch) override;

  virtual void DidComposite(uint64_t aTransactionId,
                            const mozilla::TimeStamp& aCompositeStart,
                            const mozilla::TimeStamp& aCompositeEnd) override;

  virtual void ClearCachedResources(Layer* aSubtree = nullptr) override;
  virtual void UpdateTextureFactoryIdentifier(const TextureFactoryIdentifier& aNewIdentifier,
                                              uint64_t aDeviceResetSeqNo) override;
  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() override;

  virtual void SetTransactionIdAllocator(TransactionIdAllocator* aAllocator) override
  { mTransactionIdAllocator = aAllocator; }

  virtual void AddDidCompositeObserver(DidCompositeObserver* aObserver) override;
  virtual void RemoveDidCompositeObserver(DidCompositeObserver* aObserver) override;

  virtual void FlushRendering() override;
  virtual void WaitOnTransactionProcessed() override;

  virtual void SendInvalidRegion(const nsIntRegion& aRegion) override;

  virtual void ScheduleComposite() override;

  virtual void SetNeedsComposite(bool aNeedsComposite) override
  {
    mNeedsComposite = aNeedsComposite;
  }
  virtual bool NeedsComposite() const override { return mNeedsComposite; }
  virtual void SetIsFirstPaint() override { mIsFirstPaint = true; }
  virtual void SetFocusTarget(const FocusTarget& aFocusTarget) override;

  bool AsyncPanZoomEnabled() const override;

  // adds an imagekey to a list of keys that will be discarded on the next
  // transaction or destruction
  void AddImageKeyForDiscard(wr::ImageKey);
  void DiscardImages();
  void DiscardLocalImages();

  // Methods to manage the compositor animation ids. Active animations are still
  // going, and when they end we discard them and remove them from the active
  // list.
  void AddActiveCompositorAnimationId(uint64_t aId);
  void AddCompositorAnimationsIdForDiscard(uint64_t aId);
  void DiscardCompositorAnimations();

  WebRenderBridgeChild* WrBridge() const { return mWrChild; }

  void SetTransactionIncomplete() { mTransactionIncomplete = true; }

  // See equivalent function in ClientLayerManager
  void LogTestDataForCurrentPaint(FrameMetrics::ViewID aScrollId,
                                  const std::string& aKey,
                                  const std::string& aValue) {
    MOZ_ASSERT(gfxPrefs::APZTestLoggingEnabled(), "don't call me");
    mApzTestData.LogTestDataForPaint(mPaintSequenceNumber, aScrollId, aKey, aValue);
  }
  // See equivalent function in ClientLayerManager
  const APZTestData& GetAPZTestData() const
  { return mApzTestData; }

  // Those are data that we kept between transactions. We used to cache some
  // data in the layer. But in layers free mode, we don't have layer which
  // means we need some other place to cached the data between transaction.
  // We store the data in frame's property.
  template<class T> already_AddRefed<T>
  CreateOrRecycleWebRenderUserData(nsDisplayItem* aItem, bool* aOutIsRecycled = nullptr)
  {
    MOZ_ASSERT(aItem);
    nsIFrame* frame = aItem->Frame();
    if (aOutIsRecycled) {
      *aOutIsRecycled = true;
    }

    nsIFrame::WebRenderUserDataTable* userDataTable =
      frame->GetProperty(nsIFrame::WebRenderUserDataProperty());

    if (!userDataTable) {
      userDataTable = new nsIFrame::WebRenderUserDataTable();
      frame->AddProperty(nsIFrame::WebRenderUserDataProperty(), userDataTable);
    }

    RefPtr<WebRenderUserData>& data = userDataTable->GetOrInsert(aItem->GetPerFrameKey());
    if (!data || (data->GetType() != T::Type()) || !data->IsDataValid(this)) {
      // To recreate a new user data, we should remove the data from the table first.
      if (data) {
        data->RemoveFromTable();
      }
      data = new T(this, aItem, &mWebRenderUserDatas);
      mWebRenderUserDatas.PutEntry(data);
      if (aOutIsRecycled) {
        *aOutIsRecycled = false;
      }
    }

    MOZ_ASSERT(data);
    MOZ_ASSERT(data->GetType() == T::Type());

    // Mark the data as being used. We will remove unused user data in the end of EndTransaction.
    data->SetUsed(true);

    if (T::Type() == WebRenderUserData::UserDataType::eCanvas) {
      mLastCanvasDatas.PutEntry(data->AsCanvasData());
    }
    RefPtr<T> res = static_cast<T*>(data.get());
    return res.forget();
  }

  bool SetPendingScrollUpdateForNextTransaction(FrameMetrics::ViewID aScrollId,
                                                const ScrollUpdateInfo& aUpdateInfo) override;

private:
  /**
   * Take a snapshot of the parent context, and copy
   * it into mTarget.
   */
  void MakeSnapshotIfRequired(LayoutDeviceIntSize aSize);

  void ClearLayer(Layer* aLayer);

  bool EndTransactionInternal(EndTransactionFlags aFlags,
                              nsDisplayList* aDisplayList = nullptr,
                              nsDisplayListBuilder* aDisplayListBuilder = nullptr);

  void RemoveUnusedAndResetWebRenderUserData()
  {
    for (auto iter = mWebRenderUserDatas.Iter(); !iter.Done(); iter.Next()) {
      WebRenderUserData* data = iter.Get()->GetKey();
      if (!data->IsUsed()) {
        nsIFrame* frame = data->GetFrame();

        MOZ_ASSERT(frame->HasProperty(nsIFrame::WebRenderUserDataProperty()));

        nsIFrame::WebRenderUserDataTable* userDataTable =
          frame->GetProperty(nsIFrame::WebRenderUserDataProperty());

        MOZ_ASSERT(userDataTable->Count());

        userDataTable->Remove(data->GetDisplayItemKey());

        if (!userDataTable->Count()) {
          frame->RemoveProperty(nsIFrame::WebRenderUserDataProperty());
        }
        iter.Remove();
        continue;
      }

      data->SetUsed(false);
    }
  }

private:
  nsIWidget* MOZ_NON_OWNING_REF mWidget;
  nsTArray<wr::ImageKey> mImageKeysToDelete;
  // TODO - This is needed because we have some code that creates image keys
  // and enqueues them for deletion right away which is bad not only because
  // of poor texture cache usage, but also because images end up deleted before
  // they are used. This should hopfully be temporary.
  nsTArray<wr::ImageKey> mImageKeysToDeleteLater;

  // Set of compositor animation ids for which there are active animations (as
  // of the last transaction) on the compositor side.
  std::unordered_set<uint64_t> mActiveCompositorAnimationIds;
  // Compositor animation ids for animations that are done now and that we want
  // the compositor to discard information for.
  nsTArray<uint64_t> mDiscardedCompositorAnimationsIds;

  RefPtr<WebRenderBridgeChild> mWrChild;

  RefPtr<TransactionIdAllocator> mTransactionIdAllocator;
  uint64_t mLatestTransactionId;

  nsTArray<DidCompositeObserver*> mDidCompositeObservers;

  // These fields are used to save a copy of the display list for
  // empty transactions in layers-free mode.
  wr::BuiltDisplayList mBuiltDisplayList;
  nsTArray<WebRenderParentCommand> mParentCommands;

  // This holds the scroll data that we need to send to the compositor for
  // APZ to do it's job
  WebRenderScrollData mScrollData;
  // We use this as a temporary data structure while building the mScrollData
  // inside a layers-free transaction.
  std::vector<WebRenderLayerScrollData> mLayerScrollData;
  // We use this as a temporary data structure to track the current display
  // item's ASR as we recurse in CreateWebRenderCommandsFromDisplayList. We
  // need this so that WebRenderLayerScrollData items that deeper in the
  // tree don't duplicate scroll metadata that their ancestors already have.
  std::vector<const ActiveScrolledRoot*> mAsrStack;
  const ActiveScrolledRoot* mLastAsr;

public:
  // Note: two DisplayItemClipChain* A and B might actually be "equal" (as per
  // DisplayItemClipChain::Equal(A, B)) even though they are not the same pointer
  // (A != B). In this hopefully-rare case, they will get separate entries
  // in this map when in fact we could collapse them. However, to collapse
  // them involves writing a custom hash function for the pointer type such that
  // A and B hash to the same things whenever DisplayItemClipChain::Equal(A, B)
  // is true, and that will incur a performance penalty for all the hashmap
  // operations, so is probably not worth it. With the current code we might
  // end up creating multiple clips in WR that are effectively identical but
  // have separate clip ids. Hopefully this won't happen very often.
  typedef std::unordered_map<const DisplayItemClipChain*, wr::WrClipId> ClipIdMap;
private:
  ClipIdMap mClipIdCache;

  bool mTransactionIncomplete;

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

  typedef nsTHashtable<nsRefPtrHashKey<WebRenderCanvasData>> CanvasDataSet;
  // Store of WebRenderCanvasData objects for use in empty transactions
  CanvasDataSet mLastCanvasDatas;

  WebRenderUserDataRefTable mWebRenderUserDatas;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERLAYERMANAGER_H */
