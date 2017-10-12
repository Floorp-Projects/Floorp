/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERCOMMANDBUILDER_H
#define GFX_WEBRENDERCOMMANDBUILDER_H

#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/layers/WebRenderScrollData.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "nsDisplayList.h"
#include "nsIFrame.h"

namespace mozilla {

namespace layers {

class CanvasLayer;
class ImageClient;
class ImageContainer;
class WebRenderBridgeChild;
class WebRenderCanvasData;
class WebRenderCanvasRendererAsync;
class WebRenderImageData;
class WebRenderFallbackData;
class WebRenderParentCommand;
class WebRenderUserData;

class WebRenderCommandBuilder {
  typedef nsTHashtable<nsRefPtrHashKey<WebRenderUserData>> WebRenderUserDataRefTable;
  typedef nsTHashtable<nsRefPtrHashKey<WebRenderCanvasData>> CanvasDataSet;

public:
  explicit WebRenderCommandBuilder(WebRenderLayerManager* aManager)
  : mManager(aManager)
  , mLastAsr(nullptr)
  {}

  void Destroy();

  void EmptyTransaction();

  void BuildWebRenderCommands(wr::DisplayListBuilder& aBuilder,
                              wr::IpcResourceUpdateQueue& aResourceUpdates,
                              nsDisplayList* aDisplayList,
                              nsDisplayListBuilder* aDisplayListBuilder,
                              WebRenderScrollData& aScrollData,
                              wr::LayoutSize& aContentSize);

  Maybe<wr::ImageKey> CreateImageKey(nsDisplayItem* aItem,
                                     ImageContainer* aContainer,
                                     mozilla::wr::DisplayListBuilder& aBuilder,
                                     mozilla::wr::IpcResourceUpdateQueue& aResources,
                                     const StackingContextHelper& aSc,
                                     gfx::IntSize& aSize);

  WebRenderUserDataRefTable* GetWebRenderUserDataTable() { return &mWebRenderUserDatas; }

  bool PushImage(nsDisplayItem* aItem,
                 ImageContainer* aContainer,
                 mozilla::wr::DisplayListBuilder& aBuilder,
                 mozilla::wr::IpcResourceUpdateQueue& aResources,
                 const StackingContextHelper& aSc,
                 const LayerRect& aRect);

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

  already_AddRefed<WebRenderFallbackData> GenerateFallbackData(nsDisplayItem* aItem,
                                                               wr::DisplayListBuilder& aBuilder,
                                                               wr::IpcResourceUpdateQueue& aResources,
                                                               const StackingContextHelper& aSc,
                                                               nsDisplayListBuilder* aDisplayListBuilder,
                                                               LayerRect& aImageRect);

  void RemoveUnusedAndResetWebRenderUserData();

  // Those are data that we kept between transactions. We used to cache some
  // data in the layer. But in layers free mode, we don't have layer which
  // means we need some other place to cached the data between transaction.
  // We store the data in frame's property.
  template<class T> already_AddRefed<T>
  CreateOrRecycleWebRenderUserData(nsDisplayItem* aItem,
                                   bool* aOutIsRecycled = nullptr)
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
    if (!data || (data->GetType() != T::Type()) || !data->IsDataValid(mManager)) {
      // To recreate a new user data, we should remove the data from the table first.
      if (data) {
        data->RemoveFromTable();
      }
      data = new T(mManager, aItem);
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
  WebRenderLayerManager* mManager;
  ClipIdMap mClipIdCache;

  // These fields are used to save a copy of the display list for
  // empty transactions in layers-free mode.
  nsTArray<WebRenderParentCommand> mParentCommands;

  // We use this as a temporary data structure while building the mScrollData
  // inside a layers-free transaction.
  std::vector<WebRenderLayerScrollData> mLayerScrollData;
  // We use this as a temporary data structure to track the current display
  // item's ASR as we recurse in CreateWebRenderCommandsFromDisplayList. We
  // need this so that WebRenderLayerScrollData items that deeper in the
  // tree don't duplicate scroll metadata that their ancestors already have.
  std::vector<const ActiveScrolledRoot*> mAsrStack;
  const ActiveScrolledRoot* mLastAsr;

  WebRenderUserDataRefTable mWebRenderUserDatas;

  // Store of WebRenderCanvasData objects for use in empty transactions
  CanvasDataSet mLastCanvasDatas;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERCOMMANDBUILDER_H */
