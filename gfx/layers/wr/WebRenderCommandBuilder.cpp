/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCommandBuilder.h"

#include "BasicLayers.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/AnimationHelper.h"
#include "mozilla/layers/ClipManager.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/SharedSurfacesChild.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/UpdateImageHelper.h"
#include "mozilla/layers/WebRenderDrawEventRecorder.h"
#include "UnitTransforms.h"
#include "gfxEnv.h"
#include "nsDisplayListInvalidation.h"
#include "WebRenderCanvasRenderer.h"
#include "LayersLogging.h"
#include "LayerTreeInvalidation.h"

namespace mozilla {
namespace layers {

using namespace gfx;
static bool PaintByLayer(nsDisplayItem* aItem,
                         nsDisplayListBuilder* aDisplayListBuilder,
                         const RefPtr<BasicLayerManager>& aManager,
                         gfxContext* aContext, const gfx::Size& aScale,
                         const std::function<void()>& aPaintFunc);
static int sIndent;
#include <stdarg.h>
#include <stdio.h>

static void GP(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
#if 0
    for (int i = 0; i < sIndent; i++) { printf(" "); }
    vprintf(fmt, args);
#endif
  va_end(args);
}

// XXX: problems:
// - How do we deal with scrolling while having only a single invalidation rect?
// We can have a valid rect and an invalid rect. As we scroll the valid rect
// will move and the invalid rect will be the new area

struct BlobItemData;
static void DestroyBlobGroupDataProperty(nsTArray<BlobItemData*>* aArray);
NS_DECLARE_FRAME_PROPERTY_WITH_DTOR(BlobGroupDataProperty,
                                    nsTArray<BlobItemData*>,
                                    DestroyBlobGroupDataProperty);

// These are currently manually allocated and ownership is help by the
// mDisplayItems hash table in DIGroup
struct BlobItemData {
  // a weak pointer to the frame for this item.
  // DisplayItemData has a mFrameList to deal with merged frames. Hopefully we
  // don't need to worry about that.
  nsIFrame* mFrame;

  uint32_t mDisplayItemKey;
  nsTArray<BlobItemData*>*
      mArray;  // a weak pointer to the array that's owned by the frame property

  IntRect mRect;
  // It would be nice to not need this. We need to be able to call
  // ComputeInvalidationRegion. ComputeInvalidationRegion will sometimes reach
  // into parent style structs to get information that can change the
  // invalidation region
  UniquePtr<nsDisplayItemGeometry> mGeometry;
  DisplayItemClip mClip;
  bool mUsed;  // initialized near construction
  // XXX: only used for debugging
  bool mInvalid;

  // a weak pointer to the group that owns this item
  // we use this to track whether group for a particular item has changed
  struct DIGroup* mGroup;

  // properties that are used to emulate layer tree invalidation
  Matrix mMatrix;  // updated to track the current transform to device space
  RefPtr<BasicLayerManager> mLayerManager;

  // We need to keep a list of all the external surfaces used by the blob image.
  // We do this on a per-display item basis so that the lists remains correct
  // during invalidations.
  std::vector<RefPtr<SourceSurface>> mExternalSurfaces;

  IntRect mImageRect;

  BlobItemData(DIGroup* aGroup, nsDisplayItem* aItem)
      : mUsed(false), mGroup(aGroup) {
    mInvalid = false;
    mDisplayItemKey = aItem->GetPerFrameKey();
    AddFrame(aItem->Frame());
  }

 private:
  void AddFrame(nsIFrame* aFrame) {
    mFrame = aFrame;

    nsTArray<BlobItemData*>* array =
        aFrame->GetProperty(BlobGroupDataProperty());
    if (!array) {
      array = new nsTArray<BlobItemData*>();
      aFrame->SetProperty(BlobGroupDataProperty(), array);
    }
    array->AppendElement(this);
    mArray = array;
  }

 public:
  void ClearFrame() {
    // Delete the weak pointer to this BlobItemData on the frame
    MOZ_RELEASE_ASSERT(mFrame);
    // the property may already be removed if WebRenderUserData got deleted
    // first so we use our own mArray pointer.
    mArray->RemoveElement(this);

    // drop the entire property if nothing's left in the array
    if (mArray->IsEmpty()) {
      // If the frame is in the process of being destroyed this will fail
      // but that's ok, because the the property will be removed then anyways
      mFrame->RemoveProperty(BlobGroupDataProperty());
    }
    mFrame = nullptr;
  }

  ~BlobItemData() {
    if (mFrame) {
      ClearFrame();
    }
  }
};

static BlobItemData* GetBlobItemData(nsDisplayItem* aItem) {
  nsIFrame* frame = aItem->Frame();
  uint32_t key = aItem->GetPerFrameKey();
  const nsTArray<BlobItemData*>* array =
      frame->GetProperty(BlobGroupDataProperty());
  if (array) {
    for (BlobItemData* item : *array) {
      if (item->mDisplayItemKey == key) {
        return item;
      }
    }
  }
  return nullptr;
}

// We keep around the BlobItemData so that when we invalidate it get properly
// included in the rect
static void DestroyBlobGroupDataProperty(nsTArray<BlobItemData*>* aArray) {
  for (BlobItemData* item : *aArray) {
    GP("DestroyBlobGroupDataProperty: %p-%d\n", item->mFrame,
       item->mDisplayItemKey);
    item->mFrame = nullptr;
  }
  delete aArray;
}

static void TakeExternalSurfaces(
    WebRenderDrawEventRecorder* aRecorder,
    std::vector<RefPtr<SourceSurface>>& aExternalSurfaces,
    RenderRootStateManager* aManager, wr::IpcResourceUpdateQueue& aResources) {
  aRecorder->TakeExternalSurfaces(aExternalSurfaces);

  for (auto& surface : aExternalSurfaces) {
    // While we don't use the image key with the surface, because the blob image
    // renderer doesn't have easy access to the resource set, we still want to
    // ensure one is generated. That will ensure the surface remains alive until
    // at least the last epoch which the blob image could be used in.
    wr::ImageKey key;
    DebugOnly<nsresult> rv =
        SharedSurfacesChild::Share(surface, aManager, aResources, key);
    MOZ_ASSERT(rv.value != NS_ERROR_NOT_IMPLEMENTED);
  }
}

struct DIGroup;
struct Grouper {
  explicit Grouper(ClipManager& aClipManager)
      : mAppUnitsPerDevPixel(0),
        mDisplayListBuilder(nullptr),
        mClipManager(aClipManager) {}

  int32_t mAppUnitsPerDevPixel;
  nsDisplayListBuilder* mDisplayListBuilder;
  ClipManager& mClipManager;
  Matrix mTransform;

  // Paint the list of aChildren display items.
  void PaintContainerItem(DIGroup* aGroup, nsDisplayItem* aItem,
                          BlobItemData* aData, const IntRect& aItemBounds,
                          nsDisplayList* aChildren, gfxContext* aContext,
                          WebRenderDrawEventRecorder* aRecorder,
                          RenderRootStateManager* aRootManager,
                          wr::IpcResourceUpdateQueue& aResources);

  // Builds groups of display items split based on 'layer activity'
  void ConstructGroups(nsDisplayListBuilder* aDisplayListBuilder,
                       WebRenderCommandBuilder* aCommandBuilder,
                       wr::DisplayListBuilder& aBuilder,
                       wr::IpcResourceUpdateQueue& aResources, DIGroup* aGroup,
                       nsDisplayList* aList, const StackingContextHelper& aSc);
  // Builds a group of display items without promoting anything to active.
  void ConstructGroupInsideInactive(WebRenderCommandBuilder* aCommandBuilder,
                                    wr::DisplayListBuilder& aBuilder,
                                    wr::IpcResourceUpdateQueue& aResources,
                                    DIGroup* aGroup, nsDisplayList* aList,
                                    const StackingContextHelper& aSc);
  // Helper method for processing a single inactive item
  void ConstructItemInsideInactive(WebRenderCommandBuilder* aCommandBuilder,
                                   wr::DisplayListBuilder& aBuilder,
                                   wr::IpcResourceUpdateQueue& aResources,
                                   DIGroup* aGroup, nsDisplayItem* aItem,
                                   const StackingContextHelper& aSc);
  ~Grouper() = default;
};

// Returns whether this is an item for which complete invalidation was
// reliant on LayerTreeInvalidation in the pre-webrender world.
static bool IsContainerLayerItem(nsDisplayItem* aItem) {
  switch (aItem->GetType()) {
    case DisplayItemType::TYPE_WRAP_LIST:
    case DisplayItemType::TYPE_CONTAINER:
    case DisplayItemType::TYPE_TRANSFORM:
    case DisplayItemType::TYPE_OPACITY:
    case DisplayItemType::TYPE_FILTER:
    case DisplayItemType::TYPE_BLEND_CONTAINER:
    case DisplayItemType::TYPE_BLEND_MODE:
    case DisplayItemType::TYPE_MASK:
    case DisplayItemType::TYPE_PERSPECTIVE: {
      return true;
    }
    default: {
      return false;
    }
  }
}

#include <sstream>

static bool DetectContainerLayerPropertiesBoundsChange(
    nsDisplayItem* aItem, BlobItemData* aData,
    nsDisplayItemGeometry& aGeometry) {
  if (aItem->GetType() == DisplayItemType::TYPE_FILTER) {
    // Filters go through BasicLayerManager composition which clips to
    // the BuildingRect
    aGeometry.mBounds = aGeometry.mBounds.Intersect(aItem->GetBuildingRect());
  }

  return !aGeometry.mBounds.IsEqualEdges(aData->mGeometry->mBounds);
}

struct DIGroup {
  // XXX: Storing owning pointers to the BlobItemData in a hash table is not
  // a good choice. There are two better options:
  //
  // 1. We should just be using a linked list for this stuff.
  //    That we can iterate over only the used items.
  //    We remove from the unused list and add to the used list
  //    when we see an item.
  //
  //    we allocate using a free list.
  //
  // 2. We can use a Vec and use SwapRemove().
  //    We'll just need to be careful when iterating.
  //    The advantage of a Vec is that everything stays compact
  //    and we don't need to heap allocate the BlobItemData's
  nsTHashtable<nsPtrHashKey<BlobItemData>> mDisplayItems;

  IntRect mInvalidRect;
  nsRect mGroupBounds;
  LayerIntRect mVisibleRect;
  // This is the last visible rect sent to WebRender. It's used
  // to compute the invalid rect and ensure that we send
  // the appropriate data to WebRender for merging.
  LayerIntRect mLastVisibleRect;

  // This is the intersection of mVisibleRect and mLastVisibleRect
  // we ensure that mInvalidRect is contained in mPreservedRect
  IntRect mPreservedRect;
  IntRect mActualBounds;
  int32_t mAppUnitsPerDevPixel;
  gfx::Size mScale;
  ScrollableLayerGuid::ViewID mScrollId;
  CompositorHitTestInfo mHitInfo;
  LayerPoint mResidualOffset;
  LayerIntRect mLayerBounds;  // mGroupBounds converted to Layer space
  // mLayerBounds clipped to the container/parent of the
  // current item being processed.
  IntRect mClippedImageBounds;  // mLayerBounds with the clipping of any
                                // containers applied
  Maybe<std::pair<wr::RenderRoot, wr::BlobImageKey>> mKey;
  std::vector<RefPtr<ScaledFont>> mFonts;

  DIGroup()
      : mAppUnitsPerDevPixel(0),
        mScrollId(ScrollableLayerGuid::NULL_SCROLL_ID),
        mHitInfo(CompositorHitTestInvisibleToHit) {}

  void InvalidateRect(const IntRect& aRect) {
    auto r = aRect.Intersect(mPreservedRect);
    // Empty rects get dropped
    if (!r.IsEmpty()) {
      mInvalidRect = mInvalidRect.Union(r);
    }
  }

  IntRect ItemBounds(nsDisplayItem* aItem) {
    BlobItemData* data = GetBlobItemData(aItem);
    return data->mRect;
  }

  void ClearItems() {
    GP("items: %d\n", mDisplayItems.Count());
    for (auto iter = mDisplayItems.Iter(); !iter.Done(); iter.Next()) {
      BlobItemData* data = iter.Get()->GetKey();
      GP("Deleting %p-%d\n", data->mFrame, data->mDisplayItemKey);
      iter.Remove();
      delete data;
    }
  }

  void ClearImageKey(RenderRootStateManager* aManager, bool aForce = false) {
    if (mKey) {
      MOZ_RELEASE_ASSERT(aForce || mInvalidRect.IsEmpty());
      aManager->AddBlobImageKeyForDiscard(mKey.value().second);
      mKey = Nothing();
    }
    mFonts.clear();
  }

  static IntRect ToDeviceSpace(nsRect aBounds, Matrix& aMatrix,
                               int32_t aAppUnitsPerDevPixel) {
    // RoundedOut can convert empty rectangles to non-empty ones
    // so special case them here
    if (aBounds.IsEmpty()) {
      return IntRect();
    }
    return RoundedOut(aMatrix.TransformBounds(
        ToRect(nsLayoutUtils::RectToGfxRect(aBounds, aAppUnitsPerDevPixel))));
  }

