/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERLAYERMANAGER_H
#define GFX_WEBRENDERLAYERMANAGER_H

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
                                     const StackingContextHelper& aSc,
                                     gfx::IntSize& aSize);
  bool PushImage(nsDisplayItem* aItem,
                 ImageContainer* aContainer,
                 mozilla::wr::DisplayListBuilder& aBuilder,
                 const StackingContextHelper& aSc,
                 const LayerRect& aRect);

  already_AddRefed<WebRenderFallbackData>
  GenerateFallbackData(nsDisplayItem* aItem,
                       wr::DisplayListBuilder& aBuilder,
                       wr::IpcResourceUpdateQueue& aResourceUpdates,
                       nsDisplayListBuilder* aDisplayListBuilder,
                       LayerRect& aImageRect,
                       LayerPoint& aOffset);

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
  bool IsLayersFreeTransaction() { return mEndTransactionWithoutLayers; }
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) override;

  virtual LayersBackend GetBackendType() override { return LayersBackend::LAYERS_WR; }
  virtual void GetBackendName(nsAString& name) override { name.AssignLiteral("WebRender"); }
  virtual const char* Name() const override { return "WebRender"; }

  virtual void SetRoot(Layer* aLayer) override;

  virtual already_AddRefed<PaintedLayer> CreatePaintedLayer() override;
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() override;
  virtual already_AddRefed<ImageLayer> CreateImageLayer() override;
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() override;
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer() override;
  virtual already_AddRefed<ColorLayer> CreateColorLayer() override;
  virtual already_AddRefed<RefLayer> CreateRefLayer() override;
  virtual already_AddRefed<TextLayer> CreateTextLayer() override;
  virtual already_AddRefed<BorderLayer> CreateBorderLayer() override;
  virtual already_AddRefed<DisplayItemLayer> CreateDisplayItemLayer() override;

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

  DrawPaintedLayerCallback GetPaintedLayerCallback() const
  { return mPaintedLayerCallback; }

  void* GetPaintedLayerCallbackData() const
  { return mPaintedLayerCallbackData; }

  // adds an imagekey to a list of keys that will be discarded on the next
  // transaction or destruction
  void AddImageKeyForDiscard(wr::ImageKey);
  void DiscardImages();
  void DiscardLocalImages();

  // Before destroying a layer with animations, add its compositorAnimationsId
  // to a list of ids that will be discarded on the next transaction
  void AddCompositorAnimationsIdForDiscard(uint64_t aId);
  void DiscardCompositorAnimations();

  WebRenderBridgeChild* WrBridge() const { return mWrChild; }

  virtual void Mutated(Layer* aLayer) override;
  virtual void MutatedSimple(Layer* aLayer) override;

  void Hold(Layer* aLayer);
  void SetTransactionIncomplete() { mTransactionIncomplete = true; }
  bool IsMutatedLayer(Layer* aLayer);

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

    if (!frame->HasProperty(nsIFrame::WebRenderUserDataProperty())) {
      frame->AddProperty(nsIFrame::WebRenderUserDataProperty(),
                         new nsIFrame::WebRenderUserDataTable());
    }

    nsIFrame::WebRenderUserDataTable* userDataTable =
      frame->GetProperty(nsIFrame::WebRenderUserDataProperty());
    RefPtr<WebRenderUserData>& data = userDataTable->GetOrInsert(aItem->GetPerFrameKey());
    if (!data || (data->GetType() != T::Type()) || !data->IsDataValid(this)) {
      data = new T(this);
      if (aOutIsRecycled) {
        *aOutIsRecycled = false;
      }
    }

    MOZ_ASSERT(data);
    MOZ_ASSERT(data->GetType() == T::Type());
    if (T::Type() == WebRenderUserData::UserDataType::eCanvas) {
      mLastCanvasDatas.PutEntry(data->AsCanvasData());
    }
    RefPtr<T> res = static_cast<T*>(data.get());
    return res.forget();
  }

  bool ShouldNotifyInvalidation() const { return mShouldNotifyInvalidation; }

private:
  /**
   * Take a snapshot of the parent context, and copy
   * it into mTarget.
   */
  void MakeSnapshotIfRequired(LayoutDeviceIntSize aSize);

  void ClearLayer(Layer* aLayer);

  bool EndTransactionInternal(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags,
                              nsDisplayList* aDisplayList = nullptr,
                              nsDisplayListBuilder* aDisplayListBuilder = nullptr);

private:
  nsIWidget* MOZ_NON_OWNING_REF mWidget;
  nsTArray<wr::ImageKey> mImageKeysToDelete;
  nsTArray<uint64_t> mDiscardedCompositorAnimationsIds;

  /* PaintedLayer callbacks; valid at the end of a transaciton,
   * while rendering */
  DrawPaintedLayerCallback mPaintedLayerCallback;
  void *mPaintedLayerCallbackData;

  RefPtr<WebRenderBridgeChild> mWrChild;

  RefPtr<TransactionIdAllocator> mTransactionIdAllocator;
  uint64_t mLatestTransactionId;

  nsTArray<DidCompositeObserver*> mDidCompositeObservers;

  LayerRefArray mKeepAlive;

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

  // Layers that have been mutated. If we have an empty transaction
  // then a display item layer will no longer be valid
  // if it was a mutated layers.
  void AddMutatedLayer(Layer* aLayer);
  void ClearMutatedLayers();
  LayerRefArray mMutatedLayers;
  bool mTransactionIncomplete;

  bool mNeedsComposite;
  bool mIsFirstPaint;
  FocusTarget mFocusTarget;
  bool mEndTransactionWithoutLayers;

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

  // True if the layers-free transaction has invalidation region and then
  // we should send notification after EndTransaction
  bool mShouldNotifyInvalidation;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERLAYERMANAGER_H */
