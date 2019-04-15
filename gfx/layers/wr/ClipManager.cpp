/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ClipManager.h"

#include "DisplayItemClipChain.h"
#include "FrameMetrics.h"
#include "LayersLogging.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsDisplayList.h"
#include "nsStyleStructInlines.h"
#include "UnitTransforms.h"

// clang-format off
#define CLIP_LOG(...)
//#define CLIP_LOG(...) printf_stderr("CLIP: " __VA_ARGS__)
//#define CLIP_LOG(...) if (XRE_IsContentProcess()) printf_stderr("CLIP: " __VA_ARGS__)
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
  if (aStackingContext.AffectsClipPositioning()) {
    if (aStackingContext.ReferenceFrameId()) {
      PushOverrideForASR(
          mItemClipStack.empty() ? nullptr : mItemClipStack.top().mASR,
          aStackingContext.ReferenceFrameId().ref());
    } else {
      // Start a new cache
      mCacheStack.emplace();
    }
  }

  ItemClips clips(nullptr, nullptr, false);
  if (!mItemClipStack.empty()) {
    clips.CopyOutputsFrom(mItemClipStack.top());
  }

  if (aStackingContext.ReferenceFrameId()) {
    clips.mScrollId = aStackingContext.ReferenceFrameId().ref();
  }

  mItemClipStack.push(clips);
}

void ClipManager::EndList(const StackingContextHelper& aStackingContext) {
  MOZ_ASSERT(!mItemClipStack.empty());
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
  Maybe<wr::WrSpaceAndClip> spaceAndClip = GetScrollLayer(aASR);
  MOZ_ASSERT(spaceAndClip.isSome());

  CLIP_LOG("Pushing %p override %zu -> %s\n", aASR, spaceAndClip->space.id,
           Stringify(aSpatialId.id).c_str());

  auto it =
      mASROverride.insert({spaceAndClip->space, std::stack<wr::WrSpatialId>()});
  it.first->second.push(aSpatialId);

  // Start a new cache
  mCacheStack.emplace();
}

void ClipManager::PopOverrideForASR(const ActiveScrolledRoot* aASR) {
  MOZ_ASSERT(!mCacheStack.empty());
  mCacheStack.pop();

  Maybe<wr::WrSpaceAndClip> spaceAndClip = GetScrollLayer(aASR);
  MOZ_ASSERT(spaceAndClip.isSome());

  auto it = mASROverride.find(spaceAndClip->space);
  CLIP_LOG("Popping %p override %zu -> %s\n", aASR, spaceAndClip->space.id,
           Stringify(it->second.top().id).c_str());

  it->second.pop();
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
           Stringify(it->second.top().id).c_str());

  return it->second.top();
}