  void ComputeGeometryChange(nsDisplayItem* aItem, BlobItemData* aData,
                             Matrix& aMatrix, nsDisplayListBuilder* aBuilder) {
    // If the frame is marked as invalidated, and didn't specify a rect to
    // invalidate then we want to invalidate both the old and new bounds,
    // otherwise we only want to invalidate the changed areas. If we do get an
    // invalid rect, then we want to add this on top of the change areas.
    nsRect invalid;
    const DisplayItemClip& clip = aItem->GetClip();

    int32_t appUnitsPerDevPixel =
        aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
    MOZ_RELEASE_ASSERT(mAppUnitsPerDevPixel == appUnitsPerDevPixel);
    GP("\n");
    GP("clippedImageRect %d %d %d %d\n", mClippedImageBounds.x,
       mClippedImageBounds.y, mClippedImageBounds.width,
       mClippedImageBounds.height);
    LayerIntSize size = mVisibleRect.Size();
    GP("imageSize: %d %d\n", size.width, size.height);
    /*if (aItem->IsReused() && aData->mGeometry) {
      return;
    }*/

    GP("pre mInvalidRect: %s %p-%d - inv: %d %d %d %d\n", aItem->Name(),
       aItem->Frame(), aItem->GetPerFrameKey(), mInvalidRect.x, mInvalidRect.y,
       mInvalidRect.width, mInvalidRect.height);
    if (!aData->mGeometry) {
      // This item is being added for the first time, invalidate its entire
      // area.
      UniquePtr<nsDisplayItemGeometry> geometry(
          aItem->AllocateGeometry(aBuilder));
      nsRect clippedBounds = clip.ApplyNonRoundedIntersection(
          geometry->ComputeInvalidationRegion());
      aData->mGeometry = std::move(geometry);

      IntRect transformedRect =
          ToDeviceSpace(clippedBounds, aMatrix, appUnitsPerDevPixel);
      aData->mRect = transformedRect.Intersect(mClippedImageBounds);
      GP("CGC %s %d %d %d %d\n", aItem->Name(), clippedBounds.x,
         clippedBounds.y, clippedBounds.width, clippedBounds.height);
      GP("%d %d,  %f %f\n", mVisibleRect.TopLeft().x, mVisibleRect.TopLeft().y,
         aMatrix._11, aMatrix._22);
      GP("mRect %d %d %d %d\n", aData->mRect.x, aData->mRect.y,
         aData->mRect.width, aData->mRect.height);
      InvalidateRect(aData->mRect);
      aData->mInvalid = true;
    } else if (aData->mInvalid ||
               /* XXX: handle image load invalidation */ (
                   aItem->IsInvalid(invalid) && invalid.IsEmpty())) {
      UniquePtr<nsDisplayItemGeometry> geometry(
          aItem->AllocateGeometry(aBuilder));
      nsRect clippedBounds = clip.ApplyNonRoundedIntersection(
          geometry->ComputeInvalidationRegion());
      aData->mGeometry = std::move(geometry);

      GP("matrix: %f %f\n", aMatrix._31, aMatrix._32);
      GP("frame invalid invalidate: %s\n", aItem->Name());
      GP("old rect: %d %d %d %d\n", aData->mRect.x, aData->mRect.y,
         aData->mRect.width, aData->mRect.height);
      InvalidateRect(aData->mRect);
      // We want to snap to outside pixels. When should we multiply by the
      // matrix?
      // XXX: TransformBounds is expensive. We should avoid doing it if we have
      // no transform
      IntRect transformedRect =
          ToDeviceSpace(clippedBounds, aMatrix, appUnitsPerDevPixel);
      aData->mRect = transformedRect.Intersect(mClippedImageBounds);
      InvalidateRect(aData->mRect);
      GP("new rect: %d %d %d %d\n", aData->mRect.x, aData->mRect.y,
         aData->mRect.width, aData->mRect.height);
      aData->mInvalid = true;
    } else {
      GP("else invalidate: %s\n", aItem->Name());
      nsRegion combined;
      // this includes situations like reflow changing the position
      aItem->ComputeInvalidationRegion(aBuilder, aData->mGeometry.get(),
                                       &combined);
      if (!combined.IsEmpty()) {
        // There might be no point in doing this elaborate tracking here to get
        // smaller areas
        InvalidateRect(aData->mRect);  // invalidate the old area -- in theory
                                       // combined should take care of this
        UniquePtr<nsDisplayItemGeometry> geometry(
            aItem->AllocateGeometry(aBuilder));
        // invalidate the invalidated area.

        aData->mGeometry = std::move(geometry);

        nsRect clippedBounds = clip.ApplyNonRoundedIntersection(
            aData->mGeometry->ComputeInvalidationRegion());
        IntRect transformedRect =
            ToDeviceSpace(clippedBounds, aMatrix, appUnitsPerDevPixel);
        aData->mRect = transformedRect.Intersect(mClippedImageBounds);
        InvalidateRect(aData->mRect);

        aData->mInvalid = true;
      } else {
        if (aData->mClip != clip) {
          UniquePtr<nsDisplayItemGeometry> geometry(
              aItem->AllocateGeometry(aBuilder));
          if (!IsContainerLayerItem(aItem)) {
            // the bounds of layer items can change on us without
            // ComputeInvalidationRegion returning any change. Other items
            // shouldn't have any hidden geometry change.
            MOZ_RELEASE_ASSERT(
                geometry->mBounds.IsEqualEdges(aData->mGeometry->mBounds));
          } else {
            aData->mGeometry = std::move(geometry);
          }
          nsRect clippedBounds = clip.ApplyNonRoundedIntersection(
              aData->mGeometry->ComputeInvalidationRegion());
          IntRect transformedRect =
              ToDeviceSpace(clippedBounds, aMatrix, appUnitsPerDevPixel);
          InvalidateRect(aData->mRect);
          aData->mRect = transformedRect.Intersect(mClippedImageBounds);
          InvalidateRect(aData->mRect);

          GP("ClipChange: %s %d %d %d %d\n", aItem->Name(), aData->mRect.x,
             aData->mRect.y, aData->mRect.XMost(), aData->mRect.YMost());

        } else if (!aMatrix.ExactlyEquals(aData->mMatrix)) {
          // We haven't detected any changes so far. Unfortunately we don't
          // currently have a good way of checking if the transform has changed
          // so we just store it and see if it see if it has changed.
          // If we want this to go faster, we can probably put a flag on the
          // frame using the style sytem UpdateTransformLayer hint and check for
          // that.

          UniquePtr<nsDisplayItemGeometry> geometry(
              aItem->AllocateGeometry(aBuilder));
          if (!IsContainerLayerItem(aItem)) {
            // the bounds of layer items can change on us
            // other items shouldn't
            MOZ_RELEASE_ASSERT(
                geometry->mBounds.IsEqualEdges(aData->mGeometry->mBounds));
          } else {
            aData->mGeometry = std::move(geometry);
          }
          nsRect clippedBounds = clip.ApplyNonRoundedIntersection(
              aData->mGeometry->ComputeInvalidationRegion());
          IntRect transformedRect =
              ToDeviceSpace(clippedBounds, aMatrix, appUnitsPerDevPixel);
          InvalidateRect(aData->mRect);
          aData->mRect = transformedRect.Intersect(mClippedImageBounds);
          InvalidateRect(aData->mRect);

          GP("TransformChange: %s %d %d %d %d\n", aItem->Name(), aData->mRect.x,
             aData->mRect.y, aData->mRect.XMost(), aData->mRect.YMost());
        } else if (IsContainerLayerItem(aItem)) {
          UniquePtr<nsDisplayItemGeometry> geometry(
              aItem->AllocateGeometry(aBuilder));
          // we need to catch bounds changes of containers so that we continue
          // to have the correct bounds rects in the recording
          if (DetectContainerLayerPropertiesBoundsChange(aItem, aData,
                                                         *geometry)) {
            nsRect clippedBounds = clip.ApplyNonRoundedIntersection(
                geometry->ComputeInvalidationRegion());
            aData->mGeometry = std::move(geometry);
            IntRect transformedRect =
                ToDeviceSpace(clippedBounds, aMatrix, appUnitsPerDevPixel);
            InvalidateRect(aData->mRect);
            aData->mRect = transformedRect.Intersect(mClippedImageBounds);
            InvalidateRect(aData->mRect);
            GP("DetectContainerLayerPropertiesBoundsChange change\n");
          } else {
            // Handle changes in mClippedImageBounds
            nsRect clippedBounds = clip.ApplyNonRoundedIntersection(
                geometry->ComputeInvalidationRegion());
            IntRect transformedRect =
                ToDeviceSpace(clippedBounds, aMatrix, appUnitsPerDevPixel);
            auto rect = transformedRect.Intersect(mClippedImageBounds);
            if (!rect.IsEqualEdges(aData->mRect)) {
              GP("ContainerLayer image rect bounds change\n");
              InvalidateRect(aData->mRect);
              aData->mRect = rect;
              InvalidateRect(aData->mRect);
            } else {
              GP("Layer NoChange: %s %d %d %d %d\n", aItem->Name(),
                 aData->mRect.x, aData->mRect.y, aData->mRect.XMost(),
                 aData->mRect.YMost());
            }
          }
        } else {
          UniquePtr<nsDisplayItemGeometry> geometry(
              aItem->AllocateGeometry(aBuilder));
          nsRect clippedBounds = clip.ApplyNonRoundedIntersection(
              geometry->ComputeInvalidationRegion());
          IntRect transformedRect =
              ToDeviceSpace(clippedBounds, aMatrix, appUnitsPerDevPixel);
          auto rect = transformedRect.Intersect(mClippedImageBounds);
          // Make sure we update mRect for mClippedImageBounds changes
          if (!rect.IsEqualEdges(aData->mRect)) {
            GP("ContainerLayer image rect bounds change\n");
            InvalidateRect(aData->mRect);
            aData->mRect = rect;
            InvalidateRect(aData->mRect);
          } else {
            GP("NoChange: %s %d %d %d %d\n", aItem->Name(), aData->mRect.x,
               aData->mRect.y, aData->mRect.XMost(), aData->mRect.YMost());
          }
        }
      }
    }
    mActualBounds.OrWith(aData->mRect);
    aData->mClip = clip;
    aData->mMatrix = aMatrix;
    aData->mImageRect = mClippedImageBounds;
    GP("post mInvalidRect: %d %d %d %d\n", mInvalidRect.x, mInvalidRect.y,
       mInvalidRect.width, mInvalidRect.height);
  }

  void EndGroup(WebRenderLayerManager* aWrManager,
                nsDisplayListBuilder* aDisplayListBuilder,
                wr::DisplayListBuilder& aBuilder,
                wr::IpcResourceUpdateQueue& aResources, Grouper* aGrouper,
                nsDisplayItem* aStartItem, nsDisplayItem* aEndItem) {
    GP("\n\n");
    GP("Begin EndGroup\n");

    mVisibleRect = mVisibleRect.Intersect(ViewAs<LayerPixel>(
        mActualBounds, PixelCastJustification::LayerIsImage));

    if (mVisibleRect.IsEmpty()) {
      return;
    }

    // Invalidate any unused items
    GP("mDisplayItems\n");
    for (auto iter = mDisplayItems.Iter(); !iter.Done(); iter.Next()) {
      BlobItemData* data = iter.Get()->GetKey();
      GP("  : %p-%d\n", data->mFrame, data->mDisplayItemKey);
      if (!data->mUsed) {
        GP("Invalidate unused: %p-%d\n", data->mFrame, data->mDisplayItemKey);
        InvalidateRect(data->mRect);
        iter.Remove();
        delete data;
      } else {
        data->mUsed = false;
      }
    }

    IntSize dtSize = mVisibleRect.Size().ToUnknownSize();
    // The actual display item's size shouldn't have the scale factored in
    // Round the bounds out to leave space for unsnapped content
    LayoutDeviceToLayerScale2D scale(mScale.width, mScale.height);
    LayoutDeviceRect itemBounds =
        (LayerRect(mVisibleRect) - mResidualOffset) / scale;

    if (mInvalidRect.IsEmpty() && mVisibleRect.IsEqualEdges(mLastVisibleRect)) {
      GP("Not repainting group because it's empty\n");
      GP("End EndGroup\n");
      if (mKey) {
        // Although the contents haven't changed, the visible area *may* have,
        // so request it be updated unconditionally (wr should be able to easily
        // detect if this is a no-op on its side, if that matters)
        aResources.SetBlobImageVisibleArea(
            mKey.value().second,
            ViewAs<ImagePixel>(mVisibleRect,
                               PixelCastJustification::LayerIsImage));
        mLastVisibleRect = mVisibleRect;
        PushImage(aBuilder, itemBounds);
      }
      return;
    }

    gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
    std::vector<RefPtr<ScaledFont>> fonts;
    bool validFonts = true;
    RefPtr<WebRenderDrawEventRecorder> recorder =
        MakeAndAddRef<WebRenderDrawEventRecorder>(
            [&](MemStream& aStream,
                std::vector<RefPtr<ScaledFont>>& aScaledFonts) {
              size_t count = aScaledFonts.size();
              aStream.write((const char*)&count, sizeof(count));
              for (auto& scaled : aScaledFonts) {
                Maybe<wr::FontInstanceKey> key =
                    aWrManager->WrBridge()->GetFontKeyForScaledFont(
                        scaled, aBuilder.GetRenderRoot(), &aResources);
                if (key.isNothing()) {
                  validFonts = false;
                  break;
                }
                BlobFont font = {key.value(), scaled};
                aStream.write((const char*)&font, sizeof(font));
              }
              fonts = std::move(aScaledFonts);
            });

    RefPtr<gfx::DrawTarget> dummyDt = gfx::Factory::CreateDrawTarget(
        gfx::BackendType::SKIA, gfx::IntSize(1, 1), format);

    RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateRecordingDrawTarget(
        recorder, dummyDt, mLayerBounds.ToUnknownRect());
    // Setup the gfxContext
    RefPtr<gfxContext> context = gfxContext::CreateOrNull(dt);
    context->SetMatrix(
        Matrix::Scaling(mScale.width, mScale.height)
            .PostTranslate(mResidualOffset.x, mResidualOffset.y));

    GP("mInvalidRect: %d %d %d %d\n", mInvalidRect.x, mInvalidRect.y,
       mInvalidRect.width, mInvalidRect.height);

    RenderRootStateManager* rootManager =
        aWrManager->GetRenderRootStateManager();
    bool empty = aStartItem == aEndItem;
    if (empty) {
      ClearImageKey(rootManager, true);
      return;
    }

    // Reset mHitInfo, it will get updated inside PaintItemRange
    mHitInfo = CompositorHitTestInvisibleToHit;

    PaintItemRange(aGrouper, aStartItem, aEndItem, context, recorder,
                   rootManager, aResources);

    // XXX: set this correctly perhaps using
    // aItem->GetOpaqueRegion(aDisplayListBuilder, &snapped).
    //   Contains(paintBounds);?
    wr::OpacityType opacity = wr::OpacityType::HasAlphaChannel;

    bool hasItems = recorder->Finish();
    GP("%d Finish\n", hasItems);
    if (!validFonts) {
      gfxCriticalNote << "Failed serializing fonts for blob image";
      return;
    }
    Range<uint8_t> bytes((uint8_t*)recorder->mOutputStream.mData,
                         recorder->mOutputStream.mLength);
    if (!mKey) {
      // we don't want to send a new image that doesn't have any
      // items in it
      if (!hasItems || mVisibleRect.IsEmpty()) {
        return;
      }

      wr::BlobImageKey key =
          wr::BlobImageKey{aWrManager->WrBridge()->GetNextImageKey()};
      GP("No previous key making new one %d\n", key._0.mHandle);
      wr::ImageDescriptor descriptor(dtSize, 0, dt->GetFormat(), opacity);
      MOZ_RELEASE_ASSERT(bytes.length() > sizeof(size_t));
      if (!aResources.AddBlobImage(
              key, descriptor, bytes,
              ViewAs<ImagePixel>(mVisibleRect,
                                 PixelCastJustification::LayerIsImage))) {
        return;
      }
      mKey = Some(std::make_pair(aBuilder.GetRenderRoot(), key));
    } else {
      wr::ImageDescriptor descriptor(dtSize, 0, dt->GetFormat(), opacity);

      // Convert mInvalidRect to image space by subtracting the corner of the
      // image bounds
      auto dirtyRect = ViewAs<ImagePixel>(mInvalidRect);

      auto bottomRight = dirtyRect.BottomRight();
      GP("check invalid %d %d - %d %d\n", bottomRight.x, bottomRight.y,
         dtSize.width, dtSize.height);
      GP("Update Blob %d %d %d %d\n", mInvalidRect.x, mInvalidRect.y,
         mInvalidRect.width, mInvalidRect.height);
      if (!aResources.UpdateBlobImage(
              mKey.value().second, descriptor, bytes,
              ViewAs<ImagePixel>(mVisibleRect,
                                 PixelCastJustification::LayerIsImage),
              dirtyRect)) {
        return;
      }
    }
    mFonts = std::move(fonts);
    aResources.SetBlobImageVisibleArea(
        mKey.value().second,
        ViewAs<ImagePixel>(mVisibleRect, PixelCastJustification::LayerIsImage));
    mLastVisibleRect = mVisibleRect;
    PushImage(aBuilder, itemBounds);
    GP("End EndGroup\n\n");
  }

