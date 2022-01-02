/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERCOMMANDBUILDER_H
#define GFX_WEBRENDERCOMMANDBUILDER_H

#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/layers/ClipManager.h"
#include "mozilla/layers/HitTestInfoManager.h"
#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/layers/WebRenderScrollData.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "mozilla/SVGIntegrationUtils.h"  // for WrFiltersHolder
#include "nsDisplayList.h"
#include "nsIFrame.h"
#include "nsTHashSet.h"
#include "DisplayItemCache.h"
#include "ImgDrawResult.h"

namespace mozilla {

namespace image {
class WebRenderImageProvider;
}

namespace layers {

class ImageClient;
class ImageContainer;
class WebRenderBridgeChild;
class WebRenderCanvasData;
class WebRenderCanvasRendererAsync;
class WebRenderImageData;
class WebRenderFallbackData;
class WebRenderParentCommand;
class WebRenderUserData;

class WebRenderCommandBuilder final {
  typedef nsTHashSet<RefPtr<WebRenderUserData>> WebRenderUserDataRefTable;
  typedef nsTHashSet<RefPtr<WebRenderCanvasData>> CanvasDataSet;
  typedef nsTHashSet<RefPtr<WebRenderLocalCanvasData>> LocalCanvasDataSet;

 public:
  explicit WebRenderCommandBuilder(WebRenderLayerManager* aManager);

  void Destroy();

  void EmptyTransaction();

  bool NeedsEmptyTransaction();

  void BuildWebRenderCommands(wr::DisplayListBuilder& aBuilder,
                              wr::IpcResourceUpdateQueue& aResourceUpdates,
                              nsDisplayList* aDisplayList,
                              nsDisplayListBuilder* aDisplayListBuilder,
                              WebRenderScrollData& aScrollData,
                              WrFiltersHolder&& aFilters);

  void PushOverrideForASR(const ActiveScrolledRoot* aASR,
                          const wr::WrSpatialId& aSpatialId);
  void PopOverrideForASR(const ActiveScrolledRoot* aASR);

  Maybe<wr::ImageKey> CreateImageKey(
      nsDisplayItem* aItem, ImageContainer* aContainer,
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      mozilla::wr::ImageRendering aRendering, const StackingContextHelper& aSc,
      gfx::IntSize& aSize, const Maybe<LayoutDeviceRect>& aAsyncImageBounds);

  Maybe<wr::ImageKey> CreateImageProviderKey(
      nsDisplayItem* aItem, image::WebRenderImageProvider* aProvider,
      image::ImgDrawResult aDrawResult,
      mozilla::wr::IpcResourceUpdateQueue& aResources);

  WebRenderUserDataRefTable* GetWebRenderUserDataTable() {
    return &mWebRenderUserDatas;
  }

  bool PushImage(nsDisplayItem* aItem, ImageContainer* aContainer,
                 mozilla::wr::DisplayListBuilder& aBuilder,
                 mozilla::wr::IpcResourceUpdateQueue& aResources,
                 const StackingContextHelper& aSc,
                 const LayoutDeviceRect& aRect, const LayoutDeviceRect& aClip);

  bool PushImageProvider(nsDisplayItem* aItem,
                         image::WebRenderImageProvider* aProvider,
                         image::ImgDrawResult aDrawResult,
                         mozilla::wr::DisplayListBuilder& aBuilder,
                         mozilla::wr::IpcResourceUpdateQueue& aResources,
                         const LayoutDeviceRect& aRect,
                         const LayoutDeviceRect& aClip);

  Maybe<wr::ImageMask> BuildWrMaskImage(
      nsDisplayMasksAndClipPaths* aMaskItem, wr::DisplayListBuilder& aBuilder,
      wr::IpcResourceUpdateQueue& aResources, const StackingContextHelper& aSc,
      nsDisplayListBuilder* aDisplayListBuilder,
      const LayoutDeviceRect& aBounds);

  bool PushItemAsImage(nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
                       wr::IpcResourceUpdateQueue& aResources,
                       const StackingContextHelper& aSc,
                       nsDisplayListBuilder* aDisplayListBuilder);

  void CreateWebRenderCommandsFromDisplayList(
      nsDisplayList* aDisplayList, nsDisplayItem* aWrappingItem,
      nsDisplayListBuilder* aDisplayListBuilder,
      const StackingContextHelper& aSc, wr::DisplayListBuilder& aBuilder,
      wr::IpcResourceUpdateQueue& aResources, bool aNewClipList = true);

