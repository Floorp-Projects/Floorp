/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ClipManager.h"

#include "DisplayItemClipChain.h"
#include "FrameMetrics.h"
#include "mozilla/dom/Document.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsDisplayList.h"
#include "nsRefreshDriver.h"
#include "nsStyleStructInlines.h"
#include "UnitTransforms.h"

// clang-format off
#define CLIP_LOG(...)
//#define CLIP_LOG(s_, ...) printf_stderr("CLIP(%s): " s_, __func__, ## __VA_ARGS__)
//#define CLIP_LOG(s_, ...) if (XRE_IsContentProcess()) printf_stderr("CLIP(%s): " s_, __func__, ## __VA_ARGS__)
// clang-format on

namespace mozilla {
namespace layers {

ClipManager::ClipManager() : mManager(nullptr), mBuilder(nullptr) {}

void ClipManager::BeginBuild(WebRenderLayerManager* aManager,
                             wr::DisplayListBuilder& aBuilder) {
  MOZ_ASSERT(!mManager);
  mManager = aManager;
  MOZ_ASSERT(!mBuilder);
  mBuilder = &aBuilder;
  MOZ_ASSERT(mCacheStack.empty());
  mCacheStack.emplace();
  MOZ_ASSERT(mASROverride.empty());
  MOZ_ASSERT(mItemClipStack.empty());
}

void ClipManager::EndBuild() {
  mBuilder = nullptr;
  mManager = nullptr;
  mCacheStack.pop();
  MOZ_ASSERT(mCacheStack.empty());
  MOZ_ASSERT(mASROverride.empty());
  MOZ_ASSERT(mItemClipStack.empty());
}

void ClipManager::BeginList(const StackingContextHelper& aStackingContext) {
  CLIP_LOG("begin list %p affects = %d, ref-frame = %d\n", &aStackingContext,
           aStackingContext.AffectsClipPositioning(),
           aStackingContext.ReferenceFrameId().isSome());

  ItemClips clips(nullptr, nullptr, false);
  if (!mItemClipStack.empty()) {
    clips = mItemClipStack.top();
  }

  if (aStackingContext.AffectsClipPositioning()) {
    if (auto referenceFrameId = aStackingContext.ReferenceFrameId()) {
      PushOverrideForASR(clips.mASR, *referenceFrameId);
      clips.mScrollId = *referenceFrameId;
    } else {
      // Start a new cache
      mCacheStack.emplace();
    }
  }

  CLIP_LOG("  push: clip: %p, asr: %p, scroll = %zu, clip = %zu\n",
           clips.mChain, clips.mASR, clips.mScrollId.id,
           clips.mClipChainId.valueOr(wr::WrClipChainId{0}).id);

  mItemClipStack.push(clips);
}

void ClipManager::EndList(const StackingContextHelper& aStackingContext) {
  MOZ_ASSERT(!mItemClipStack.empty());

  CLIP_LOG("end list %p\n", &aStackingContext);

  mBuilder->SetClipChainLeaf(Nothing());
  mItemClipStack.pop();

  if (aStackingContext.AffectsClipPositioning()) {
    if (aStackingContext.ReferenceFrameId()) {
      PopOverrideForASR(mItemClipStack.empty() ? nullptr
                                               : mItemClipStack.top().mASR);
    } else {
      MOZ_ASSERT(!mCacheStack.empty());
      mCacheStack.pop();
    }
  }
}

void ClipManager::PushOverrideForASR(const ActiveScrolledRoot* aASR,
                                     const wr::WrSpatialId& aSpatialId) {
  Maybe<wr::WrSpatialId> space = GetScrollLayer(aASR);
  MOZ_ASSERT(space.isSome());

  CLIP_LOG("Pushing %p override %zu -> %zu\n", aASR, space->id, aSpatialId.id);

  if (!mItemClipStack.empty()) {
    auto& top = mItemClipStack.top();
    if (top.mASR == aASR) {
      top.mScrollId = aSpatialId;
    }
  }

  auto it = mASROverride.insert({*space, std::stack<wr::WrSpatialId>()});
  it.first->second.push(aSpatialId);

  // Start a new cache
  mCacheStack.emplace();
}

void ClipManager::PopOverrideForASR(const ActiveScrolledRoot* aASR) {
  MOZ_ASSERT(!mCacheStack.empty());
  mCacheStack.pop();

  Maybe<wr::WrSpatialId> space = GetScrollLayer(aASR);
  MOZ_ASSERT(space.isSome());

  auto it = mASROverride.find(*space);
  CLIP_LOG("Popping %p override %zu -> %s\n", aASR, space->id,
           ToString(it->second.top().id).c_str());

  it->second.pop();

  if (!mItemClipStack.empty()) {
    auto& top = mItemClipStack.top();
    if (top.mASR == aASR) {
      top.mScrollId = it->second.empty() ? *space : it->second.top();
    }
  }

  if (it->second.empty()) {
    mASROverride.erase(it);
  }
}

wr::WrSpatialId ClipManager::SpatialIdAfterOverride(
    const wr::WrSpatialId& aSpatialId) {
  auto it = mASROverride.find(aSpatialId);
  if (it == mASROverride.end()) {
    return aSpatialId;
  }
  MOZ_ASSERT(!it->second.empty());
  CLIP_LOG("Overriding %zu with %s\n", aSpatialId.id,
           ToString(it->second.top().id).c_str());

  return it->second.top();
}

wr::WrSpaceAndClipChain ClipManager::SwitchItem(nsDisplayListBuilder* aBuilder,
                                                nsDisplayItem* aItem) {
  const DisplayItemClipChain* clip = aItem->GetClipChain();
  const DisplayItemClipChain* inheritedClipChain =
      mBuilder->GetInheritedClipChain();
  if (inheritedClipChain && inheritedClipChain != clip) {
    if (!clip) {
      clip = mBuilder->GetInheritedClipChain();
    } else {
      clip = aBuilder->CreateClipChainIntersection(
          mBuilder->GetInheritedClipChain(), clip);
    }
  }
  const ActiveScrolledRoot* asr = aItem->GetActiveScrolledRoot();
  DisplayItemType type = aItem->GetType();
  if (type == DisplayItemType::TYPE_STICKY_POSITION) {
    // For sticky position items, the ASR is computed differently depending on
    // whether the item has a fixed descendant or not. But for WebRender
    // purposes we always want to use the ASR that would have been used if it
    // didn't have fixed descendants, which is stored as the "container ASR" on
    // the sticky item.
    auto* sticky = static_cast<nsDisplayStickyPosition*>(aItem);
    asr = sticky->GetContainerASR();

    // If the leafmost clip for the sticky item is just the displayport clip,
    // then skip it. This allows sticky items to remain visible even if the
    // rest of the content in the enclosing scrollframe is checkerboarding.
    if (sticky->IsClippedToDisplayPort() && clip && clip->mASR == asr) {
      clip = clip->mParent;
    }
  }

  CLIP_LOG("processing item %p (%s) asr %p clip %p, inherited = %p\n", aItem,
           DisplayItemTypeName(aItem->GetType()), asr, clip,
           inheritedClipChain);

  // In most cases we can combine the leaf of the clip chain with the clip rect
  // of the display item. This reduces the number of clip items, which avoids
  // some overhead further down the pipeline.
  bool separateLeaf = false;
  if (clip && clip->mASR == asr && clip->mClip.GetRoundedRectCount() == 0) {
    // Container display items are not currently supported because the clip
    // rect of a stacking context is not handled the same as normal display
    // items.
    separateLeaf = !aItem->GetChildren();
  }

  ItemClips clips(asr, clip, separateLeaf);
  MOZ_ASSERT(!mItemClipStack.empty());
  if (clips.HasSameInputs(mItemClipStack.top())) {
    // Early-exit because if the clips are the same as aItem's previous sibling,
    // then we don't need to do do the work of popping the old stuff and then
    // pushing it right back on for the new item. Note that if aItem doesn't
    // have a previous sibling, that means BeginList would have been called
    // just before this, which will have pushed a ItemClips(nullptr, nullptr)
    // onto mItemClipStack, so the HasSameInputs check should return false.
    CLIP_LOG("\tearly-exit for %p\n", aItem);
    return mItemClipStack.top().GetSpaceAndClipChain();
  }

  // Pop aItem's previous sibling's stuff from mBuilder in preparation for
  // pushing aItem's stuff.
  mItemClipStack.pop();

  // Zoom display items report their bounds etc using the parent document's
  // APD because zoom items act as a conversion layer between the two different
  // APDs.
  int32_t auPerDevPixel;
  if (type == DisplayItemType::TYPE_ZOOM) {
    auPerDevPixel =
        static_cast<nsDisplayZoom*>(aItem)->GetParentAppUnitsPerDevPixel();
  } else {
    auPerDevPixel = aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
  }

  // If the leaf of the clip chain is going to be merged with the display item's
  // clip rect, then we should create a clip chain id from the leaf's parent.
  if (separateLeaf) {
    CLIP_LOG("\tseparate leaf detected, ignoring the last clip\n");
    clip = clip->mParent;
  }

  // There are two ASR chains here that we need to be fully defined. One is the
  // ASR chain pointed to by |asr|. The other is the
  // ASR chain pointed to by clip->mASR. We pick the leafmost
  // of these two chains because that one will include the other. Calling
  // DefineScrollLayers with this leafmost ASR will recursively define all the
  // ASRs that we care about for this item, but will not actually push
  // anything onto the WR stack.
  const ActiveScrolledRoot* leafmostASR = asr;
  if (clip) {
    leafmostASR = ActiveScrolledRoot::PickDescendant(leafmostASR, clip->mASR);
  }
  Maybe<wr::WrSpatialId> leafmostId = DefineScrollLayers(leafmostASR, aItem);
  Unused << leafmostId;

  // Define all the clips in the item's clip chain, and obtain a clip chain id
  // for it.
  clips.mClipChainId = DefineClipChain(clip, auPerDevPixel);

  Maybe<wr::WrSpatialId> space = GetScrollLayer(asr);
  MOZ_ASSERT(space.isSome());
  clips.mScrollId = SpatialIdAfterOverride(*space);
  CLIP_LOG("\tassigning %d -> %d\n", (int)space->id, (int)clips.mScrollId.id);

  // Now that we have the scroll id and a clip id for the item, push it onto
  // the WR stack.
  clips.UpdateSeparateLeaf(*mBuilder, auPerDevPixel);
  auto spaceAndClipChain = clips.GetSpaceAndClipChain();

  CLIP_LOG("  push: clip: %p, asr: %p, scroll = %zu, clip = %zu\n",
           clips.mChain, clips.mASR, clips.mScrollId.id,
           clips.mClipChainId.valueOr(wr::WrClipChainId{0}).id);

  mItemClipStack.push(clips);

  CLIP_LOG("done setup for %p\n", aItem);
  return spaceAndClipChain;
}

Maybe<wr::WrSpatialId> ClipManager::GetScrollLayer(
    const ActiveScrolledRoot* aASR) {
  for (const ActiveScrolledRoot* asr = aASR; asr; asr = asr->mParent) {
    Maybe<wr::WrSpatialId> space =
        mBuilder->GetScrollIdForDefinedScrollLayer(asr->GetViewId());
    if (space) {
      return space;
    }

    // If this ASR doesn't have a scroll ID, then we should check its ancestor.
    // There may not be one defined because the ASR may not be scrollable or we
    // failed to get the scroll metadata.
  }

  Maybe<wr::WrSpatialId> space = mBuilder->GetScrollIdForDefinedScrollLayer(
      ScrollableLayerGuid::NULL_SCROLL_ID);
  MOZ_ASSERT(space.isSome());
  return space;
}

Maybe<wr::WrSpatialId> ClipManager::DefineScrollLayers(
    const ActiveScrolledRoot* aASR, nsDisplayItem* aItem) {
  if (!aASR) {
    // Recursion base case
    return Nothing();
  }
  ScrollableLayerGuid::ViewID viewId = aASR->GetViewId();
  Maybe<wr::WrSpatialId> space =
      mBuilder->GetScrollIdForDefinedScrollLayer(viewId);
  if (space) {
    // If we've already defined this scroll layer before, we can early-exit
    return space;
  }
  // Recurse to define the ancestors
  Maybe<wr::WrSpatialId> ancestorSpace =
      DefineScrollLayers(aASR->mParent, aItem);

  Maybe<ScrollMetadata> metadata =
      aASR->mScrollableFrame->ComputeScrollMetadata(mManager, aItem->Frame(),
                                                    aItem->ToReferenceFrame());
  if (!metadata) {
    MOZ_ASSERT_UNREACHABLE("Expected scroll metadata to be available!");
    return ancestorSpace;
  }

  FrameMetrics& metrics = metadata->GetMetrics();
  if (!metrics.IsScrollable()) {
    // This item is a scrolling no-op, skip over it in the ASR chain.
    return ancestorSpace;
  }

  nsIScrollableFrame* scrollableFrame = aASR->mScrollableFrame;
  nsIFrame* scrollFrame = do_QueryFrame(scrollableFrame);
  nsPoint offset = scrollFrame->GetOffsetToCrossDoc(aItem->Frame()) +
                   aItem->ToReferenceFrame();
  float auPerDevPixel = aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
  nsRect scrollPort = scrollableFrame->GetScrollPortRect() + offset;
  LayoutDeviceRect clipBounds =
      LayoutDeviceRect::FromAppUnits(scrollPort, auPerDevPixel);

  // The content rect that we hand to PushScrollLayer should be relative to
  // the same origin as the clipBounds that we hand to PushScrollLayer -
  // that is, both of them should be relative to the stacking context `aSc`.
  // However, when we get the scrollable rect from the FrameMetrics, the
  // origin has nothing to do with the position of the frame but instead
  // represents the minimum allowed scroll offset of the scrollable content.
  // While APZ uses this to clamp the scroll position, we don't need to send
  // this to WebRender at all. Instead, we take the position from the
  // composition bounds.
  LayoutDeviceRect contentRect =
      metrics.GetExpandedScrollableRect() * metrics.GetDevPixelsPerCSSPixel();
  contentRect.MoveTo(clipBounds.TopLeft());

  Maybe<wr::WrSpatialId> parent = ancestorSpace;
  if (parent) {
    *parent = SpatialIdAfterOverride(*parent);
  }
  // The external scroll offset is accumulated into the local space positions of
  // display items inside WR, so that the elements hash (intern) to the same
  // content ID for quick comparisons. To avoid invalidations when the
  // auPerDevPixel is not a round value, round here directly from app units.
  // This guarantees we won't introduce any inaccuracy in the external scroll
  // offset passed to WR.
  LayoutDevicePoint scrollOffset = LayoutDevicePoint::FromAppUnitsRounded(
      scrollableFrame->GetScrollPosition(), auPerDevPixel);

  // Currently we track scroll-linked effects at the granularity of documents,
  // not scroll frames, so we consider a scroll frame to have a scroll-linked
  // effect whenever its containing document does.
  nsPresContext* presContext = aItem->Frame()->PresContext();
  const bool hasScrollLinkedEffect =
      presContext->Document()->HasScrollLinkedEffect();

  return Some(mBuilder->DefineScrollLayer(
      viewId, parent, wr::ToLayoutRect(contentRect),
      wr::ToLayoutRect(clipBounds), wr::ToLayoutVector2D(scrollOffset),
      wr::ToWrAPZScrollGeneration(scrollableFrame->ScrollGenerationOnApz()),
      wr::ToWrHasScrollLinkedEffect(hasScrollLinkedEffect),
      wr::SpatialKey(uint64_t(scrollFrame), 0, wr::SpatialKeyKind::Scroll)));
}

Maybe<wr::WrClipChainId> ClipManager::DefineClipChain(
    const DisplayItemClipChain* aChain, int32_t aAppUnitsPerDevPixel) {
  AutoTArray<wr::WrClipId, 6> allClipIds;
  // Iterate through the clips in the current item's clip chain, define them
  // in WR, and put their IDs into |clipIds|.
  for (const DisplayItemClipChain* chain = aChain; chain;
       chain = chain->mParent) {
    ClipIdMap& cache = mCacheStack.top();
    auto it = cache.find(chain);
    if (it != cache.end()) {
      // Found it in the currently-active cache, so just use the id we have for
      // it.
      CLIP_LOG("cache[%p] => hit\n", chain);
      allClipIds.AppendElements(it->second);
      continue;
    }
    if (!chain->mClip.HasClip()) {
      // This item in the chain is a no-op, skip over it
      continue;
    }

    LayoutDeviceRect clip = LayoutDeviceRect::FromAppUnits(
        chain->mClip.GetClipRect(), aAppUnitsPerDevPixel);
    AutoTArray<wr::ComplexClipRegion, 6> wrRoundedRects;
    chain->mClip.ToComplexClipRegions(aAppUnitsPerDevPixel, wrRoundedRects);

    Maybe<wr::WrSpatialId> space = GetScrollLayer(chain->mASR);
    // Before calling DefineClipChain we defined the ASRs by calling
    // DefineScrollLayers, so we must have a scrollId here.
    MOZ_ASSERT(space.isSome());

    // Define the clip
    *space = SpatialIdAfterOverride(*space);

    AutoTArray<wr::WrClipId, 4> chainClipIds;

    auto rectClipId = mBuilder->DefineRectClip(space, wr::ToLayoutRect(clip));
    CLIP_LOG("cache[%p] <= %zu\n", chain, rectClipId.id);
    chainClipIds.AppendElement(rectClipId);

    for (const auto& complexClip : wrRoundedRects) {
      auto complexClipId = mBuilder->DefineRoundedRectClip(space, complexClip);
      CLIP_LOG("cache[%p] <= %zu\n", chain, complexClipId.id);
      chainClipIds.AppendElement(complexClipId);
    }

    cache[chain] = chainClipIds.Clone();
    allClipIds.AppendElements(chainClipIds);
  }

  if (allClipIds.IsEmpty()) {
    return Nothing();
  }

  return Some(mBuilder->DefineClipChain(allClipIds));
}

ClipManager::~ClipManager() {
  MOZ_ASSERT(!mBuilder);
  MOZ_ASSERT(mCacheStack.empty());
  MOZ_ASSERT(mItemClipStack.empty());
}

ClipManager::ItemClips::ItemClips(const ActiveScrolledRoot* aASR,
                                  const DisplayItemClipChain* aChain,
                                  bool aSeparateLeaf)
    : mASR(aASR), mChain(aChain), mSeparateLeaf(aSeparateLeaf) {
  mScrollId = wr::wr_root_scroll_node_id();
}

void ClipManager::ItemClips::UpdateSeparateLeaf(
    wr::DisplayListBuilder& aBuilder, int32_t aAppUnitsPerDevPixel) {
  Maybe<wr::LayoutRect> clipLeaf;
  if (mSeparateLeaf) {
    MOZ_ASSERT(mChain);
    clipLeaf.emplace(wr::ToLayoutRect(LayoutDeviceRect::FromAppUnits(
        mChain->mClip.GetClipRect(), aAppUnitsPerDevPixel)));
  }

  aBuilder.SetClipChainLeaf(clipLeaf);
}

bool ClipManager::ItemClips::HasSameInputs(const ItemClips& aOther) {
  return mASR == aOther.mASR && mChain == aOther.mChain &&
         mSeparateLeaf == aOther.mSeparateLeaf;
}

wr::WrSpaceAndClipChain ClipManager::ItemClips::GetSpaceAndClipChain() const {
  auto spaceAndClipChain = wr::RootScrollNodeWithChain();
  spaceAndClipChain.space = mScrollId;
  if (mClipChainId) {
    spaceAndClipChain.clip_chain = mClipChainId->id;
  }
  return spaceAndClipChain;
}

}  // namespace layers
}  // namespace mozilla