  void PushImage(wr::DisplayListBuilder& aBuilder,
                 const LayoutDeviceRect& bounds) {
    wr::LayoutRect dest = wr::ToLayoutRect(bounds);
    GP("PushImage: %f %f %f %f\n", dest.origin.x, dest.origin.y,
       dest.size.width, dest.size.height);
    gfx::SamplingFilter sampleFilter = gfx::SamplingFilter::
        LINEAR;  // nsLayoutUtils::GetSamplingFilterForFrame(aItem->Frame());
    bool backfaceHidden = false;

    // We don't really know the exact shape of this blob because it may contain
    // SVG shapes. Also mHitInfo may be a combination of hit info flags from
    // different shapes so generate an irregular-area hit-test region for it.
    CompositorHitTestInfo hitInfo = mHitInfo;
    if (hitInfo.contains(CompositorHitTestFlags::eVisibleToHitTest)) {
      hitInfo += CompositorHitTestFlags::eIrregularArea;
    }

    // XXX - clipping the item against the paint rect breaks some content.
    // cf. Bug 1455422.
    // wr::LayoutRect clip = wr::ToLayoutRect(bounds.Intersect(mVisibleRect));

    aBuilder.SetHitTestInfo(mScrollId, hitInfo, SideBits::eNone);
    aBuilder.PushImage(dest, dest, !backfaceHidden,
                       wr::ToImageRendering(sampleFilter),
                       wr::AsImageKey(mKey.value().second));
    aBuilder.ClearHitTestInfo();
  }

  void PaintItemRange(Grouper* aGrouper, nsDisplayItem* aStartItem,
                      nsDisplayItem* aEndItem, gfxContext* aContext,
                      WebRenderDrawEventRecorder* aRecorder,
                      RenderRootStateManager* aRootManager,
                      wr::IpcResourceUpdateQueue& aResources) {
    LayerIntSize size = mVisibleRect.Size();
    for (nsDisplayItem* item = aStartItem; item != aEndItem;
         item = item->GetAbove()) {
      BlobItemData* data = GetBlobItemData(item);
      IntRect bounds = data->mRect;
      auto bottomRight = bounds.BottomRight();

      GP("Trying %s %p-%d %d %d %d %d\n", item->Name(), item->Frame(),
         item->GetPerFrameKey(), bounds.x, bounds.y, bounds.XMost(),
         bounds.YMost());

      if (item->GetType() == DisplayItemType::TYPE_COMPOSITOR_HITTEST_INFO) {
        // Accumulate the hit-test info flags. In cases where there are multiple
        // hittest-info display items with different flags, mHitInfo will have
        // the union of all those flags. If that is the case, we will
        // additionally set eIrregularArea (at the site that we use mHitInfo)
        // so that downstream consumers of this (primarily APZ) will know that
        // the exact shape of what gets hit with what is unknown.
        mHitInfo +=
            static_cast<nsDisplayCompositorHitTestInfo*>(item)->HitTestFlags();
        continue;
      }

      GP("paint check invalid %d %d - %d %d\n", bottomRight.x, bottomRight.y,
         size.width, size.height);
      // skip empty items
      if (bounds.IsEmpty()) {
        continue;
      }

      bool dirty = true;
      auto preservedBounds = bounds.Intersect(mPreservedRect);
      if (!mInvalidRect.Contains(preservedBounds)) {
        GP("Passing\n");
        dirty = false;
        BlobItemData* data = GetBlobItemData(item);
        if (data->mInvalid) {
          gfxCriticalError()
              << "DisplayItem" << item->Name() << "-should be invalid";
        }
        // if the item is invalid it needs to be fully contained
        MOZ_RELEASE_ASSERT(!data->mInvalid);
      }

      nsDisplayList* children = item->GetChildren();
      if (children) {
        GP("doing children in EndGroup\n");
        aGrouper->PaintContainerItem(this, item, data, bounds, children,
                                     aContext, aRecorder, aRootManager,
                                     aResources);
        continue;
      }
      nsPaintedDisplayItem* paintedItem = item->AsPaintedDisplayItem();
      if (!paintedItem) {
        continue;
      }
      if (dirty) {
        // What should the clip settting strategy be? We can set the full
        // clip everytime. this is probably easiest for now. An alternative
        // would be to put the push and the pop into separate items and let
        // invalidation handle it that way.
        DisplayItemClip currentClip = paintedItem->GetClip();

        if (currentClip.HasClip()) {
          aContext->Save();
          currentClip.ApplyTo(aContext, aGrouper->mAppUnitsPerDevPixel);
        }
        aContext->NewPath();
        GP("painting %s %p-%d\n", paintedItem->Name(), paintedItem->Frame(),
           paintedItem->GetPerFrameKey());
        if (aGrouper->mDisplayListBuilder->IsPaintingToWindow()) {
          paintedItem->Frame()->AddStateBits(NS_FRAME_PAINTED_THEBES);
        }

        paintedItem->Paint(aGrouper->mDisplayListBuilder, aContext);
        TakeExternalSurfaces(aRecorder, data->mExternalSurfaces, aRootManager,
                             aResources);

        if (currentClip.HasClip()) {
          aContext->Restore();
        }
      }
      aContext->GetDrawTarget()->FlushItem(bounds);
    }
  }

  ~DIGroup() {
    GP("Group destruct\n");
    for (auto iter = mDisplayItems.Iter(); !iter.Done(); iter.Next()) {
      BlobItemData* data = iter.Get()->GetKey();
      GP("Deleting %p-%d\n", data->mFrame, data->mDisplayItemKey);
      iter.Remove();
      delete data;
    }
  }
};

// If we have an item we need to make sure it matches the current group
// otherwise it means the item switched groups and we need to invalidate
// it and recreate the data.
static BlobItemData* GetBlobItemDataForGroup(nsDisplayItem* aItem,
                                             DIGroup* aGroup) {
  BlobItemData* data = GetBlobItemData(aItem);
  if (data) {
    MOZ_RELEASE_ASSERT(data->mGroup->mDisplayItems.Contains(data));
    if (data->mGroup != aGroup) {
      GP("group don't match %p %p\n", data->mGroup, aGroup);
      data->ClearFrame();
      // the item is for another group
      // it should be cleared out as being unused at the end of this paint
      data = nullptr;
    }
  }
  if (!data) {
    GP("Allocating blob data\n");
    data = new BlobItemData(aGroup, aItem);
    aGroup->mDisplayItems.PutEntry(data);
  }
  data->mUsed = true;
  return data;
}

void Grouper::PaintContainerItem(DIGroup* aGroup, nsDisplayItem* aItem,
                                 BlobItemData* aData,
                                 const IntRect& aItemBounds,
                                 nsDisplayList* aChildren, gfxContext* aContext,
                                 WebRenderDrawEventRecorder* aRecorder,
                                 RenderRootStateManager* aRootManager,
                                 wr::IpcResourceUpdateQueue& aResources) {
  switch (aItem->GetType()) {
    case DisplayItemType::TYPE_TRANSFORM: {
      DisplayItemClip currentClip = aItem->GetClip();

      gfxContextMatrixAutoSaveRestore saveMatrix;
      if (currentClip.HasClip()) {
        aContext->Save();
        currentClip.ApplyTo(aContext, this->mAppUnitsPerDevPixel);
        aContext->GetDrawTarget()->FlushItem(aItemBounds);
      } else {
        saveMatrix.SetContext(aContext);
      }

      auto transformItem = static_cast<nsDisplayTransform*>(aItem);
      Matrix4x4Flagged trans = transformItem->GetTransform();
      Matrix trans2d;
      if (!trans.Is2D(&trans2d)) {
        // We don't currently support doing invalidation inside 3d transforms.
        // For now just paint it as a single item.
        BlobItemData* data = GetBlobItemDataForGroup(aItem, aGroup);
        if (data->mLayerManager->GetRoot()) {
          data->mLayerManager->BeginTransaction();
          data->mLayerManager->EndTransaction(
              FrameLayerBuilder::DrawPaintedLayer, mDisplayListBuilder);
          TakeExternalSurfaces(aRecorder, data->mExternalSurfaces, aRootManager,
                               aResources);
          aContext->GetDrawTarget()->FlushItem(aItemBounds);
        }
      } else {
        aContext->Multiply(ThebesMatrix(trans2d));
        aGroup->PaintItemRange(this, aChildren->GetBottom(), nullptr, aContext,
                               aRecorder, aRootManager, aResources);
      }

      if (currentClip.HasClip()) {
        aContext->Restore();
        aContext->GetDrawTarget()->FlushItem(aItemBounds);
      }
      break;
    }
    case DisplayItemType::TYPE_OPACITY: {
      auto opacityItem = static_cast<nsDisplayOpacity*>(aItem);
      float opacity = opacityItem->GetOpacity();
      if (opacity == 0.0f) {
        return;
      }

      aContext->GetDrawTarget()->PushLayer(false, opacityItem->GetOpacity(),
                                           nullptr, mozilla::gfx::Matrix(),
                                           aItemBounds);
      GP("beginGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
         aItem->GetPerFrameKey());
      aContext->GetDrawTarget()->FlushItem(aItemBounds);
      aGroup->PaintItemRange(this, aChildren->GetBottom(), nullptr, aContext,
                             aRecorder, aRootManager, aResources);
      aContext->GetDrawTarget()->PopLayer();
      GP("endGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
         aItem->GetPerFrameKey());
      aContext->GetDrawTarget()->FlushItem(aItemBounds);
      break;
    }
    case DisplayItemType::TYPE_BLEND_MODE: {
      auto blendItem = static_cast<nsDisplayBlendMode*>(aItem);
      auto blendMode = blendItem->BlendMode();
      aContext->GetDrawTarget()->PushLayerWithBlend(
          false, 1.0, nullptr, mozilla::gfx::Matrix(), aItemBounds, false,
          blendMode);
      GP("beginGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
         aItem->GetPerFrameKey());
      aContext->GetDrawTarget()->FlushItem(aItemBounds);
      aGroup->PaintItemRange(this, aChildren->GetBottom(), nullptr, aContext,
                             aRecorder, aRootManager, aResources);
      aContext->GetDrawTarget()->PopLayer();
      GP("endGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
         aItem->GetPerFrameKey());
      aContext->GetDrawTarget()->FlushItem(aItemBounds);
      break;
    }
    case DisplayItemType::TYPE_BLEND_CONTAINER: {
      aContext->GetDrawTarget()->PushLayer(false, 1.0, nullptr,
                                           mozilla::gfx::Matrix(), aItemBounds);
      GP("beginGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
         aItem->GetPerFrameKey());
      aContext->GetDrawTarget()->FlushItem(aItemBounds);
      aGroup->PaintItemRange(this, aChildren->GetBottom(), nullptr, aContext,
                             aRecorder, aRootManager, aResources);
      aContext->GetDrawTarget()->PopLayer();
      GP("endGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
         aItem->GetPerFrameKey());
      aContext->GetDrawTarget()->FlushItem(aItemBounds);
      break;
    }
    case DisplayItemType::TYPE_MASK: {
      GP("Paint Mask\n");
      auto maskItem = static_cast<nsDisplayMasksAndClipPaths*>(aItem);
      maskItem->SetPaintRect(maskItem->GetClippedBounds(mDisplayListBuilder));
      if (maskItem->IsValidMask()) {
        maskItem->PaintWithContentsPaintCallback(
            mDisplayListBuilder, aContext, [&] {
              GP("beginGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
                 aItem->GetPerFrameKey());
              aContext->GetDrawTarget()->FlushItem(aItemBounds);
              aGroup->PaintItemRange(this, aChildren->GetBottom(), nullptr,
                                     aContext, aRecorder, aRootManager,
                                     aResources);
              GP("endGroup %s %p-%d\n", aItem->Name(), aItem->Frame(),
                 aItem->GetPerFrameKey());
            });
        TakeExternalSurfaces(aRecorder, aData->mExternalSurfaces, aRootManager,
                             aResources);
        aContext->GetDrawTarget()->FlushItem(aItemBounds);
      }
      break;
    }
    case DisplayItemType::TYPE_FILTER: {
      GP("Paint Filter\n");
      // We don't currently support doing invalidation inside nsDisplayFilters
      // for now just paint it as a single item
      BlobItemData* data = GetBlobItemDataForGroup(aItem, aGroup);
      if (data->mLayerManager->GetRoot()) {
        data->mLayerManager->BeginTransaction();
        static_cast<nsDisplayFilters*>(aItem)->PaintAsLayer(
            mDisplayListBuilder, aContext, data->mLayerManager);
        if (data->mLayerManager->InTransaction()) {
          data->mLayerManager->AbortTransaction();
        }
        TakeExternalSurfaces(aRecorder, data->mExternalSurfaces, aRootManager,
                             aResources);
        aContext->GetDrawTarget()->FlushItem(aItemBounds);
      }
      break;
    }

    default:
      aGroup->PaintItemRange(this, aChildren->GetBottom(), nullptr, aContext,
                             aRecorder, aRootManager, aResources);
      break;
  }
}

class WebRenderGroupData : public WebRenderUserData {
 public:
  WebRenderGroupData(RenderRootStateManager* aWRManager, nsDisplayItem* aItem);
  virtual ~WebRenderGroupData();