  // aWrappingItem has to be non-null.
  void DoGroupingForDisplayList(nsDisplayList* aDisplayList,
                                nsDisplayItem* aWrappingItem,
                                nsDisplayListBuilder* aDisplayListBuilder,
                                const StackingContextHelper& aSc,
                                wr::DisplayListBuilder& aBuilder,
                                wr::IpcResourceUpdateQueue& aResources);

  already_AddRefed<WebRenderFallbackData> GenerateFallbackData(
      nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
      wr::IpcResourceUpdateQueue& aResources, const StackingContextHelper& aSc,
      nsDisplayListBuilder* aDisplayListBuilder, LayoutDeviceRect& aImageRect);

  void RemoveUnusedAndResetWebRenderUserData();
  void ClearCachedResources();

  bool ShouldDumpDisplayList(nsDisplayListBuilder* aBuilder);
  wr::usize GetBuilderDumpIndex() const { return mBuilderDumpIndex; }

  bool GetContainsSVGGroup() { return mContainsSVGGroup; }

  // Those are data that we kept between transactions. We used to cache some
  // data in the layer. But in layers free mode, we don't have layer which
  // means we need some other place to cached the data between transaction.
  // We store the data in frame's property.
  template <class T>
  already_AddRefed<T> CreateOrRecycleWebRenderUserData(
      nsDisplayItem* aItem, bool* aOutIsRecycled = nullptr) {
    MOZ_ASSERT(aItem);
    nsIFrame* frame = aItem->Frame();
    if (aOutIsRecycled) {
      *aOutIsRecycled = true;
    }

    WebRenderUserDataTable* userDataTable =
        frame->GetProperty(WebRenderUserDataProperty::Key());

    if (!userDataTable) {
      userDataTable = new WebRenderUserDataTable();
      frame->AddProperty(WebRenderUserDataProperty::Key(), userDataTable);
    }

    RefPtr<WebRenderUserData>& data = userDataTable->LookupOrInsertWith(
        WebRenderUserDataKey(aItem->GetPerFrameKey(), T::Type()), [&] {
          auto data = MakeRefPtr<T>(GetRenderRootStateManager(), aItem);
          mWebRenderUserDatas.Insert(data);
          if (aOutIsRecycled) {
            *aOutIsRecycled = false;
          }
          return data;
        });

    MOZ_ASSERT(data);
    MOZ_ASSERT(data->GetType() == T::Type());

    // Mark the data as being used. We will remove unused user data in the end
    // of EndTransaction.
    data->SetUsed(true);

    switch (T::Type()) {
      case WebRenderUserData::UserDataType::eCanvas:
        mLastCanvasDatas.Insert(data->AsCanvasData());
        break;
      case WebRenderUserData::UserDataType::eLocalCanvas:
        mLastLocalCanvasDatas.Insert(data->AsLocalCanvasData());
        break;
      default:
        break;
    }

    RefPtr<T> res = static_cast<T*>(data.get());
    return res.forget();
  }

  WebRenderLayerManager* mManager;

 private:
  RenderRootStateManager* GetRenderRootStateManager();
  void CreateWebRenderCommands(nsDisplayItem* aItem,
                               mozilla::wr::DisplayListBuilder& aBuilder,
                               mozilla::wr::IpcResourceUpdateQueue& aResources,
                               const StackingContextHelper& aSc,
                               nsDisplayListBuilder* aDisplayListBuilder);

  bool ComputeInvalidationForDisplayItem(nsDisplayListBuilder* aBuilder,
                                         const nsPoint& aShift,
                                         nsDisplayItem* aItem);
  bool ComputeInvalidationForDisplayList(nsDisplayListBuilder* aBuilder,
                                         const nsPoint& aShift,
                                         nsDisplayList* aList);

  ClipManager mClipManager;
  HitTestInfoManager mHitTestInfoManager;

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
  // Store of WebRenderLocalCanvasData objects for use in empty transactions
  LocalCanvasDataSet mLastLocalCanvasDatas;

  wr::usize mBuilderDumpIndex;
  wr::usize mDumpIndent;

 public:
  // Whether consecutive inactive display items should be grouped into one
  // blob image.
  bool mDoGrouping;

  // True if the most recently build display list contained an svg that
  // we did grouping for.
  bool mContainsSVGGroup;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_WEBRENDERCOMMANDBUILDER_H */