wr::WrSpaceAndClipChain ClipManager::SwitchItem(nsDisplayItem* aItem) {
  const DisplayItemClipChain* clip = aItem->GetClipChain();
  const ActiveScrolledRoot* asr = aItem->GetActiveScrolledRoot();
  CLIP_LOG("processing item %p (%s) asr %p\n", aItem,
           DisplayItemTypeName(aItem->GetType()), asr);

  DisplayItemType type = aItem->GetType();
  if (type == DisplayItemType::TYPE_STICKY_POSITION) {
    // For sticky position items, the ASR is computed differently depending
    // on whether the item has a fixed descendant or not. But for WebRender
    // purposes we always want to use the ASR that would have been used if it
    // didn't have fixed descendants, which is stored as the "container ASR" on
    // the sticky item.
    asr = static_cast<nsDisplayStickyPosition*>(aItem)->GetContainerASR();
  }

  // In most cases we can combine the leaf of the clip chain with the clip rect
  // of the display item. This reduces the number of clip items, which avoids
  // some overhead further down the pipeline.
  bool separateLeaf = false;
  if (clip && clip->mASR == asr && clip->mClip.GetRoundedRectCount() == 0) {
    // Container display items are not currently supported because the clip
    // rect of a stacking context is not handled the same as normal display
    // items.
    separateLeaf = aItem->GetChildren() == nullptr;
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
  Maybe<wr::WrSpaceAndClip> leafmostId = DefineScrollLayers(leafmostASR, aItem);

  // Define all the clips in the item's clip chain, and obtain a clip chain id
  // for it.
  clips.mClipChainId = DefineClipChain(clip, auPerDevPixel);

  Maybe<wr::WrSpaceAndClip> spaceAndClip = GetScrollLayer(asr);
  MOZ_ASSERT(spaceAndClip.isSome());
  clips.mScrollId = SpatialIdAfterOverride(spaceAndClip->space);
  CLIP_LOG("\tassigning %d -> %d\n", (int)spaceAndClip->space.id,
           (int)clips.mScrollId.id);

  // Now that we have the scroll id and a clip id for the item, push it onto
  // the WR stack.
  clips.UpdateSeparateLeaf(*mBuilder, auPerDevPixel);
  auto spaceAndClipChain = clips.GetSpaceAndClipChain();
  mItemClipStack.push(clips);

  CLIP_LOG("done setup for %p\n", aItem);
  return spaceAndClipChain;
}

Maybe<wr::WrSpaceAndClip> ClipManager::GetScrollLayer(
    const ActiveScrolledRoot* aASR) {
  for (const ActiveScrolledRoot* asr = aASR; asr; asr = asr->mParent) {
    Maybe<wr::WrSpaceAndClip> spaceAndClip =
        mBuilder->GetScrollIdForDefinedScrollLayer(asr->GetViewId());
    if (spaceAndClip) {
      return spaceAndClip;
    }

    // If this ASR doesn't have a scroll ID, then we should check its ancestor.
    // There may not be one defined because the ASR may not be scrollable or we
    // failed to get the scroll metadata.
  }

  Maybe<wr::WrSpaceAndClip> spaceAndClip =
      mBuilder->GetScrollIdForDefinedScrollLayer(
          ScrollableLayerGuid::NULL_SCROLL_ID);
  MOZ_ASSERT(spaceAndClip.isSome());
  return spaceAndClip;
}

Maybe<wr::WrSpaceAndClip> ClipManager::DefineScrollLayers(
    const ActiveScrolledRoot* aASR, nsDisplayItem* aItem) {
  if (!aASR) {
    // Recursion base case
    return Nothing();
  }
  ScrollableLayerGuid::ViewID viewId = aASR->GetViewId();
  Maybe<wr::WrSpaceAndClip> spaceAndClip =
      mBuilder->GetScrollIdForDefinedScrollLayer(viewId);
  if (spaceAndClip) {
    // If we've already defined this scroll layer before, we can early-exit
    return spaceAndClip;
  }
  // Recurse to define the ancestors
  Maybe<wr::WrSpaceAndClip> ancestorSpaceAndClip =
      DefineScrollLayers(aASR->mParent, aItem);

  Maybe<ScrollMetadata> metadata =
      aASR->mScrollableFrame->ComputeScrollMetadata(
          mManager, aItem->ReferenceFrame(), Nothing(), nullptr);
  if (!metadata) {
    MOZ_ASSERT_UNREACHABLE("Expected scroll metadata to be available!");
    return ancestorSpaceAndClip;
  }

  FrameMetrics& metrics = metadata->GetMetrics();
  if (!metrics.IsScrollable()) {
    // This item is a scrolling no-op, skip over it in the ASR chain.
    return ancestorSpaceAndClip;
  }

  LayoutDeviceRect contentRect =
      metrics.GetExpandedScrollableRect() * metrics.GetDevPixelsPerCSSPixel();
  LayoutDeviceRect clipBounds = LayoutDeviceRect::FromUnknownRect(
      metrics.GetCompositionBounds().ToUnknownRect());
  // The content rect that we hand to PushScrollLayer should be relative to
  // the same origin as the clipBounds that we hand to PushScrollLayer - that
  // is, both of them should be relative to the stacking context `aSc`.
  // However, when we get the scrollable rect from the FrameMetrics, the origin
  // has nothing to do with the position of the frame but instead represents
  // the minimum allowed scroll offset of the scrollable content. While APZ
  // uses this to clamp the scroll position, we don't need to send this to
  // WebRender at all. Instead, we take the position from the composition
  // bounds.
  contentRect.MoveTo(clipBounds.TopLeft());

  Maybe<wr::WrSpaceAndClip> parent = ancestorSpaceAndClip;
  if (parent) {
    parent->space = SpatialIdAfterOverride(parent->space);
  }
  LayoutDevicePoint scrollOffset =
      metrics.GetScrollOffset() * metrics.GetDevPixelsPerCSSPixel();
  return Some(mBuilder->DefineScrollLayer(
      viewId, parent, wr::ToRoundedLayoutRect(contentRect),
      wr::ToRoundedLayoutRect(clipBounds), wr::ToLayoutPoint(scrollOffset)));
}

Maybe<wr::WrClipChainId> ClipManager::DefineClipChain(
    const DisplayItemClipChain* aChain, int32_t aAppUnitsPerDevPixel) {
  AutoTArray<wr::WrClipId, 6> clipIds;
  // Iterate through the clips in the current item's clip chain, define them
  // in WR, and put their IDs into |clipIds|.
  for (const DisplayItemClipChain* chain = aChain; chain;
       chain = chain->mParent) {
    ClipIdMap& cache = mCacheStack.top();
    auto it = cache.find(chain);
    if (it != cache.end()) {
      // Found it in the currently-active cache, so just use the id we have for
      // it.
      CLIP_LOG("cache[%p] => %zu\n", chain, it->second.id);
      clipIds.AppendElement(it->second);
      continue;
    }
    if (!chain->mClip.HasClip()) {
      // This item in the chain is a no-op, skip over it
      continue;
    }

    LayoutDeviceRect clip = LayoutDeviceRect::FromAppUnits(
        chain->mClip.GetClipRect(), aAppUnitsPerDevPixel);
    nsTArray<wr::ComplexClipRegion> wrRoundedRects;
    chain->mClip.ToComplexClipRegions(aAppUnitsPerDevPixel, wrRoundedRects);

    Maybe<wr::WrSpaceAndClip> spaceAndClip = GetScrollLayer(chain->mASR);
    // Before calling DefineClipChain we defined the ASRs by calling
    // DefineScrollLayers, so we must have a scrollId here.
    MOZ_ASSERT(spaceAndClip.isSome());

    // Define the clip
    spaceAndClip->space = SpatialIdAfterOverride(spaceAndClip->space);
    wr::WrClipId clipId = mBuilder->DefineClip(
        spaceAndClip, wr::ToRoundedLayoutRect(clip), &wrRoundedRects);
    clipIds.AppendElement(clipId);
    cache[chain] = clipId;
    CLIP_LOG("cache[%p] <= %zu\n", chain, clipId.id);
  }

  if (clipIds.IsEmpty()) {
    return Nothing();
  }

  return Some(mBuilder->DefineClipChain(clipIds));
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
    clipLeaf.emplace(wr::ToRoundedLayoutRect(LayoutDeviceRect::FromAppUnits(
        mChain->mClip.GetClipRect(), aAppUnitsPerDevPixel)));
  }

  aBuilder.SetClipChainLeaf(clipLeaf);
}

bool ClipManager::ItemClips::HasSameInputs(const ItemClips& aOther) {
  return mASR == aOther.mASR && mChain == aOther.mChain &&
         mSeparateLeaf == aOther.mSeparateLeaf;
}

void ClipManager::ItemClips::CopyOutputsFrom(const ItemClips& aOther) {
  mScrollId = aOther.mScrollId;
  mClipChainId = aOther.mClipChainId;
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