  WebRenderGroupData* AsGroupData() override { return this; }
  UserDataType GetType() override { return UserDataType::eGroup; }
  static UserDataType Type() { return UserDataType::eGroup; }

  DIGroup mSubGroup;
  DIGroup mFollowingGroup;
};

static bool IsItemProbablyActive(nsDisplayItem* aItem,
                                 nsDisplayListBuilder* aDisplayListBuilder,
                                 bool aParentActive = true);

static bool HasActiveChildren(const nsDisplayList& aList,
                              nsDisplayListBuilder* aDisplayListBuilder) {
  for (nsDisplayItem* i = aList.GetBottom(); i; i = i->GetAbove()) {
    if (IsItemProbablyActive(i, aDisplayListBuilder, false)) {
      return true;
    }
  }
  return false;
}

// This function decides whether we want to treat this item as "active", which
// means that it's a container item which we will turn into a WebRender
// StackingContext, or whether we treat it as "inactive" and include it inside
// the parent blob image.
//
// We can't easily use GetLayerState because it wants a bunch of layers related
// information.
static bool IsItemProbablyActive(nsDisplayItem* aItem,
                                 nsDisplayListBuilder* aDisplayListBuilder,
                                 bool aParentActive) {
  switch (aItem->GetType()) {
    case DisplayItemType::TYPE_TRANSFORM: {
      nsDisplayTransform* transformItem =
          static_cast<nsDisplayTransform*>(aItem);
      const Matrix4x4Flagged& t = transformItem->GetTransform();
      Matrix t2d;
      bool is2D = t.Is2D(&t2d);
      GP("active: %d\n", transformItem->MayBeAnimated(aDisplayListBuilder));
      return transformItem->MayBeAnimated(aDisplayListBuilder, false) ||
             !is2D ||
             HasActiveChildren(*transformItem->GetChildren(),
                               aDisplayListBuilder);
    }
    case DisplayItemType::TYPE_OPACITY: {
      nsDisplayOpacity* opacityItem = static_cast<nsDisplayOpacity*>(aItem);
      bool active = opacityItem->NeedsActiveLayer(aDisplayListBuilder,
                                                  opacityItem->Frame(), false);
      GP("active: %d\n", active);
      return active || HasActiveChildren(*opacityItem->GetChildren(),
                                         aDisplayListBuilder);
    }
    case DisplayItemType::TYPE_FOREIGN_OBJECT: {
      return true;
    }
    case DisplayItemType::TYPE_BLEND_MODE: {
      /* BLEND_MODE needs to be active if it might have a previous sibling
       * that is active. We use the activeness of the parent as a rough
       * proxy for this situation. */
      return aParentActive ||
             HasActiveChildren(*aItem->GetChildren(), aDisplayListBuilder);
    }
    case DisplayItemType::TYPE_WRAP_LIST:
    case DisplayItemType::TYPE_CONTAINER:
    case DisplayItemType::TYPE_MASK:
    case DisplayItemType::TYPE_PERSPECTIVE: {
      if (aItem->GetChildren()) {
        return HasActiveChildren(*aItem->GetChildren(), aDisplayListBuilder);
      }
      return false;
    }
    case DisplayItemType::TYPE_FILTER: {
      nsDisplayFilters* filters = static_cast<nsDisplayFilters*>(aItem);
      return filters->CanCreateWebRenderCommands();
    }
    default:
      // TODO: handle other items?
      return false;
  }
}

// This does a pass over the display lists and will join the display items
// into groups as well as paint them
void Grouper::ConstructGroups(nsDisplayListBuilder* aDisplayListBuilder,
                              WebRenderCommandBuilder* aCommandBuilder,
                              wr::DisplayListBuilder& aBuilder,
                              wr::IpcResourceUpdateQueue& aResources,
                              DIGroup* aGroup, nsDisplayList* aList,
                              const StackingContextHelper& aSc) {
  DIGroup* currentGroup = aGroup;

  nsDisplayItem* item = aList->GetBottom();
  nsDisplayItem* startOfCurrentGroup = item;
  while (item) {
    if (IsItemProbablyActive(item, mDisplayListBuilder)) {
      // We're going to be starting a new group.
      RefPtr<WebRenderGroupData> groupData =
          aCommandBuilder->CreateOrRecycleWebRenderUserData<WebRenderGroupData>(
              item, aBuilder.GetRenderRoot());

      groupData->mFollowingGroup.mInvalidRect.SetEmpty();

      // Initialize groupData->mFollowingGroup with data from currentGroup.
      // We want to copy out this information before calling EndGroup because
      // EndGroup will set mLastVisibleRect depending on whether
      // we send something to WebRender.

      // TODO: compute the group bounds post-grouping, so that they can be
      // tighter for just the sublist that made it into this group.
      // We want to ensure the tight bounds are still clipped by area
      // that we're building the display list for.
      if (groupData->mFollowingGroup.mScale != currentGroup->mScale ||
          groupData->mFollowingGroup.mAppUnitsPerDevPixel !=
              currentGroup->mAppUnitsPerDevPixel ||
          groupData->mFollowingGroup.mResidualOffset !=
              currentGroup->mResidualOffset) {
        if (groupData->mFollowingGroup.mAppUnitsPerDevPixel !=
            currentGroup->mAppUnitsPerDevPixel) {
          GP("app unit change following: %d %d\n",
             groupData->mFollowingGroup.mAppUnitsPerDevPixel,
             currentGroup->mAppUnitsPerDevPixel);
        }
        // The group changed size
        GP("Inner group size change\n");
        groupData->mFollowingGroup.ClearItems();
        groupData->mFollowingGroup.ClearImageKey(
            aCommandBuilder->mManager->GetRenderRootStateManager());
      }
      groupData->mFollowingGroup.mGroupBounds = currentGroup->mGroupBounds;
      groupData->mFollowingGroup.mAppUnitsPerDevPixel =
          currentGroup->mAppUnitsPerDevPixel;
      groupData->mFollowingGroup.mLayerBounds = currentGroup->mLayerBounds;
      groupData->mFollowingGroup.mClippedImageBounds =
          currentGroup->mClippedImageBounds;
      groupData->mFollowingGroup.mScale = currentGroup->mScale;
      groupData->mFollowingGroup.mResidualOffset =
          currentGroup->mResidualOffset;
      groupData->mFollowingGroup.mVisibleRect = currentGroup->mVisibleRect;
      groupData->mFollowingGroup.mPreservedRect =
          groupData->mFollowingGroup.mVisibleRect
              .Intersect(groupData->mFollowingGroup.mLastVisibleRect)
              .ToUnknownRect();
      groupData->mFollowingGroup.mActualBounds = IntRect();

      currentGroup->EndGroup(aCommandBuilder->mManager, aDisplayListBuilder,
                             aBuilder, aResources, this, startOfCurrentGroup,
                             item);

      {
        auto spaceAndClipChain = mClipManager.SwitchItem(item);
        wr::SpaceAndClipChainHelper saccHelper(aBuilder, spaceAndClipChain);

        sIndent++;
        // Note: this call to CreateWebRenderCommands can recurse back into
        // this function.
        RenderRootStateManager* manager =
            aCommandBuilder->mManager->GetRenderRootStateManager();
        bool createdWRCommands = item->CreateWebRenderCommands(
            aBuilder, aResources, aSc, manager, mDisplayListBuilder);
        sIndent--;
        MOZ_RELEASE_ASSERT(
            createdWRCommands,
            "active transforms should always succeed at creating "
            "WebRender commands");
      }

      currentGroup = &groupData->mFollowingGroup;

      startOfCurrentGroup = item->GetAbove();
    } else {  // inactive item
      ConstructItemInsideInactive(aCommandBuilder, aBuilder, aResources,
                                  currentGroup, item, aSc);
    }

    item = item->GetAbove();
  }

  currentGroup->EndGroup(aCommandBuilder->mManager, aDisplayListBuilder,
                         aBuilder, aResources, this, startOfCurrentGroup,
                         nullptr);
}

// This does a pass over the display lists and will join the display items
// into a single group.
void Grouper::ConstructGroupInsideInactive(
    WebRenderCommandBuilder* aCommandBuilder, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources, DIGroup* aGroup,
    nsDisplayList* aList, const StackingContextHelper& aSc) {
  nsDisplayItem* item = aList->GetBottom();
  while (item) {
    ConstructItemInsideInactive(aCommandBuilder, aBuilder, aResources, aGroup,
                                item, aSc);
    item = item->GetAbove();
  }
}

bool BuildLayer(nsDisplayItem* aItem, BlobItemData* aData,
                nsDisplayListBuilder* aDisplayListBuilder,
                const gfx::Size& aScale);

void Grouper::ConstructItemInsideInactive(
    WebRenderCommandBuilder* aCommandBuilder, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources, DIGroup* aGroup,
    nsDisplayItem* aItem, const StackingContextHelper& aSc) {
  nsDisplayList* children = aItem->GetChildren();
  BlobItemData* data = GetBlobItemDataForGroup(aItem, aGroup);

  /* mInvalid unfortunately persists across paints. Clear it so that if we don't
   * set it to 'true' we ensure that we're not using the value from the last
   * time that we painted */
  data->mInvalid = false;

  // we compute the geometry change here because we have the transform around
  // still
  aGroup->ComputeGeometryChange(aItem, data, mTransform, mDisplayListBuilder);

  // Temporarily restrict the image bounds to the bounds of the container so
  // that clipped children within the container know about the clip. This
  // ensures that the bounds passed to FlushItem are contained in the bounds of
  // the clip so that we don't include items in the recording without including
  // their corresponding clipping items.
  IntRect oldClippedImageBounds = aGroup->mClippedImageBounds;
  aGroup->mClippedImageBounds =
      aGroup->mClippedImageBounds.Intersect(data->mRect);

  if (aItem->GetType() == DisplayItemType::TYPE_FILTER) {
    gfx::Size scale(1, 1);
    // If ComputeDifferences finds any change, we invalidate the entire
    // container item. This is needed because blob merging requires the entire
    // item to be within the invalid region.
    if (BuildLayer(aItem, data, mDisplayListBuilder, scale)) {
      data->mInvalid = true;
      aGroup->InvalidateRect(data->mRect);
    }
  } else if (aItem->GetType() == DisplayItemType::TYPE_TRANSFORM) {
    nsDisplayTransform* transformItem = static_cast<nsDisplayTransform*>(aItem);
    const Matrix4x4Flagged& t = transformItem->GetTransform();
    Matrix t2d;
    bool is2D = t.Is2D(&t2d);
    if (!is2D) {
      // We'll use BasicLayerManager to handle 3d transforms.
      gfx::Size scale(1, 1);
      // If ComputeDifferences finds any change, we invalidate the entire
      // container item. This is needed because blob merging requires the entire
      // item to be within the invalid region.
      if (BuildLayer(aItem, data, mDisplayListBuilder, scale)) {
        data->mInvalid = true;
        aGroup->InvalidateRect(data->mRect);
      }
    } else {
      Matrix m = mTransform;

      GP("t2d: %f %f\n", t2d._31, t2d._32);
      mTransform.PreMultiply(t2d);
      GP("mTransform: %f %f\n", mTransform._31, mTransform._32);
      ConstructGroupInsideInactive(aCommandBuilder, aBuilder, aResources,
                                   aGroup, children, aSc);

      mTransform = m;
    }
  } else if (children) {
    sIndent++;
    ConstructGroupInsideInactive(aCommandBuilder, aBuilder, aResources, aGroup,
                                 children, aSc);
    sIndent--;
  }

  GP("Including %s of %d\n", aItem->Name(), aGroup->mDisplayItems.Count());
  aGroup->mClippedImageBounds = oldClippedImageBounds;
}

/* This is just a copy of nsRect::ScaleToOutsidePixels with an offset added in.
 * The offset is applied just before the rounding. It's in the scaled space. */
static mozilla::gfx::IntRect ScaleToOutsidePixelsOffset(
    nsRect aRect, float aXScale, float aYScale, nscoord aAppUnitsPerPixel,
    LayerPoint aOffset) {
  mozilla::gfx::IntRect rect;
  rect.SetNonEmptyBox(
      NSToIntFloor(NSAppUnitsToFloatPixels(aRect.x, float(aAppUnitsPerPixel)) *
                       aXScale +
                   aOffset.x),
      NSToIntFloor(NSAppUnitsToFloatPixels(aRect.y, float(aAppUnitsPerPixel)) *
                       aYScale +
                   aOffset.y),
      NSToIntCeil(
          NSAppUnitsToFloatPixels(aRect.XMost(), float(aAppUnitsPerPixel)) *
              aXScale +
          aOffset.x),
      NSToIntCeil(
          NSAppUnitsToFloatPixels(aRect.YMost(), float(aAppUnitsPerPixel)) *
              aYScale +
          aOffset.y));
  return rect;
}

/* This function is the same as the above except that it rounds to the
 * nearest instead of rounding out. We use it for attempting to compute the
 * actual pixel bounds of opaque items */
static mozilla::gfx::IntRect ScaleToNearestPixelsOffset(
    nsRect aRect, float aXScale, float aYScale, nscoord aAppUnitsPerPixel,
    LayerPoint aOffset) {
  mozilla::gfx::IntRect rect;
  rect.SetNonEmptyBox(
      NSToIntFloor(NSAppUnitsToFloatPixels(aRect.x, float(aAppUnitsPerPixel)) *
                       aXScale +
                   aOffset.x + 0.5),
      NSToIntFloor(NSAppUnitsToFloatPixels(aRect.y, float(aAppUnitsPerPixel)) *
                       aYScale +
                   aOffset.y + 0.5),
      NSToIntFloor(
          NSAppUnitsToFloatPixels(aRect.XMost(), float(aAppUnitsPerPixel)) *
              aXScale +
          aOffset.x + 0.5),
      NSToIntFloor(
          NSAppUnitsToFloatPixels(aRect.YMost(), float(aAppUnitsPerPixel)) *
              aYScale +
          aOffset.y + 0.5));
  return rect;
}

RenderRootStateManager* WebRenderCommandBuilder::GetRenderRootStateManager() {
  return mManager->GetRenderRootStateManager();
}

void WebRenderCommandBuilder::DoGroupingForDisplayList(
    nsDisplayList* aList, nsDisplayItem* aWrappingItem,
    nsDisplayListBuilder* aDisplayListBuilder, const StackingContextHelper& aSc,
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources) {
  if (!aList->GetBottom()) {
    return;
  }

  GP("DoGroupingForDisplayList\n");

  mClipManager.BeginList(aSc);
  Grouper g(mClipManager);

  int32_t appUnitsPerDevPixel =
      aWrappingItem->Frame()->PresContext()->AppUnitsPerDevPixel();

  g.mDisplayListBuilder = aDisplayListBuilder;
  RefPtr<WebRenderGroupData> groupData =
      CreateOrRecycleWebRenderUserData<WebRenderGroupData>(
          aWrappingItem, aBuilder.GetRenderRoot());

  bool snapped;
  nsRect groupBounds =
      aWrappingItem->GetUntransformedBounds(aDisplayListBuilder, &snapped);
  DIGroup& group = groupData->mSubGroup;

  gfx::Size scale = aSc.GetInheritedScale();
  GP("Inherrited scale %f %f\n", scale.width, scale.height);

  auto trans =
      ViewAs<LayerPixel>(aSc.GetSnappingSurfaceTransform().GetTranslation());
  auto snappedTrans = LayerIntPoint::Floor(trans);
  LayerPoint residualOffset = trans - snappedTrans;

  auto p = group.mGroupBounds;
  auto q = groupBounds;
  // XXX: we currently compute the paintRect for the entire svg, but if the svg
  // gets split into multiple groups (blobs), then they will all inherit this
  // overall size even though they may each be much smaller. This can lead to
  // allocating much larger textures than necessary in webrender.
  //
  // Don't bother fixing this unless we run into this in the real world, though.
  auto layerBounds = LayerIntRect::FromUnknownRect(
      ScaleToOutsidePixelsOffset(groupBounds, scale.width, scale.height,
                                 appUnitsPerDevPixel, residualOffset));

  const nsRect& untransformedPaintRect =
      aWrappingItem->GetUntransformedPaintRect();

  auto visibleRect = LayerIntRect::FromUnknownRect(
                         ScaleToOutsidePixelsOffset(
                             untransformedPaintRect, scale.width, scale.height,
                             appUnitsPerDevPixel, residualOffset))
                         .Intersect(layerBounds);

  GP("LayerBounds: %d %d %d %d\n", layerBounds.x, layerBounds.y,
     layerBounds.width, layerBounds.height);
  GP("VisibleRect: %d %d %d %d\n", visibleRect.x, visibleRect.y,
     visibleRect.width, visibleRect.height);

  GP("Inherrited scale %f %f\n", scale.width, scale.height);
  GP("Bounds: %d %d %d %d vs %d %d %d %d\n", p.x, p.y, p.width, p.height, q.x,
     q.y, q.width, q.height);

  group.mInvalidRect.SetEmpty();
  if (group.mAppUnitsPerDevPixel != appUnitsPerDevPixel ||
      group.mScale != scale || group.mResidualOffset != residualOffset) {
    GP("Property change. Deleting blob\n");

    if (group.mAppUnitsPerDevPixel != appUnitsPerDevPixel) {
      GP(" App unit change %d -> %d\n", group.mAppUnitsPerDevPixel,
         appUnitsPerDevPixel);
    }
    // The bounds have changed so we need to discard the old image and add all
    // the commands again.
    auto p = group.mGroupBounds;
    auto q = groupBounds;
    if (!group.mGroupBounds.IsEqualEdges(groupBounds)) {
      GP(" Bounds change: %d %d %d %d -> %d %d %d %d\n", p.x, p.y, p.width,
         p.height, q.x, q.y, q.width, q.height);
    }

    if (group.mScale != scale) {
      GP(" Scale %f %f -> %f %f\n", group.mScale.width, group.mScale.height,
         scale.width, scale.height);
    }

    if (group.mResidualOffset != residualOffset) {
      GP(" Residual Offset %f %f -> %f %f\n", group.mResidualOffset.x,
         group.mResidualOffset.y, residualOffset.x, residualOffset.y);
    }

    group.ClearItems();
    group.ClearImageKey(mManager->GetRenderRootStateManager());
  }

  ScrollableLayerGuid::ViewID scrollId = ScrollableLayerGuid::NULL_SCROLL_ID;
  if (const ActiveScrolledRoot* asr = aWrappingItem->GetActiveScrolledRoot()) {
    scrollId = asr->GetViewId();
  }

  g.mAppUnitsPerDevPixel = appUnitsPerDevPixel;
  group.mResidualOffset = residualOffset;
  group.mGroupBounds = groupBounds;
  group.mLayerBounds = layerBounds;
  group.mVisibleRect = visibleRect;
  group.mActualBounds = IntRect();
  group.mPreservedRect =
      group.mVisibleRect.Intersect(group.mLastVisibleRect).ToUnknownRect();
  group.mAppUnitsPerDevPixel = appUnitsPerDevPixel;
  group.mClippedImageBounds = layerBounds.ToUnknownRect();

  g.mTransform = Matrix::Scaling(scale.width, scale.height)
                     .PostTranslate(residualOffset.x, residualOffset.y);
  group.mScale = scale;
  group.mScrollId = scrollId;
  g.ConstructGroups(aDisplayListBuilder, this, aBuilder, aResources, &group,
                    aList, aSc);
  mClipManager.EndList(aSc);
}

WebRenderCommandBuilder::WebRenderCommandBuilder(
    WebRenderLayerManager* aManager)
    : mManager(aManager),
      mLastAsr(nullptr),
      mBuilderDumpIndex(0),
      mDumpIndent(0),
      mDoGrouping(false),
      mContainsSVGGroup(false) {}

void WebRenderCommandBuilder::Destroy() {
  mLastCanvasDatas.Clear();
  mLastLocalCanvasDatas.Clear();
  ClearCachedResources();
}

void WebRenderCommandBuilder::EmptyTransaction() {
  // We need to update canvases that might have changed.
  for (auto iter = mLastCanvasDatas.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<WebRenderCanvasData> canvasData = iter.Get()->GetKey();
    WebRenderCanvasRendererAsync* canvas = canvasData->GetCanvasRenderer();
    if (canvas) {
      canvas->UpdateCompositableClientForEmptyTransaction();
    }
  }
  for (auto iter = mLastLocalCanvasDatas.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<WebRenderLocalCanvasData> canvasData = iter.Get()->GetKey();
    canvasData->RefreshExternalImage();
    canvasData->RequestFrameReadback();
  }
}

bool WebRenderCommandBuilder::NeedsEmptyTransaction() {
  return !mLastCanvasDatas.IsEmpty() || !mLastLocalCanvasDatas.IsEmpty();
}

void WebRenderCommandBuilder::BuildWebRenderCommands(
    wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResourceUpdates, nsDisplayList* aDisplayList,
    nsDisplayListBuilder* aDisplayListBuilder, WebRenderScrollData& aScrollData,
    WrFiltersHolder&& aFilters) {
  AUTO_PROFILER_LABEL_CATEGORY_PAIR(GRAPHICS_WRDisplayList);

  StackingContextHelper sc;
  aScrollData = WebRenderScrollData(mManager);
  MOZ_ASSERT(mLayerScrollData.empty());
  mClipManager.BeginBuild(mManager, aBuilder);
  mBuilderDumpIndex = 0;
  mLastCanvasDatas.Clear();
  mLastLocalCanvasDatas.Clear();
  mLastAsr = nullptr;
  mContainsSVGGroup = false;
  MOZ_ASSERT(mDumpIndent == 0);

  {
    wr::StackingContextParams params;
    params.mFilters = std::move(aFilters.filters);
    params.mFilterDatas = std::move(aFilters.filter_datas);
    params.clip =
        wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());

    StackingContextHelper pageRootSc(sc, nullptr, nullptr, nullptr, aBuilder,
                                     params);
    if (ShouldDumpDisplayList(aDisplayListBuilder)) {
      mBuilderDumpIndex =
          aBuilder.Dump(mDumpIndent + 1, Some(mBuilderDumpIndex), Nothing());
    }
    CreateWebRenderCommandsFromDisplayList(aDisplayList, nullptr,
                                           aDisplayListBuilder, pageRootSc,
                                           aBuilder, aResourceUpdates);
  }

