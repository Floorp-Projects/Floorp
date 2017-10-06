/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCommandBuilder.h"

#include "BasicLayers.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/ScrollingLayersHelper.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/UpdateImageHelper.h"
#include "nsDisplayListInvalidation.h"
#include "WebRenderCanvasRenderer.h"
#include "LayerTreeInvalidation.h"

namespace mozilla {
namespace layers {

void WebRenderCommandBuilder::Destroy()
{
  mLastCanvasDatas.Clear();
  RemoveUnusedAndResetWebRenderUserData();
}

void
WebRenderCommandBuilder::BuildWebRenderCommands(wr::DisplayListBuilder& aBuilder,
                                                wr::IpcResourceUpdateQueue& aResourceUpdates,
                                                nsDisplayList* aDisplayList,
                                                nsDisplayListBuilder* aDisplayListBuilder,
                                                WebRenderScrollData& aScrollData,
                                                wr::LayoutSize& aContentSize)
{
  { // scoping for StackingContextHelper RAII

    StackingContextHelper sc;
    mParentCommands.Clear();
    aScrollData = WebRenderScrollData();
    MOZ_ASSERT(mLayerScrollData.empty());
    mLastCanvasDatas.Clear();
    mLastAsr = nullptr;

    CreateWebRenderCommandsFromDisplayList(aDisplayList, aDisplayListBuilder, sc,
                                           aBuilder, aResourceUpdates);

    // Make a "root" layer data that has everything else as descendants
    mLayerScrollData.emplace_back();
    mLayerScrollData.back().InitializeRoot(mLayerScrollData.size() - 1);
    if (aDisplayListBuilder->IsBuildingLayerEventRegions()) {
      nsIPresShell* shell = aDisplayListBuilder->RootReferenceFrame()->PresContext()->PresShell();
      if (nsLayoutUtils::HasDocumentLevelListenersForApzAwareEvents(shell)) {
        mLayerScrollData.back().SetEventRegionsOverride(EventRegionsOverride::ForceDispatchToContent);
      }
    }
    auto callback = [&aScrollData](FrameMetrics::ViewID aScrollId) -> bool {
      return aScrollData.HasMetadataFor(aScrollId);
    };
    if (Maybe<ScrollMetadata> rootMetadata = nsLayoutUtils::GetRootMetadata(
          aDisplayListBuilder, nullptr, ContainerLayerParameters(), callback)) {
      mLayerScrollData.back().AppendScrollMetadata(aScrollData, rootMetadata.ref());
    }
    // Append the WebRenderLayerScrollData items into WebRenderScrollData
    // in reverse order, from topmost to bottommost. This is in keeping with
    // the semantics of WebRenderScrollData.
    for (auto i = mLayerScrollData.crbegin(); i != mLayerScrollData.crend(); i++) {
      aScrollData.AddLayerData(*i);
    }
    mLayerScrollData.clear();
    mClipIdCache.clear();

    // Remove the user data those are not displayed on the screen and
    // also reset the data to unused for next transaction.
    RemoveUnusedAndResetWebRenderUserData();
  }

  mManager->WrBridge()->AddWebRenderParentCommands(mParentCommands);
}

void
WebRenderCommandBuilder::CreateWebRenderCommandsFromDisplayList(nsDisplayList* aDisplayList,
                                                                nsDisplayListBuilder* aDisplayListBuilder,
                                                                const StackingContextHelper& aSc,
                                                                wr::DisplayListBuilder& aBuilder,
                                                                wr::IpcResourceUpdateQueue& aResources)
{
  bool apzEnabled = mManager->AsyncPanZoomEnabled();
  EventRegions eventRegions;

  for (nsDisplayItem* i = aDisplayList->GetBottom(); i; i = i->GetAbove()) {
    nsDisplayItem* item = i;
    DisplayItemType itemType = item->GetType();

    // If the item is a event regions item, but is empty (has no regions in it)
    // then we should just throw it out
    if (itemType == DisplayItemType::TYPE_LAYER_EVENT_REGIONS) {
      nsDisplayLayerEventRegions* eventRegions =
        static_cast<nsDisplayLayerEventRegions*>(item);
      if (eventRegions->IsEmpty()) {
        continue;
      }
    }

    // Peek ahead to the next item and try merging with it or swapping with it
    // if necessary.
    AutoTArray<nsDisplayItem*, 1> mergedItems;
    mergedItems.AppendElement(item);
    for (nsDisplayItem* peek = item->GetAbove(); peek; peek = peek->GetAbove()) {
      if (!item->CanMerge(peek)) {
        break;
      }

      mergedItems.AppendElement(peek);

      // Move the iterator forward since we will merge this item.
      i = peek;
    }

    if (mergedItems.Length() > 1) {
      item = aDisplayListBuilder->MergeItems(mergedItems);
      MOZ_ASSERT(item && itemType == item->GetType());
    }

    bool forceNewLayerData = false;
    size_t layerCountBeforeRecursing = mLayerScrollData.size();
    if (apzEnabled) {
      // For some types of display items we want to force a new
      // WebRenderLayerScrollData object, to ensure we preserve the APZ-relevant
      // data that is in the display item.
      forceNewLayerData = item->UpdateScrollData(nullptr, nullptr);

      // Anytime the ASR changes we also want to force a new layer data because
      // the stack of scroll metadata is going to be different for this
      // display item than previously, so we can't squash the display items
      // into the same "layer".
      const ActiveScrolledRoot* asr = item->GetActiveScrolledRoot();
      if (asr != mLastAsr) {
        mLastAsr = asr;
        forceNewLayerData = true;
      }

      // If we're creating a new layer data then flush whatever event regions
      // we've collected onto the old layer.
      if (forceNewLayerData && !eventRegions.IsEmpty()) {
        // If eventRegions is non-empty then we must have a layer data already,
        // because we (below) force one if we encounter an event regions item
        // with an empty layer data list. Additionally, the most recently
        // created layer data must have been created from an item whose ASR
        // is the same as the ASR on the event region items that were collapsed
        // into |eventRegions|. This is because any ASR change causes us to force
        // a new layer data which flushes the eventRegions.
        MOZ_ASSERT(!mLayerScrollData.empty());
        mLayerScrollData.back().AddEventRegions(eventRegions);
        eventRegions.SetEmpty();
      }

      // Collapse event region data into |eventRegions|, which will either be
      // empty, or filled with stuff from previous display items with the same
      // ASR.
      if (itemType == DisplayItemType::TYPE_LAYER_EVENT_REGIONS) {
        nsDisplayLayerEventRegions* regionsItem =
            static_cast<nsDisplayLayerEventRegions*>(item);
        int32_t auPerDevPixel = item->Frame()->PresContext()->AppUnitsPerDevPixel();
        EventRegions regions(
            regionsItem->HitRegion().ScaleToOutsidePixels(1.0f, 1.0f, auPerDevPixel),
            regionsItem->MaybeHitRegion().ScaleToOutsidePixels(1.0f, 1.0f, auPerDevPixel),
            regionsItem->DispatchToContentHitRegion().ScaleToOutsidePixels(1.0f, 1.0f, auPerDevPixel),
            regionsItem->NoActionRegion().ScaleToOutsidePixels(1.0f, 1.0f, auPerDevPixel),
            regionsItem->HorizontalPanRegion().ScaleToOutsidePixels(1.0f, 1.0f, auPerDevPixel),
            regionsItem->VerticalPanRegion().ScaleToOutsidePixels(1.0f, 1.0f, auPerDevPixel));

        eventRegions.OrWith(regions);
        if (mLayerScrollData.empty()) {
          // If we don't have a layer data yet then create one because we will
          // need it to store this event region information.
          forceNewLayerData = true;
        }
      }

      // If we're going to create a new layer data for this item, stash the
      // ASR so that if we recurse into a sublist they will know where to stop
      // walking up their ASR chain when building scroll metadata.
      if (forceNewLayerData) {
        mAsrStack.push_back(asr);
      }
    }

    nsDisplayList* childItems = item->GetSameCoordinateSystemChildren();
    if (item->ShouldFlattenAway(aDisplayListBuilder)) {
      MOZ_ASSERT(childItems);
      CreateWebRenderCommandsFromDisplayList(childItems, aDisplayListBuilder, aSc,
                                             aBuilder, aResources);
    } else {
      // ensure the scope of ScrollingLayersHelper is maintained
      ScrollingLayersHelper clip(item, aBuilder, aSc, mClipIdCache, apzEnabled);

      // Note: this call to CreateWebRenderCommands can recurse back into
      // this function if the |item| is a wrapper for a sublist.
      if (!item->CreateWebRenderCommands(aBuilder, aResources, aSc, mManager,
                                         aDisplayListBuilder)) {
        PushItemAsImage(item, aBuilder, aResources, aSc, aDisplayListBuilder);
      }
    }

    if (apzEnabled) {
      if (forceNewLayerData) {
        // Pop the thing we pushed before the recursion, so the topmost item on
        // the stack is enclosing display item's ASR (or the stack is empty)
        mAsrStack.pop_back();
        const ActiveScrolledRoot* stopAtAsr =
            mAsrStack.empty() ? nullptr : mAsrStack.back();

        int32_t descendants = mLayerScrollData.size() - layerCountBeforeRecursing;

        mLayerScrollData.emplace_back();
        mLayerScrollData.back().Initialize(mManager->GetScrollData(), item, descendants, stopAtAsr);
      } else if (mLayerScrollData.size() != layerCountBeforeRecursing &&
                 !eventRegions.IsEmpty()) {
        // We are not forcing a new layer for |item|, but we did create some
        // layers while recursing. In this case, we need to make sure any
        // event regions that we were carrying end up on the right layer. So we
        // do an event region "flush" but retroactively; i.e. the event regions
        // end up on the layer that was mLayerScrollData.back() prior to the
        // recursion.
        MOZ_ASSERT(layerCountBeforeRecursing > 0);
        mLayerScrollData[layerCountBeforeRecursing - 1].AddEventRegions(eventRegions);
        eventRegions.SetEmpty();
      }
    }
  }

  // If we have any event region info left over we need to flush it before we
  // return. Again, at this point the layer data list must be non-empty, and
  // the most recently created layer data will have been created by an item
  // with matching ASRs.
  if (!eventRegions.IsEmpty()) {
    MOZ_ASSERT(apzEnabled);
    MOZ_ASSERT(!mLayerScrollData.empty());
    mLayerScrollData.back().AddEventRegions(eventRegions);
  }
}

Maybe<wr::ImageKey>
WebRenderCommandBuilder::CreateImageKey(nsDisplayItem* aItem,
                                        ImageContainer* aContainer,
                                        mozilla::wr::DisplayListBuilder& aBuilder,
                                        mozilla::wr::IpcResourceUpdateQueue& aResources,
                                        const StackingContextHelper& aSc,
                                        gfx::IntSize& aSize)
{
  RefPtr<WebRenderImageData> imageData = CreateOrRecycleWebRenderUserData<WebRenderImageData>(aItem);
  MOZ_ASSERT(imageData);

  if (aContainer->IsAsync()) {
    bool snap;
    nsRect bounds = aItem->GetBounds(nullptr, &snap);
    int32_t appUnitsPerDevPixel = aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
    LayerRect rect = ViewAs<LayerPixel>(
      LayoutDeviceRect::FromAppUnits(bounds, appUnitsPerDevPixel),
      PixelCastJustification::WebRenderHasUnitResolution);
    LayerRect scBounds(0, 0, rect.width, rect.Height());
    gfx::MaybeIntSize scaleToSize;
    if (!aContainer->GetScaleHint().IsEmpty()) {
      scaleToSize = Some(aContainer->GetScaleHint());
    }
    // TODO!
    // We appear to be using the image bridge for a lot (most/all?) of
    // layers-free image handling and that breaks frame consistency.
    imageData->CreateAsyncImageWebRenderCommands(aBuilder,
                                                 aContainer,
                                                 aSc,
                                                 rect,
                                                 scBounds,
                                                 gfx::Matrix4x4(),
                                                 scaleToSize,
                                                 wr::ImageRendering::Auto,
                                                 wr::MixBlendMode::Normal,
                                                 !aItem->BackfaceIsHidden());
    return Nothing();
  }

  AutoLockImage autoLock(aContainer);
  if (!autoLock.HasImage()) {
    return Nothing();
  }
  mozilla::layers::Image* image = autoLock.GetImage();
  aSize = image->GetSize();

  return imageData->UpdateImageKey(aContainer, aResources);
}

bool
WebRenderCommandBuilder::PushImage(nsDisplayItem* aItem,
                                   ImageContainer* aContainer,
                                   mozilla::wr::DisplayListBuilder& aBuilder,
                                   mozilla::wr::IpcResourceUpdateQueue& aResources,
                                   const StackingContextHelper& aSc,
                                   const LayerRect& aRect)
{
  gfx::IntSize size;
  Maybe<wr::ImageKey> key = CreateImageKey(aItem, aContainer,
                                           aBuilder, aResources,
                                           aSc, size);
  if (aContainer->IsAsync()) {
    // Async ImageContainer does not create ImageKey, instead it uses Pipeline.
    MOZ_ASSERT(key.isNothing());
    return true;
  }
  if (!key) {
    return false;
  }

  auto r = aSc.ToRelativeLayoutRect(aRect);
  gfx::SamplingFilter sampleFilter = nsLayoutUtils::GetSamplingFilterForFrame(aItem->Frame());
  aBuilder.PushImage(r, r, !aItem->BackfaceIsHidden(), wr::ToImageRendering(sampleFilter), key.value());

  return true;
}

static void
PaintItemByDrawTarget(nsDisplayItem* aItem,
                      gfx::DrawTarget* aDT,
                      const LayerRect& aImageRect,
                      const LayerPoint& aOffset,
                      nsDisplayListBuilder* aDisplayListBuilder,
                      RefPtr<BasicLayerManager>& aManager,
                      WebRenderLayerManager* aWrManager,
                      const gfx::Size& aScale)
{
  MOZ_ASSERT(aDT);

  aDT->ClearRect(aImageRect.ToUnknownRect());
  RefPtr<gfxContext> context = gfxContext::CreateOrNull(aDT);
  MOZ_ASSERT(context);

  context->SetMatrix(context->CurrentMatrix().PreScale(aScale.width, aScale.height).PreTranslate(-aOffset.x, -aOffset.y));

  switch (aItem->GetType()) {
  case DisplayItemType::TYPE_MASK:
    static_cast<nsDisplayMask*>(aItem)->PaintMask(aDisplayListBuilder, context);
    break;
  case DisplayItemType::TYPE_FILTER:
    {
      if (aManager == nullptr) {
        aManager = new BasicLayerManager(BasicLayerManager::BLM_INACTIVE);
      }

      FrameLayerBuilder* layerBuilder = new FrameLayerBuilder();
      layerBuilder->Init(aDisplayListBuilder, aManager);
      layerBuilder->DidBeginRetainedLayerTransaction(aManager);

      aManager->BeginTransactionWithTarget(context);

      ContainerLayerParameters param;
      RefPtr<Layer> layer =
        static_cast<nsDisplayFilter*>(aItem)->BuildLayer(aDisplayListBuilder,
                                                         aManager, param);

      if (layer) {
        UniquePtr<LayerProperties> props;
        props = Move(LayerProperties::CloneFrom(aManager->GetRoot()));

        aManager->SetRoot(layer);
        layerBuilder->WillEndTransaction();

        static_cast<nsDisplayFilter*>(aItem)->PaintAsLayer(aDisplayListBuilder,
                                                           context, aManager);
      }

      if (aManager->InTransaction()) {
        aManager->AbortTransaction();
      }
      aManager->SetTarget(nullptr);
      break;
    }
  default:
    aItem->Paint(aDisplayListBuilder, context);
    break;
  }

  if (gfxPrefs::WebRenderHighlightPaintedLayers()) {
    aDT->SetTransform(gfx::Matrix());
    aDT->FillRect(gfx::Rect(0, 0, aImageRect.Width(), aImageRect.Height()), gfx::ColorPattern(gfx::Color(1.0, 0.0, 0.0, 0.5)));
  }
  if (aItem->Frame()->PresContext()->GetPaintFlashing()) {
    aDT->SetTransform(gfx::Matrix());
    float r = float(rand()) / RAND_MAX;
    float g = float(rand()) / RAND_MAX;
    float b = float(rand()) / RAND_MAX;
    aDT->FillRect(gfx::Rect(0, 0, aImageRect.Width(), aImageRect.Height()), gfx::ColorPattern(gfx::Color(r, g, b, 0.5)));
  }
}

already_AddRefed<WebRenderFallbackData>
WebRenderCommandBuilder::GenerateFallbackData(nsDisplayItem* aItem,
                                              wr::DisplayListBuilder& aBuilder,
                                              wr::IpcResourceUpdateQueue& aResources,
                                              const StackingContextHelper& aSc,
                                              nsDisplayListBuilder* aDisplayListBuilder,
                                              LayerRect& aImageRect)
{
  RefPtr<WebRenderFallbackData> fallbackData = CreateOrRecycleWebRenderUserData<WebRenderFallbackData>(aItem);

  bool snap;
  nsRect itemBounds = aItem->GetBounds(aDisplayListBuilder, &snap);
  nsRect clippedBounds = itemBounds;

  const DisplayItemClip& clip = aItem->GetClip();
  // Blob images will only draw the visible area of the blob so we don't need to clip
  // them here and can just rely on the webrender clipping.
  if (clip.HasClip() && !gfxPrefs::WebRenderBlobImages()) {
    clippedBounds = itemBounds.Intersect(clip.GetClipRect());
  }

  // nsDisplayItem::Paint() may refer the variables that come from ComputeVisibility().
  // So we should call RecomputeVisibility() before painting. e.g.: nsDisplayBoxShadowInner
  // uses mVisibleRegion in Paint() and mVisibleRegion is computed in
  // nsDisplayBoxShadowInner::ComputeVisibility().
  nsRegion visibleRegion(clippedBounds);
  aItem->RecomputeVisibility(aDisplayListBuilder, &visibleRegion);

  const int32_t appUnitsPerDevPixel = aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
  LayerRect bounds = ViewAs<LayerPixel>(
      LayoutDeviceRect::FromAppUnits(clippedBounds, appUnitsPerDevPixel),
      PixelCastJustification::WebRenderHasUnitResolution);

  gfx::Size scale = aSc.GetInheritedScale();
  LayerIntSize paintSize = RoundedToInt(LayerSize(bounds.width * scale.width, bounds.height * scale.height));
  if (paintSize.width == 0 || paintSize.height == 0) {
    return nullptr;
  }

  bool needPaint = true;
  LayerIntPoint offset = RoundedToInt(bounds.TopLeft());
  aImageRect = LayerRect(offset, LayerSize(RoundedToInt(bounds.Size())));
  LayerRect paintRect = LayerRect(LayerPoint(0, 0), LayerSize(paintSize));
  nsAutoPtr<nsDisplayItemGeometry> geometry = fallbackData->GetGeometry();

  // nsDisplayFilter is rendered via BasicLayerManager which means the invalidate
  // region is unknown until we traverse the displaylist contained by it.
  if (geometry && !fallbackData->IsInvalid() &&
      aItem->GetType() != DisplayItemType::TYPE_FILTER) {
    nsRect invalid;
    nsRegion invalidRegion;

    if (aItem->IsInvalid(invalid)) {
      invalidRegion.OrWith(clippedBounds);
    } else {
      nsPoint shift = itemBounds.TopLeft() - geometry->mBounds.TopLeft();
      geometry->MoveBy(shift);
      aItem->ComputeInvalidationRegion(aDisplayListBuilder, geometry, &invalidRegion);

      nsRect lastBounds = fallbackData->GetBounds();
      lastBounds.MoveBy(shift);

      if (!lastBounds.IsEqualInterior(clippedBounds)) {
        invalidRegion.OrWith(lastBounds);
        invalidRegion.OrWith(clippedBounds);
      }
    }
    needPaint = !invalidRegion.IsEmpty();
  }

  if (needPaint) {
    gfx::SurfaceFormat format = aItem->GetType() == DisplayItemType::TYPE_MASK ?
                                                      gfx::SurfaceFormat::A8 : gfx::SurfaceFormat::B8G8R8A8;
    if (gfxPrefs::WebRenderBlobImages()) {
      bool snapped;
      bool isOpaque = aItem->GetOpaqueRegion(aDisplayListBuilder, &snapped).Contains(clippedBounds);

      RefPtr<gfx::DrawEventRecorderMemory> recorder = MakeAndAddRef<gfx::DrawEventRecorderMemory>();
      RefPtr<gfx::DrawTarget> dummyDt =
        gfx::Factory::CreateDrawTarget(gfx::BackendType::SKIA, gfx::IntSize(1, 1), format);
      RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateRecordingDrawTarget(recorder, dummyDt, paintSize.ToUnknownSize());
      PaintItemByDrawTarget(aItem, dt, paintRect, offset, aDisplayListBuilder,
                            fallbackData->mBasicLayerManager, mManager, scale);
      recorder->Finish();

      Range<uint8_t> bytes((uint8_t*)recorder->mOutputStream.mData, recorder->mOutputStream.mLength);
      wr::ImageKey key = mManager->WrBridge()->GetNextImageKey();
      wr::ImageDescriptor descriptor(paintSize.ToUnknownSize(), 0, dt->GetFormat(), isOpaque);
      aResources.AddBlobImage(key, descriptor, bytes);
      fallbackData->SetKey(key);
    } else {
      fallbackData->CreateImageClientIfNeeded();
      RefPtr<ImageClient> imageClient = fallbackData->GetImageClient();
      RefPtr<ImageContainer> imageContainer = LayerManager::CreateImageContainer();

      {
        UpdateImageHelper helper(imageContainer, imageClient, paintSize.ToUnknownSize(), format);
        {
          RefPtr<gfx::DrawTarget> dt = helper.GetDrawTarget();
          if (!dt) {
            return nullptr;
          }
          PaintItemByDrawTarget(aItem, dt, paintRect, offset,
                                aDisplayListBuilder,
                                fallbackData->mBasicLayerManager, mManager, scale);
        }
        if (!helper.UpdateImage()) {
          return nullptr;
        }
      }

      // Force update the key in fallback data since we repaint the image in this path.
      // If not force update, fallbackData may reuse the original key because it
      // doesn't know UpdateImageHelper already updated the image container.
      if (!fallbackData->UpdateImageKey(imageContainer, aResources, true)) {
        return nullptr;
      }
    }

    geometry = aItem->AllocateGeometry(aDisplayListBuilder);
    fallbackData->SetInvalid(false);
  }

  // Update current bounds to fallback data
  fallbackData->SetGeometry(Move(geometry));
  fallbackData->SetBounds(clippedBounds);

  MOZ_ASSERT(fallbackData->GetKey());

  return fallbackData.forget();
}

Maybe<wr::WrImageMask>
WebRenderCommandBuilder::BuildWrMaskImage(nsDisplayItem* aItem,
                                          wr::DisplayListBuilder& aBuilder,
                                          wr::IpcResourceUpdateQueue& aResources,
                                          const StackingContextHelper& aSc,
                                          nsDisplayListBuilder* aDisplayListBuilder,
                                          const LayerRect& aBounds)
{
  LayerRect imageRect;
  RefPtr<WebRenderFallbackData> fallbackData = GenerateFallbackData(aItem, aBuilder, aResources,
                                                                    aSc, aDisplayListBuilder,
                                                                    imageRect);
  if (!fallbackData) {
    return Nothing();
  }

  wr::WrImageMask imageMask;
  imageMask.image = fallbackData->GetKey().value();
  imageMask.rect = aSc.ToRelativeLayoutRect(aBounds);
  imageMask.repeat = false;
  return Some(imageMask);
}

bool
WebRenderCommandBuilder::PushItemAsImage(nsDisplayItem* aItem,
                                         wr::DisplayListBuilder& aBuilder,
                                         wr::IpcResourceUpdateQueue& aResources,
                                         const StackingContextHelper& aSc,
                                         nsDisplayListBuilder* aDisplayListBuilder)
{
  LayerRect imageRect;
  RefPtr<WebRenderFallbackData> fallbackData = GenerateFallbackData(aItem, aBuilder, aResources,
                                                                    aSc, aDisplayListBuilder,
                                                                    imageRect);
  if (!fallbackData) {
    return false;
  }

  wr::LayoutRect dest = aSc.ToRelativeLayoutRect(imageRect);
  gfx::SamplingFilter sampleFilter = nsLayoutUtils::GetSamplingFilterForFrame(aItem->Frame());
  aBuilder.PushImage(dest,
                     dest,
                     !aItem->BackfaceIsHidden(),
                     wr::ToImageRendering(sampleFilter),
                     fallbackData->GetKey().value());
  return true;
}

void
WebRenderCommandBuilder::RemoveUnusedAndResetWebRenderUserData()
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

      if (data->GetType() == WebRenderUserData::UserDataType::eCanvas) {
        mLastCanvasDatas.RemoveEntry(data->AsCanvasData());
      }

      iter.Remove();
      continue;
    }

    data->SetUsed(false);
  }
}

} // namespace layers
} // namespace mozilla