  // Make a "root" layer data that has everything else as descendants
  mLayerScrollData.emplace_back();
  mLayerScrollData.back().InitializeRoot(mLayerScrollData.size() - 1);
  auto callback =
      [&aScrollData](ScrollableLayerGuid::ViewID aScrollId) -> bool {
    return aScrollData.HasMetadataFor(aScrollId).isSome();
  };
  Maybe<ScrollMetadata> rootMetadata = nsLayoutUtils::GetRootMetadata(
      aDisplayListBuilder, mManager, ContainerLayerParameters(), callback);
  if (rootMetadata) {
    // Put the fallback root metadata on the rootmost layer that is
    // a matching async zoom container, or the root layer that we just
    // created above.
    size_t rootMetadataTarget = mLayerScrollData.size() - 1;
    for (size_t i = rootMetadataTarget; i > 0; i--) {
      if (auto zoomContainerId =
              mLayerScrollData[i - 1].GetAsyncZoomContainerId()) {
        if (*zoomContainerId == rootMetadata->GetMetrics().GetScrollId()) {
          rootMetadataTarget = i - 1;
          break;
        }
      }
    }
    mLayerScrollData[rootMetadataTarget].AppendScrollMetadata(
        aScrollData, rootMetadata.ref());
  }

  // Append the WebRenderLayerScrollData items into WebRenderScrollData
  // in reverse order, from topmost to bottommost. This is in keeping with
  // the semantics of WebRenderScrollData.
  for (auto it = mLayerScrollData.crbegin(); it != mLayerScrollData.crend();
       it++) {
    aScrollData.AddLayerData(*it);
  }
  mLayerScrollData.clear();
  mClipManager.EndBuild();

  // Remove the user data those are not displayed on the screen and
  // also reset the data to unused for next transaction.
  RemoveUnusedAndResetWebRenderUserData();
}

bool WebRenderCommandBuilder::ShouldDumpDisplayList(
    nsDisplayListBuilder* aBuilder) {
  return aBuilder != nullptr && aBuilder->IsInActiveDocShell() &&
         ((XRE_IsParentProcess() &&
           StaticPrefs::gfx_webrender_dl_dump_parent()) ||
          (XRE_IsContentProcess() &&
           StaticPrefs::gfx_webrender_dl_dump_content()));
}

void WebRenderCommandBuilder::CreateWebRenderCommands(
    nsDisplayItem* aItem, mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    nsDisplayListBuilder* aDisplayListBuilder) {
  auto* item = aItem->AsPaintedDisplayItem();
  MOZ_RELEASE_ASSERT(item, "Tried to paint item that cannot be painted");

  if (aBuilder.ReuseItem(item)) {
    // No further processing should be needed, since the item was reused.
    return;
  }

  aItem->SetPaintRect(aItem->GetBuildingRect());
  RenderRootStateManager* manager = mManager->GetRenderRootStateManager();

  // Note: this call to CreateWebRenderCommands can recurse back into
  // this function if the |item| is a wrapper for a sublist.
  const bool createdWRCommands = aItem->CreateWebRenderCommands(
      aBuilder, aResources, aSc, manager, aDisplayListBuilder);

  if (!createdWRCommands) {
    PushItemAsImage(aItem, aBuilder, aResources, aSc, aDisplayListBuilder);
  }
}

void WebRenderCommandBuilder::CreateWebRenderCommandsFromDisplayList(
    nsDisplayList* aDisplayList, nsDisplayItem* aWrappingItem,
    nsDisplayListBuilder* aDisplayListBuilder, const StackingContextHelper& aSc,
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources) {
  if (mDoGrouping) {
    MOZ_RELEASE_ASSERT(
        aWrappingItem,
        "Only the root list should have a null wrapping item, and mDoGrouping "
        "should never be true for the root list.");
    GP("actually entering the grouping code\n");
    DoGroupingForDisplayList(aDisplayList, aWrappingItem, aDisplayListBuilder,
                             aSc, aBuilder, aResources);
    return;
  }

  bool dumpEnabled = ShouldDumpDisplayList(aDisplayListBuilder);
  if (dumpEnabled) {
    // If we're inside a nested display list, print the WR DL items from the
    // wrapper item before we start processing the nested items.
    mBuilderDumpIndex =
        aBuilder.Dump(mDumpIndent + 1, Some(mBuilderDumpIndex), Nothing());
  }

  mDumpIndent++;
  mClipManager.BeginList(aSc);

  bool apzEnabled = mManager->AsyncPanZoomEnabled();

  FlattenedDisplayListIterator iter(aDisplayListBuilder, aDisplayList);
  while (iter.HasNext()) {
    nsDisplayItem* item = iter.GetNextItem();

    DisplayItemType itemType = item->GetType();

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

      // Refer to the comment on StackingContextHelper::mDeferredTransformItem
      // for an overview of what this is about. This bit of code applies to the
      // case where we are deferring a transform item, and we then need to defer
      // another transform with a different ASR. In such a case we cannot just
      // merge the deferred transforms, but need to force a new
      // WebRenderLayerScrollData item to flush the old deferred transform, so
      // that we can then start deferring the new one.
      if (!forceNewLayerData &&
          item->GetType() == DisplayItemType::TYPE_TRANSFORM &&
          aSc.GetDeferredTransformItem() &&
          (*aSc.GetDeferredTransformItem())->GetActiveScrolledRoot() != asr) {
        forceNewLayerData = true;
      }

      // If we're going to create a new layer data for this item, stash the
      // ASR so that if we recurse into a sublist they will know where to stop
      // walking up their ASR chain when building scroll metadata.
      if (forceNewLayerData) {
        mAsrStack.push_back(asr);
      }
    }

    // This is where we emulate the clip/scroll stack that was previously
    // implemented on the WR display list side.
    auto spaceAndClipChain = mClipManager.SwitchItem(item);
    wr::SpaceAndClipChainHelper saccHelper(aBuilder, spaceAndClipChain);

    {  // scope restoreDoGrouping
      AutoRestore<bool> restoreDoGrouping(mDoGrouping);
      if (itemType == DisplayItemType::TYPE_SVG_WRAPPER) {
        // Inside an <svg>, all display items that are not LAYER_ACTIVE wrapper
        // display items (like animated transforms / opacity) share the same
        // animated geometry root, so we can combine subsequent items of that
        // type into the same image.
        mContainsSVGGroup = mDoGrouping = true;
        GP("attempting to enter the grouping code\n");
      }

      if (dumpEnabled) {
        std::stringstream ss;
        nsFrame::PrintDisplayItem(aDisplayListBuilder, item, ss,
                                  static_cast<uint32_t>(mDumpIndent));
        printf_stderr("%s", ss.str().c_str());
      }

      CreateWebRenderCommands(item, aBuilder, aResources, aSc,
                              aDisplayListBuilder);

      if (dumpEnabled) {
        mBuilderDumpIndex =
            aBuilder.Dump(mDumpIndent + 1, Some(mBuilderDumpIndex), Nothing());
      }
    }

    if (apzEnabled) {
      if (forceNewLayerData) {
        // Pop the thing we pushed before the recursion, so the topmost item on
        // the stack is enclosing display item's ASR (or the stack is empty)
        mAsrStack.pop_back();
        const ActiveScrolledRoot* stopAtAsr =
            mAsrStack.empty() ? nullptr : mAsrStack.back();

        int32_t descendants =
            mLayerScrollData.size() - layerCountBeforeRecursing;

        // See the comments on StackingContextHelper::mDeferredTransformItem
        // for an overview of what deferred transforms are.
        // In the case where we deferred a transform, but have a child display
        // item with a different ASR than the deferred transform item, we cannot
        // put the transform on the WebRenderLayerScrollData item for the child.
        // We cannot do this because it will not conform to APZ's expectations
        // with respect to how the APZ tree ends up structured. In particular,
        // the GetTransformToThis() for the child APZ (which is created for the
        // child item's ASR) will not include the transform when we actually do
        // want it to.
        // When we run into this scenario, we solve it by creating two
        // WebRenderLayerScrollData items; one that just holds the transform,
        // that we deferred, and a child WebRenderLayerScrollData item that
        // holds the scroll metadata for the child's ASR.
        Maybe<nsDisplayTransform*> deferred = aSc.GetDeferredTransformItem();
        if (deferred && (*deferred)->GetActiveScrolledRoot() !=
                            item->GetActiveScrolledRoot()) {
          // This creates the child WebRenderLayerScrollData for |item|, but
          // omits the transform (hence the Nothing() as the last argument to
          // Initialize(...)). We also need to make sure that the ASR from
          // the deferred transform item is not on this node, so we use that
          // ASR as the "stop at" ASR for this WebRenderLayerScrollData.
          mLayerScrollData.emplace_back();
          mLayerScrollData.back().Initialize(
              mManager->GetScrollData(), item, descendants,
              (*deferred)->GetActiveScrolledRoot(), Nothing());

          // The above WebRenderLayerScrollData will also be a descendant of
          // the transform-holding WebRenderLayerScrollData we create below.
          descendants++;

          // This creates the WebRenderLayerScrollData for the deferred
          // transform item. This holds the transform matrix and the remaining
          // ASRs needed to complete the ASR chain (i.e. the ones from the
          // stopAtAsr down to the deferred transform item's ASR, which must be
          // "between" stopAtAsr and |item|'s ASR in the ASR tree).
          mLayerScrollData.emplace_back();
          mLayerScrollData.back().Initialize(mManager->GetScrollData(),
                                             *deferred, descendants, stopAtAsr,
                                             aSc.GetDeferredTransformMatrix());
        } else {
          // This is the "simple" case where we don't need to create two
          // WebRenderLayerScrollData items; we can just create one that also
          // holds the deferred transform matrix, if any.
          mLayerScrollData.emplace_back();
          mLayerScrollData.back().Initialize(mManager->GetScrollData(), item,
                                             descendants, stopAtAsr,
                                             aSc.GetDeferredTransformMatrix());
        }
      }
    }
  }

  mDumpIndent--;
  mClipManager.EndList(aSc);
}

void WebRenderCommandBuilder::PushOverrideForASR(
    const ActiveScrolledRoot* aASR, const wr::WrSpatialId& aSpatialId) {
  mClipManager.PushOverrideForASR(aASR, aSpatialId);
}

void WebRenderCommandBuilder::PopOverrideForASR(
    const ActiveScrolledRoot* aASR) {
  mClipManager.PopOverrideForASR(aASR);
}

Maybe<wr::ImageKey> WebRenderCommandBuilder::CreateImageKey(
    nsDisplayItem* aItem, ImageContainer* aContainer,
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    mozilla::wr::ImageRendering aRendering, const StackingContextHelper& aSc,
    gfx::IntSize& aSize, const Maybe<LayoutDeviceRect>& aAsyncImageBounds) {
  RefPtr<WebRenderImageData> imageData =
      CreateOrRecycleWebRenderUserData<WebRenderImageData>(
          aItem, aBuilder.GetRenderRoot());
  MOZ_ASSERT(imageData);

  if (aContainer->IsAsync()) {
    MOZ_ASSERT(aAsyncImageBounds);

    LayoutDeviceRect rect = aAsyncImageBounds.value();
    LayoutDeviceRect scBounds(LayoutDevicePoint(0, 0), rect.Size());
    gfx::MaybeIntSize scaleToSize;
    if (!aContainer->GetScaleHint().IsEmpty()) {
      scaleToSize = Some(aContainer->GetScaleHint());
    }
    gfx::Matrix4x4 transform =
        gfx::Matrix4x4::From2D(aContainer->GetTransformHint());
    // TODO!
    // We appear to be using the image bridge for a lot (most/all?) of
    // layers-free image handling and that breaks frame consistency.
    imageData->CreateAsyncImageWebRenderCommands(
        aBuilder, aContainer, aSc, rect, scBounds, transform, scaleToSize,
        aRendering, wr::MixBlendMode::Normal, !aItem->BackfaceIsHidden());
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

bool WebRenderCommandBuilder::PushImage(
    nsDisplayItem* aItem, ImageContainer* aContainer,
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, const LayoutDeviceRect& aRect,
    const LayoutDeviceRect& aClip) {
  mozilla::wr::ImageRendering rendering = wr::ToImageRendering(
      nsLayoutUtils::GetSamplingFilterForFrame(aItem->Frame()));
  gfx::IntSize size;
  Maybe<wr::ImageKey> key =
      CreateImageKey(aItem, aContainer, aBuilder, aResources, rendering, aSc,
                     size, Some(aRect));
  if (aContainer->IsAsync()) {
    // Async ImageContainer does not create ImageKey, instead it uses Pipeline.
    MOZ_ASSERT(key.isNothing());
    return true;
  }
  if (!key) {
    return false;
  }

  auto r = wr::ToLayoutRect(aRect);
  auto c = wr::ToLayoutRect(aClip);
  aBuilder.PushImage(r, c, !aItem->BackfaceIsHidden(), rendering, key.value());

  return true;
}

bool BuildLayer(nsDisplayItem* aItem, BlobItemData* aData,
                nsDisplayListBuilder* aDisplayListBuilder,
                const gfx::Size& aScale) {
  if (!aData->mLayerManager) {
    aData->mLayerManager =
        new BasicLayerManager(BasicLayerManager::BLM_INACTIVE);
  }
  RefPtr<BasicLayerManager> blm = aData->mLayerManager;
  UniquePtr<LayerProperties> props;
  if (blm->GetRoot()) {
    props = LayerProperties::CloneFrom(blm->GetRoot());
  }
  FrameLayerBuilder* layerBuilder = new FrameLayerBuilder();
  layerBuilder->Init(aDisplayListBuilder, blm, nullptr, true);
  layerBuilder->DidBeginRetainedLayerTransaction(blm);

  blm->BeginTransaction();
  bool isInvalidated = false;

  ContainerLayerParameters param(aScale.width, aScale.height);
  RefPtr<Layer> root = aItem->AsPaintedDisplayItem()->BuildLayer(
      aDisplayListBuilder, blm, param);

  if (root) {
    blm->SetRoot(root);
    layerBuilder->WillEndTransaction();

    // Check if there is any invalidation region.
    nsIntRegion invalid;
    if (props) {
      props->ComputeDifferences(root, invalid, nullptr);
      if (!invalid.IsEmpty()) {
        isInvalidated = true;
      }
    } else {
      isInvalidated = true;
    }
  }
  blm->AbortTransaction();

  return isInvalidated;
}

static bool PaintByLayer(nsDisplayItem* aItem,
                         nsDisplayListBuilder* aDisplayListBuilder,
                         const RefPtr<BasicLayerManager>& aManager,
                         gfxContext* aContext, const gfx::Size& aScale,
                         const std::function<void()>& aPaintFunc) {
  UniquePtr<LayerProperties> props;
  if (aManager->GetRoot()) {
    props = LayerProperties::CloneFrom(aManager->GetRoot());
  }
  FrameLayerBuilder* layerBuilder = new FrameLayerBuilder();
  layerBuilder->Init(aDisplayListBuilder, aManager, nullptr, true);
  layerBuilder->DidBeginRetainedLayerTransaction(aManager);

  aManager->SetDefaultTarget(aContext);
  nsCString none;
  aManager->BeginTransactionWithTarget(aContext, none);
  bool isInvalidated = false;

  ContainerLayerParameters param(aScale.width, aScale.height);
  RefPtr<Layer> root = aItem->AsPaintedDisplayItem()->BuildLayer(
      aDisplayListBuilder, aManager, param);

  if (root) {
    aManager->SetRoot(root);
    layerBuilder->WillEndTransaction();

    aPaintFunc();

    // Check if there is any invalidation region.
    nsIntRegion invalid;
    if (props) {
      props->ComputeDifferences(root, invalid, nullptr);
      if (!invalid.IsEmpty()) {
        isInvalidated = true;
      }
    } else {
      isInvalidated = true;
    }
  }

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::DumpDisplayList() || gfxEnv::DumpPaint()) {
    fprintf_stderr(
        gfxUtils::sDumpPaintFile,
        "Basic layer tree for painting contents of display item %s(%p):\n",
        aItem->Name(), aItem->Frame());
    std::stringstream stream;
    aManager->Dump(stream, "", gfxEnv::DumpPaintToFile());
    fprint_stderr(
        gfxUtils::sDumpPaintFile,
        stream);  // not a typo, fprint_stderr declared in LayersLogging.h
  }
#endif

  if (aManager->InTransaction()) {
    aManager->AbortTransaction();
  }

  aManager->SetTarget(nullptr);
  aManager->SetDefaultTarget(nullptr);

  return isInvalidated;
}

static bool PaintItemByDrawTarget(nsDisplayItem* aItem, gfx::DrawTarget* aDT,
                                  const LayoutDevicePoint& aOffset,
                                  const IntRect& visibleRect,
                                  nsDisplayListBuilder* aDisplayListBuilder,
                                  const RefPtr<BasicLayerManager>& aManager,
                                  const gfx::Size& aScale,
                                  Maybe<gfx::DeviceColor>& aHighlight) {
  MOZ_ASSERT(aDT);

  bool isInvalidated = false;
  // XXX Why is this ClearRect() needed?
  aDT->ClearRect(Rect(visibleRect));
  RefPtr<gfxContext> context = gfxContext::CreateOrNull(aDT);
  MOZ_ASSERT(context);

  switch (aItem->GetType()) {
    case DisplayItemType::TYPE_SVG_WRAPPER:
    case DisplayItemType::TYPE_MASK: {
      // These items should be handled by other code paths
      MOZ_RELEASE_ASSERT(0);
      break;
    }
    case DisplayItemType::TYPE_FILTER: {
      context->SetMatrix(context->CurrentMatrix()
                             .PreScale(aScale.width, aScale.height)
                             .PreTranslate(-aOffset.x, -aOffset.y));
      isInvalidated = PaintByLayer(
          aItem, aDisplayListBuilder, aManager, context, {1, 1}, [&]() {
            static_cast<nsDisplayFilters*>(aItem)->PaintAsLayer(
                aDisplayListBuilder, context, aManager);
          });
      break;
    }

    default:
      if (!aItem->AsPaintedDisplayItem()) {
        break;
      }

      context->SetMatrix(context->CurrentMatrix()
                             .PreScale(aScale.width, aScale.height)
                             .PreTranslate(-aOffset.x, -aOffset.y));
      if (aDisplayListBuilder->IsPaintingToWindow()) {
        aItem->Frame()->AddStateBits(NS_FRAME_PAINTED_THEBES);
      }
      aItem->AsPaintedDisplayItem()->Paint(aDisplayListBuilder, context);
      isInvalidated = true;
      break;
  }

  if (aItem->GetType() != DisplayItemType::TYPE_MASK) {
    // Apply highlight fills, if the appropriate prefs are set.
    // We don't do this for masks because we'd be filling the A8 mask surface,
    // which isn't very useful.
    if (aHighlight) {
      aDT->SetTransform(gfx::Matrix());
      aDT->FillRect(Rect(visibleRect), gfx::ColorPattern(aHighlight.value()));
    }
    if (aItem->Frame()->PresContext()->GetPaintFlashing() && isInvalidated) {
      aDT->SetTransform(gfx::Matrix());
      float r = float(rand()) / float(RAND_MAX);
      float g = float(rand()) / float(RAND_MAX);
      float b = float(rand()) / float(RAND_MAX);
      aDT->FillRect(Rect(visibleRect),
                    gfx::ColorPattern(gfx::DeviceColor(r, g, b, 0.5)));
    }
  }

  return isInvalidated;
}

// When drawing fallback images we create either
// a real image or a blob image that will contain the display item.
// In the case of a blob image we paint the item at 0,0 instead
// of trying to keep at aItem->GetBounds().TopLeft() like we do
// with SVG. We do this because there's not necessarily a reference frame
// between us and the rest of the world so the the coordinates
// that we get for the bounds are not necessarily stable across scrolling
// or other movement.
already_AddRefed<WebRenderFallbackData>
WebRenderCommandBuilder::GenerateFallbackData(
    nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources, const StackingContextHelper& aSc,
    nsDisplayListBuilder* aDisplayListBuilder, LayoutDeviceRect& aImageRect) {
  const bool paintOnContentSide = aItem->MustPaintOnContentSide();
  bool useBlobImage =
      StaticPrefs::gfx_webrender_blob_images() && !paintOnContentSide;
  Maybe<gfx::DeviceColor> highlight = Nothing();
  if (StaticPrefs::gfx_webrender_highlight_painted_layers()) {
    highlight = Some(useBlobImage ? gfx::DeviceColor(1.0, 0.0, 0.0, 0.5)
                                  : gfx::DeviceColor(1.0, 1.0, 0.0, 0.5));
  }

  RefPtr<WebRenderFallbackData> fallbackData =
      CreateOrRecycleWebRenderUserData<WebRenderFallbackData>(
          aItem, aBuilder.GetRenderRoot());

  bool snap;
  nsRect itemBounds = aItem->GetBounds(aDisplayListBuilder, &snap);

  // Blob images will only draw the visible area of the blob so we don't need to
  // clip them here and can just rely on the webrender clipping.
  // TODO We also don't clip native themed widget to avoid over-invalidation
  // during scrolling. It would be better to support a sort of streaming/tiling
  // scheme for large ones but the hope is that we should not have large native
  // themed items.
  nsRect paintBounds = (useBlobImage || paintOnContentSide)
                           ? itemBounds
                           : aItem->GetClippedBounds(aDisplayListBuilder);

  // nsDisplayItem::Paint() may refer the variables that come from
  // ComputeVisibility(). So we should call ComputeVisibility() before painting.
  // e.g.: nsDisplayBoxShadowInner uses mPaintRect in Paint() and mPaintRect is
  // computed in nsDisplayBoxShadowInner::ComputeVisibility().
  nsRegion visibleRegion(paintBounds);
  aItem->SetPaintRect(paintBounds);
  aItem->ComputeVisibility(aDisplayListBuilder, &visibleRegion);

  const int32_t appUnitsPerDevPixel =
      aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
  auto bounds =
      LayoutDeviceRect::FromAppUnits(paintBounds, appUnitsPerDevPixel);
  if (bounds.IsEmpty()) {
    return nullptr;
  }

  gfx::Size scale = aSc.GetInheritedScale();
  gfx::Size oldScale = fallbackData->mScale;
  // We tolerate slight changes in scale so that we don't, for example,
  // rerasterize on MotionMark
  bool differentScale = gfx::FuzzyEqual(scale.width, oldScale.width, 1e-6f) &&
                        gfx::FuzzyEqual(scale.height, oldScale.height, 1e-6f);

  LayoutDeviceToLayerScale2D layerScale(scale.width, scale.height);

  auto trans =
      ViewAs<LayerPixel>(aSc.GetSnappingSurfaceTransform().GetTranslation());
  auto snappedTrans = LayerIntPoint::Floor(trans);
  LayerPoint residualOffset = trans - snappedTrans;

  nsRegion opaqueRegion = aItem->GetOpaqueRegion(aDisplayListBuilder, &snap);
  wr::OpacityType opacity = opaqueRegion.Contains(paintBounds)
                                ? wr::OpacityType::Opaque
                                : wr::OpacityType::HasAlphaChannel;

  LayerIntRect dtRect, visibleRect;
  // If we think the item is opaque we round the bounds
  // to the nearest pixel instead of rounding them out. If we rounded
  // out we'd potentially introduce transparent pixels.
  //
  // Ideally we'd be able to ask an item its bounds in pixels and whether
  // they're all opaque. Unfortunately no such API exists so we currently
  // just hope that we get it right.
  if (opacity == wr::OpacityType::Opaque && snap) {
    dtRect = LayerIntRect::FromUnknownRect(
        ScaleToNearestPixelsOffset(paintBounds, scale.width, scale.height,
                                   appUnitsPerDevPixel, residualOffset));

    visibleRect = LayerIntRect::FromUnknownRect(
                      ScaleToNearestPixelsOffset(
                          aItem->GetBuildingRect(), scale.width, scale.height,
                          appUnitsPerDevPixel, residualOffset))
                      .Intersect(dtRect);
  } else {
    dtRect = LayerIntRect::FromUnknownRect(
        ScaleToOutsidePixelsOffset(paintBounds, scale.width, scale.height,
                                   appUnitsPerDevPixel, residualOffset));

    visibleRect = LayerIntRect::FromUnknownRect(
                      ScaleToOutsidePixelsOffset(
                          aItem->GetBuildingRect(), scale.width, scale.height,
                          appUnitsPerDevPixel, residualOffset))
                      .Intersect(dtRect);
  }

  auto visibleSize = visibleRect.Size();
  // these rectangles can overflow from scaling so try to
  // catch that with IsEmpty() checks. See bug 1622126.
  if (visibleSize.IsEmpty() || dtRect.IsEmpty()) {
    return nullptr;
  }

  if (useBlobImage) {
    // Display item bounds should be unscaled
    aImageRect = visibleRect / layerScale;
  } else {
    // Display item bounds should be unscaled
    aImageRect = dtRect / layerScale;
  }

  // We always paint items at 0,0 so the visibleRect that we use inside the blob
  // is needs to be adjusted by the display item bounds top left.
  visibleRect -= dtRect.TopLeft();

  nsDisplayItemGeometry* geometry = fallbackData->mGeometry.get();

  bool needPaint = true;

  // nsDisplayFilters is rendered via BasicLayerManager which means the
  // invalidate region is unknown until we traverse the displaylist contained by
  // it.
  if (geometry && !fallbackData->IsInvalid() &&
      aItem->GetType() != DisplayItemType::TYPE_FILTER &&
      aItem->GetType() != DisplayItemType::TYPE_SVG_WRAPPER && differentScale) {
    nsRect invalid;
    nsRegion invalidRegion;

    if (aItem->IsInvalid(invalid)) {
      invalidRegion.OrWith(paintBounds);
    } else {
      nsPoint shift = itemBounds.TopLeft() - geometry->mBounds.TopLeft();
      geometry->MoveBy(shift);
      aItem->ComputeInvalidationRegion(aDisplayListBuilder, geometry,
                                       &invalidRegion);

      nsRect lastBounds = fallbackData->mBounds;
      lastBounds.MoveBy(shift);

      if (!lastBounds.IsEqualInterior(paintBounds)) {
        invalidRegion.OrWith(lastBounds);
        invalidRegion.OrWith(paintBounds);
      }
    }
    needPaint = !invalidRegion.IsEmpty();
  }

  if (needPaint || !fallbackData->GetImageKey()) {
    fallbackData->mGeometry =
        WrapUnique(aItem->AllocateGeometry(aDisplayListBuilder));

    gfx::SurfaceFormat format = aItem->GetType() == DisplayItemType::TYPE_MASK
                                    ? gfx::SurfaceFormat::A8
                                    : (opacity == wr::OpacityType::Opaque
                                           ? gfx::SurfaceFormat::B8G8R8X8
                                           : gfx::SurfaceFormat::B8G8R8A8);
    if (useBlobImage) {
      MOZ_ASSERT(!opaqueRegion.IsComplex());

      std::vector<RefPtr<ScaledFont>> fonts;
      bool validFonts = true;
      RefPtr<WebRenderDrawEventRecorder> recorder =
          MakeAndAddRef<WebRenderDrawEventRecorder>(
              [&](MemStream& aStream,
                  std::vector<RefPtr<ScaledFont>>& aScaledFonts) {
                size_t count = aScaledFonts.size();
                aStream.write((const char*)&count, sizeof(count));
                for (auto& scaled : aScaledFonts) {
                  Maybe<wr::FontInstanceKey> key =
                      mManager->WrBridge()->GetFontKeyForScaledFont(
                          scaled, aBuilder.GetRenderRoot(), &aResources);
                  if (key.isNothing()) {
                    validFonts = false;
                    break;
                  }
                  BlobFont font = {key.value(), scaled};
                  aStream.write((const char*)&font, sizeof(font));
                }
                fonts = std::move(aScaledFonts);
              });
      RefPtr<gfx::DrawTarget> dummyDt = gfx::Factory::CreateDrawTarget(
          gfx::BackendType::SKIA, gfx::IntSize(1, 1), format);
      RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateRecordingDrawTarget(
          recorder, dummyDt, (dtRect - dtRect.TopLeft()).ToUnknownRect());
      if (!fallbackData->mBasicLayerManager) {
        fallbackData->mBasicLayerManager =
            new BasicLayerManager(BasicLayerManager::BLM_INACTIVE);
      }
      bool isInvalidated = PaintItemByDrawTarget(
          aItem, dt, (dtRect / layerScale).TopLeft(),
          /*aVisibleRect: */ dt->GetRect(), aDisplayListBuilder,
          fallbackData->mBasicLayerManager, scale, highlight);
      if (!isInvalidated) {
        if (!aItem->GetBuildingRect().IsEqualInterior(
                fallbackData->mBuildingRect)) {
          // The building rect has changed but we didn't see any invalidations.
          // We should still consider this an invalidation.
          isInvalidated = true;
        }
      }

      // the item bounds are relative to the blob origin which is
      // dtRect.TopLeft()
      recorder->FlushItem((dtRect - dtRect.TopLeft()).ToUnknownRect());
      recorder->Finish();

      if (!validFonts) {
        gfxCriticalNote << "Failed serializing fonts for blob image";
        return nullptr;
      }

      if (isInvalidated) {
        Range<uint8_t> bytes((uint8_t*)recorder->mOutputStream.mData,
                             recorder->mOutputStream.mLength);
        wr::BlobImageKey key =
            wr::BlobImageKey{mManager->WrBridge()->GetNextImageKey()};
        wr::ImageDescriptor descriptor(visibleSize.ToUnknownSize(), 0,
                                       dt->GetFormat(), opacity);
        if (!aResources.AddBlobImage(
                key, descriptor, bytes,
                ViewAs<ImagePixel>(visibleRect,
                                   PixelCastJustification::LayerIsImage))) {
          return nullptr;
        }
        TakeExternalSurfaces(recorder, fallbackData->mExternalSurfaces,
                             mManager->GetRenderRootStateManager(), aResources);
        fallbackData->SetBlobImageKey(key);
        fallbackData->SetFonts(fonts);
      } else {
        // If there is no invalidation region and we don't have a image key,
        // it means we don't need to push image for the item.
        if (!fallbackData->GetBlobImageKey().isSome()) {
          return nullptr;
        }
      }
    } else {
      WebRenderImageData* imageData = fallbackData->PaintIntoImage();

      imageData->CreateImageClientIfNeeded();
      RefPtr<ImageClient> imageClient = imageData->GetImageClient();
      RefPtr<ImageContainer> imageContainer =
          LayerManager::CreateImageContainer();
      bool isInvalidated = false;

      {
        UpdateImageHelper helper(imageContainer, imageClient,
                                 dtRect.Size().ToUnknownSize(), format);
        {
          RefPtr<gfx::DrawTarget> dt = helper.GetDrawTarget();
          if (!dt) {
            return nullptr;
          }
          if (!fallbackData->mBasicLayerManager) {
            fallbackData->mBasicLayerManager =
                new BasicLayerManager(mManager->GetWidget());
          }
          isInvalidated = PaintItemByDrawTarget(
              aItem, dt,
              /*aOffset: */ aImageRect.TopLeft(),
              /*aVisibleRect: */ dt->GetRect(), aDisplayListBuilder,
              fallbackData->mBasicLayerManager, scale, highlight);
        }

        if (isInvalidated) {
          // Update image if there it's invalidated.
          if (!helper.UpdateImage(aBuilder.GetRenderRoot())) {
            return nullptr;
          }
        } else {
          // If there is no invalidation region and we don't have a image key,
          // it means we don't need to push image for the item.
          if (!imageData->GetImageKey().isSome()) {
            return nullptr;
          }
        }
      }

      // Force update the key in fallback data since we repaint the image in
      // this path. If not force update, fallbackData may reuse the original key
      // because it doesn't know UpdateImageHelper already updated the image
      // container.
      if (isInvalidated &&
          !imageData->UpdateImageKey(imageContainer, aResources, true)) {
        return nullptr;
      }
    }

    fallbackData->mScale = scale;
    fallbackData->SetInvalid(false);
  }

  if (useBlobImage) {
    aResources.SetBlobImageVisibleArea(
        fallbackData->GetBlobImageKey().value(),
        ViewAs<ImagePixel>(visibleRect, PixelCastJustification::LayerIsImage));
  }

  // Update current bounds to fallback data
  fallbackData->mBounds = paintBounds;
  fallbackData->mBuildingRect = aItem->GetBuildingRect();

  MOZ_ASSERT(fallbackData->GetImageKey());

  return fallbackData.forget();
}

class WebRenderMaskData : public WebRenderUserData {
 public:
  explicit WebRenderMaskData(RenderRootStateManager* aManager,
                             nsDisplayItem* aItem)
      : WebRenderUserData(aManager, aItem),
        mMaskStyle(nsStyleImageLayers::LayerType::Mask),
        mShouldHandleOpacity(false) {
    MOZ_COUNT_CTOR(WebRenderMaskData);
  }
  virtual ~WebRenderMaskData() {
    MOZ_COUNT_DTOR(WebRenderMaskData);
    ClearImageKey();
  }

  void ClearImageKey() {
    if (mBlobKey) {
      mManager->AddBlobImageKeyForDiscard(mBlobKey.value());
    }
    mBlobKey.reset();
  }

  UserDataType GetType() override { return UserDataType::eMask; }
  static UserDataType Type() { return UserDataType::eMask; }

  Maybe<wr::BlobImageKey> mBlobKey;
  std::vector<RefPtr<gfx::ScaledFont>> mFonts;
  std::vector<RefPtr<gfx::SourceSurface>> mExternalSurfaces;
  LayerIntRect mItemRect;
  nsPoint mMaskOffset;
  nsStyleImageLayers mMaskStyle;
  gfx::Size mScale;
  bool mShouldHandleOpacity;
};

Maybe<wr::ImageMask> WebRenderCommandBuilder::BuildWrMaskImage(
    nsDisplayMasksAndClipPaths* aMaskItem, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources, const StackingContextHelper& aSc,
    nsDisplayListBuilder* aDisplayListBuilder,
    const LayoutDeviceRect& aBounds) {
  RefPtr<WebRenderMaskData> maskData =
      CreateOrRecycleWebRenderUserData<WebRenderMaskData>(
          aMaskItem, aBuilder.GetRenderRoot());

  if (!maskData) {
    return Nothing();
  }

  bool snap;
  nsRect bounds = aMaskItem->GetBounds(aDisplayListBuilder, &snap);

  const int32_t appUnitsPerDevPixel =
      aMaskItem->Frame()->PresContext()->AppUnitsPerDevPixel();

  Size scale = aSc.GetInheritedScale();
  Size oldScale = maskData->mScale;
  // This scale determination should probably be done using
  // ChooseScaleAndSetTransform but for now we just fake it.
  // We tolerate slight changes in scale so that we don't, for example,
  // rerasterize on MotionMark
  bool sameScale = FuzzyEqual(scale.width, oldScale.width, 1e-6f) &&
                   FuzzyEqual(scale.height, oldScale.height, 1e-6f);

  LayerIntRect itemRect =
      LayerIntRect::FromUnknownRect(bounds.ScaleToOutsidePixels(
          scale.width, scale.height, appUnitsPerDevPixel));

  if (itemRect.IsEmpty()) {
    return Nothing();
  }

  LayoutDeviceToLayerScale2D layerScale(scale.width, scale.height);
  LayoutDeviceRect imageRect = LayerRect(itemRect) / layerScale;

  nsPoint maskOffset = aMaskItem->ToReferenceFrame() - bounds.TopLeft();

  nsRect dirtyRect;
  // If this mask item is being painted for the first time, some members of
  // WebRenderMaskData are still default initialized. This is intentional.
  if (aMaskItem->IsInvalid(dirtyRect) ||
      !itemRect.IsEqualInterior(maskData->mItemRect) ||
      !(aMaskItem->Frame()->StyleSVGReset()->mMask == maskData->mMaskStyle) ||
      maskOffset != maskData->mMaskOffset || !sameScale ||
      aMaskItem->ShouldHandleOpacity() != maskData->mShouldHandleOpacity) {
    IntSize size = itemRect.Size().ToUnknownSize();

    std::vector<RefPtr<ScaledFont>> fonts;
    bool validFonts = true;
    RefPtr<WebRenderDrawEventRecorder> recorder =
        MakeAndAddRef<WebRenderDrawEventRecorder>(
            [&](MemStream& aStream,
                std::vector<RefPtr<ScaledFont>>& aScaledFonts) {
              size_t count = aScaledFonts.size();
              aStream.write((const char*)&count, sizeof(count));

              for (auto& scaled : aScaledFonts) {
                Maybe<wr::FontInstanceKey> key =
                    mManager->WrBridge()->GetFontKeyForScaledFont(
                        scaled, aBuilder.GetRenderRoot(), &aResources);
                if (key.isNothing()) {
                  validFonts = false;
                  break;
                }
                BlobFont font = {key.value(), scaled};
                aStream.write((const char*)&font, sizeof(font));
              }

              fonts = std::move(aScaledFonts);
            });

    RefPtr<DrawTarget> dummyDt = Factory::CreateDrawTarget(
        BackendType::SKIA, IntSize(1, 1), SurfaceFormat::A8);
    RefPtr<DrawTarget> dt = Factory::CreateRecordingDrawTarget(
        recorder, dummyDt, IntRect(IntPoint(0, 0), size));

    RefPtr<gfxContext> context = gfxContext::CreateOrNull(dt);
    MOZ_ASSERT(context);

    context->SetMatrix(context->CurrentMatrix()
                           .PreTranslate(-itemRect.x, -itemRect.y)
                           .PreScale(scale.width, scale.height));

    bool maskPainted = false;
    bool maskIsComplete =
        aMaskItem->PaintMask(aDisplayListBuilder, context, &maskPainted);
    if (!maskPainted) {
      return Nothing();
    }

    // If a mask is incomplete or missing (e.g. it's display: none) the proper
    // behaviour depends on the masked frame being html or svg.
    //
    // For an HTML frame:
    //   According to css-masking spec, always create a mask surface when
    //   we have any item in maskFrame even if all of those items are
    //   non-resolvable <mask-sources> or <images> so continue with the
    //   painting code. Note that in a common case of no layer of the mask being
    //   complete or even partially complete then the mask surface will be
    //   transparent black so this results in hiding the frame.
    // For an SVG frame:
    //   SVG 1.1 say that if we fail to resolve a mask, we should draw the
    //   object unmasked so return Nothing().
    if (!maskIsComplete &&
        (aMaskItem->Frame()->GetStateBits() & NS_FRAME_SVG_LAYOUT)) {
      return Nothing();
    }

    recorder->FlushItem(IntRect(0, 0, size.width, size.height));
    recorder->Finish();

    if (!validFonts) {
      gfxCriticalNote << "Failed serializing fonts for blob mask image";
      return Nothing();
    }

    Range<uint8_t> bytes((uint8_t*)recorder->mOutputStream.mData,
                         recorder->mOutputStream.mLength);
    wr::BlobImageKey key =
        wr::BlobImageKey{mManager->WrBridge()->GetNextImageKey()};
    wr::ImageDescriptor descriptor(size, 0, dt->GetFormat(),
                                   wr::OpacityType::HasAlphaChannel);
    if (!aResources.AddBlobImage(key, descriptor, bytes,
                                 ImageIntRect(0, 0, size.width, size.height))) {
      return Nothing();
    }
    maskData->ClearImageKey();
    maskData->mBlobKey = Some(key);
    maskData->mFonts = fonts;
    TakeExternalSurfaces(recorder, maskData->mExternalSurfaces,
                         mManager->GetRenderRootStateManager(), aResources);
    if (maskIsComplete) {
      maskData->mItemRect = itemRect;
      maskData->mMaskOffset = maskOffset;
      maskData->mScale = scale;
      maskData->mMaskStyle = aMaskItem->Frame()->StyleSVGReset()->mMask;
      maskData->mShouldHandleOpacity = aMaskItem->ShouldHandleOpacity();
    }
  }

  wr::ImageMask imageMask;
  imageMask.image = wr::AsImageKey(maskData->mBlobKey.value());
  imageMask.rect = wr::ToLayoutRect(imageRect);
  imageMask.repeat = false;
  return Some(imageMask);
}

bool WebRenderCommandBuilder::PushItemAsImage(
    nsDisplayItem* aItem, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources, const StackingContextHelper& aSc,
    nsDisplayListBuilder* aDisplayListBuilder) {
  LayoutDeviceRect imageRect;
  RefPtr<WebRenderFallbackData> fallbackData = GenerateFallbackData(
      aItem, aBuilder, aResources, aSc, aDisplayListBuilder, imageRect);
  if (!fallbackData) {
    return false;
  }

  wr::LayoutRect dest = wr::ToLayoutRect(imageRect);
  gfx::SamplingFilter sampleFilter =
      nsLayoutUtils::GetSamplingFilterForFrame(aItem->Frame());
  aBuilder.PushImage(dest, dest, !aItem->BackfaceIsHidden(),
                     wr::ToImageRendering(sampleFilter),
                     fallbackData->GetImageKey().value());
  return true;
}

void WebRenderCommandBuilder::RemoveUnusedAndResetWebRenderUserData() {
  for (auto iter = mWebRenderUserDatas.Iter(); !iter.Done(); iter.Next()) {
    WebRenderUserData* data = iter.Get()->GetKey();
    if (!data->IsUsed()) {
      nsIFrame* frame = data->GetFrame();

      MOZ_ASSERT(frame->HasProperty(WebRenderUserDataProperty::Key()));

      WebRenderUserDataTable* userDataTable =
          frame->GetProperty(WebRenderUserDataProperty::Key());

      MOZ_ASSERT(userDataTable->Count());

      userDataTable->Remove(
          WebRenderUserDataKey(data->GetDisplayItemKey(), data->GetType()));

      if (!userDataTable->Count()) {
        frame->RemoveProperty(WebRenderUserDataProperty::Key());
        userDataTable = nullptr;
      }

      switch (data->GetType()) {
        case WebRenderUserData::UserDataType::eCanvas:
          mLastCanvasDatas.RemoveEntry(data->AsCanvasData());
          break;
        case WebRenderUserData::UserDataType::eLocalCanvas:
          mLastLocalCanvasDatas.RemoveEntry(data->AsLocalCanvasData());
          break;
        default:
          break;
      }

      iter.Remove();
      continue;
    }

    data->SetUsed(false);
  }
}

void WebRenderCommandBuilder::ClearCachedResources() {
  RemoveUnusedAndResetWebRenderUserData();
  // UserDatas should only be in the used state during a call to
  // WebRenderCommandBuilder::BuildWebRenderCommands The should always be false
  // upon return from BuildWebRenderCommands().
  MOZ_RELEASE_ASSERT(mWebRenderUserDatas.Count() == 0);
}

WebRenderGroupData::WebRenderGroupData(
    RenderRootStateManager* aRenderRootStateManager, nsDisplayItem* aItem)
    : WebRenderUserData(aRenderRootStateManager, aItem) {
  MOZ_COUNT_CTOR(WebRenderGroupData);
}

WebRenderGroupData::~WebRenderGroupData() {
  MOZ_COUNT_DTOR(WebRenderGroupData);
  GP("Group data destruct\n");
  mSubGroup.ClearImageKey(mManager, true);
  mFollowingGroup.ClearImageKey(mManager, true);
}

}  // namespace layers
}  // namespace mozilla
