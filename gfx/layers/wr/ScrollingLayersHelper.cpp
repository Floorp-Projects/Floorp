/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ScrollingLayersHelper.h"

#include "FrameMetrics.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "UnitTransforms.h"

namespace mozilla {
namespace layers {

ScrollingLayersHelper::ScrollingLayersHelper(nsDisplayItem* aItem,
                                             wr::DisplayListBuilder& aBuilder,
                                             const StackingContextHelper& aStackingContext,
                                             WebRenderCommandBuilder::ClipIdMap& aCache,
                                             bool aApzEnabled)
  : mBuilder(&aBuilder)
  , mPushedClipAndScroll(false)
{
  int32_t auPerDevPixel = aItem->Frame()->PresContext()->AppUnitsPerDevPixel();

  if (!aApzEnabled) {
    // If APZ is not enabled, we can ignore all the stuff with ASRs; we just
    // need to define the clip chain on the item and that's it.
    DefineAndPushChain(aItem->GetClipChain(), aBuilder, aStackingContext,
        auPerDevPixel, aCache);
    return;
  }

  // There are two ASR chains here that we need to be fully defined. One is the
  // ASR chain pointed to by aItem->GetActiveScrolledRoot(). The other is the
  // ASR chain pointed to by aItem->GetClipChain()->mASR. We pick the leafmost
  // of these two chains because that one will include the other. And then we
  // call DefineAndPushScrollLayers with it, which will recursively push all
  // the necessary clips and scroll layer items for that ASR chain.
  const ActiveScrolledRoot* leafmostASR = aItem->GetActiveScrolledRoot();
  if (aItem->GetClipChain()) {
    leafmostASR = ActiveScrolledRoot::PickDescendant(leafmostASR,
        aItem->GetClipChain()->mASR);
  }
  DefineAndPushScrollLayers(aItem, leafmostASR,
      aItem->GetClipChain(), aBuilder, auPerDevPixel, aStackingContext, aCache);

  // Next, we push the leaf part of the clip chain that is scrolled by the
  // leafmost ASR. All the clips outside the leafmost ASR were already pushed
  // in the above call. This call may be a no-op if the item's ASR got picked
  // as the leaftmostASR previously, because that means these clips were pushed
  // already as being "outside" leafmostASR.
  DefineAndPushChain(aItem->GetClipChain(), aBuilder, aStackingContext,
      auPerDevPixel, aCache);

  // Finally, if clip chain's ASR was the leafmost ASR, then the top of the
  // scroll id stack right now will point to that, rather than the item's ASR
  // which is what we want. So we override that by doing a PushClipAndScrollInfo
  // call. This should generally only happen for fixed-pos type items, but we
  // use code generic enough to handle other cases.
  FrameMetrics::ViewID scrollId = aItem->GetActiveScrolledRoot()
      ? nsLayoutUtils::ViewIDForASR(aItem->GetActiveScrolledRoot())
      : FrameMetrics::NULL_SCROLL_ID;
  if (aBuilder.TopmostScrollId() != scrollId) {
    Maybe<wr::WrClipId> clipId = mBuilder->TopmostClipId();
    mBuilder->PushClipAndScrollInfo(scrollId, clipId.ptrOr(nullptr));
    mPushedClipAndScroll = true;
  }
}

std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>>
ScrollingLayersHelper::DefineClipChain(nsDisplayItem* aItem,
                                       const ActiveScrolledRoot* aAsr,
                                       const DisplayItemClipChain* aChain,
                                       int32_t aAppUnitsPerDevPixel,
                                       const StackingContextHelper& aStackingContext,
                                       WebRenderCommandBuilder::ClipIdMap& aCache)
{
  // This is the main entry point for defining the clip chain for a display
  // item. This function recursively walks up the ASR chain and the display
  // item's clip chain to define all the ASRs and clips necessary. Each level
  // of the recursion defines one item, if it hasn't been defined already.
  // The |aAsr| and |aChain| parameters are the important ones to track during
  // the recursion; the rest of the parameters don't change (although |aCache|
  // might be updated with new things).
  // At each level of the recursion, the return value is the pair of identifiers
  // that correspond to aAsr and aChain, respectively.

  // These are the possible cases when recursing:
  //
  // aAsr is null, aChain is null     => base case; return
  // aAsr is non-null, aChain is null => recurse(aAsr->mParent, null),
  //                                     then define aAsr
  // aAsr is null, aChain is non-null => assert(aChain->mASR == null),
  //                                     recurse(null, aChain->mParent),
  //                                     then define aChain
  // aChain->mASR == aAsr             => recurse(aAsr, aChain->mParent),
  //                                     then define aChain
  // aChain->mASR != aAsr             => recurse(aAsr->mParent, aChain),
  //                                     then define aAsr
  //
  // These can basically be collapsed down into two codepaths; one that recurses
  // on the ASR chain and one that recurses on the clip chain; that's what the
  // code below does.

  // in all of these cases, this invariant should hold:
  //   PickDescendant(aChain->mASR, aAsr) == aAsr
  MOZ_ASSERT(!aChain || ActiveScrolledRoot::PickDescendant(aChain->mASR, aAsr) == aAsr);

  if (aChain && aChain->mASR == aAsr) {
    return RecurseAndDefineClip(aItem, aAsr, aChain, aAppUnitsPerDevPixel, aStackingContext, aCache);
  }
  if (aAsr) {
    return RecurseAndDefineAsr(aItem, aAsr, aChain, aAppUnitsPerDevPixel, aStackingContext, aCache);
  }

  MOZ_ASSERT(!aChain && !aAsr);

  return std::make_pair(Nothing(), Nothing());
}

std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>>
ScrollingLayersHelper::RecurseAndDefineClip(nsDisplayItem* aItem,
                                            const ActiveScrolledRoot* aAsr,
                                            const DisplayItemClipChain* aChain,
                                            int32_t aAppUnitsPerDevPixel,
                                            const StackingContextHelper& aSc,
                                            WebRenderCommandBuilder::ClipIdMap& aCache)
{
  MOZ_ASSERT(aChain);

  // This will hold our return value
  std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>> ids;

  if (mBuilder->HasExtraClip()) {
    // We can't use aCache directly. However if there's an out-of-band clip that
    // was pushed on top of aChain, we should return the id for that OOB clip,
    // so that anything we want to define as a descendant of aChain we actually
    // end up defining as a descendant of the OOB clip.
    ids.second = mBuilder->GetCacheOverride(aChain);
  } else {
    auto it = aCache.find(aChain);
    if (it != aCache.end()) {
      ids.second = Some(it->second);
    }
  }
  if (ids.second) {
    // If we've already got an id for this clip, we can early-exit
    if (aAsr) {
      FrameMetrics::ViewID scrollId = nsLayoutUtils::ViewIDForASR(aAsr);
      MOZ_ASSERT(mBuilder->IsScrollLayerDefined(scrollId));
      ids.first = Some(scrollId);
    }
    return ids;
  }

  // If not, recurse to ensure all the ancestors are defined
  auto ancestorIds = DefineClipChain(
      aItem, aAsr, aChain->mParent, aAppUnitsPerDevPixel, aSc, aCache);
  ids = ancestorIds;

  if (!aChain->mClip.HasClip()) {
    // This item in the chain is a no-op, skip over it
    return ids;
  }

  // Now we need to figure out whether the new clip we're defining should be
  // a child of aChain->mParent, or of aAsr.
  if (aChain->mParent) {
    if (mBuilder->GetCacheOverride(aChain->mParent)) {
      // If the parent clip had an override (i.e. the parent display item pushed
      // an out-of-band clip), then we definitely want to use that as the parent
      // because everything defined inside that clip should have it as an
      // ancestor.
      ancestorIds.first = Nothing();
    } else if (aChain->mParent->mASR == aAsr) {
      // If the parent clip item shares the ASR, then this clip needs to be
      // a child of the aChain->mParent, which will already be a descendant of
      // the ASR.
      ancestorIds.first = Nothing();
    } else {
      // But if the ASRs are different, this is the outermost clip that's
      // still inside aAsr, and we need to make it a child of aAsr rather
      // than aChain->mParent.
      ancestorIds.second = Nothing();
    }
  }
  // At most one of the ancestor pair should be defined here, and the one that
  // is defined will be the parent clip for the new clip that we're defining.
  MOZ_ASSERT(!(ancestorIds.first && ancestorIds.second));

  LayoutDeviceRect clip = LayoutDeviceRect::FromAppUnits(
      aChain->mClip.GetClipRect(), aAppUnitsPerDevPixel);
  nsTArray<wr::ComplexClipRegion> wrRoundedRects;
  aChain->mClip.ToComplexClipRegions(aAppUnitsPerDevPixel, aSc, wrRoundedRects);

  // Define the clip
  wr::WrClipId clipId = mBuilder->DefineClip(
      ancestorIds.first, ancestorIds.second,
      aSc.ToRelativeLayoutRect(clip), &wrRoundedRects);
  if (!mBuilder->HasExtraClip()) {
    aCache[aChain] = clipId;
  }

  ids.second = Some(clipId);
  return ids;
}

std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>>
ScrollingLayersHelper::RecurseAndDefineAsr(nsDisplayItem* aItem,
                                           const ActiveScrolledRoot* aAsr,
                                           const DisplayItemClipChain* aChain,
                                           int32_t aAppUnitsPerDevPixel,
                                           const StackingContextHelper& aSc,
                                           WebRenderCommandBuilder::ClipIdMap& aCache)
{
  MOZ_ASSERT(aAsr);

  // This will hold our return value
  std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>> ids;

  FrameMetrics::ViewID scrollId = nsLayoutUtils::ViewIDForASR(aAsr);
  if (mBuilder->IsScrollLayerDefined(scrollId)) {
    // If we've already defined this scroll layer before, we can early-exit
    ids.first = Some(scrollId);
    if (aChain) {
      if (mBuilder->HasExtraClip()) {
        ids.second = mBuilder->GetCacheOverride(aChain);
      } else {
        auto it = aCache.find(aChain);
        if (it == aCache.end()) {
          // Degenerate case, where there are two clip chain items that are
          // fundamentally the same but are different objects and so we can't
          // find it in the cache via hashing. Linear search for it instead.
          // XXX This shouldn't happen very often but it might still turn out
          // to be a performance cliff, so we should figure out a better way to
          // deal with this.
          for (it = aCache.begin(); it != aCache.end(); it++) {
            if (DisplayItemClipChain::Equal(aChain, it->first)) {
              break;
            }
          }
        }
        // If |it == aCache.end()| here then we have run into a case where the
        // scroll layer was previously defined a specific parent clip, and
        // now here it has a different parent clip. Gecko can create display
        // lists like this because it treats the ASR chain and clipping chain
        // more independently, but we can't yet represent this in WR. This is
        // tracked by bug 1409442. For now we'll just leave ids.second as
        // Nothing() which will effectively ignore the clip |aChain|. Once WR
        // supports multiple ancestors on a scroll layer we can deal with this
        // better. The layout/reftests/text/wordwrap-08.html has a Text display
        // item that exercises this case.
        if (it != aCache.end()) {
          ids.second = Some(it->second);
        }
      }
    }
    return ids;
  }

  // If not, recurse to ensure all the ancestors are defined
  auto ancestorIds = DefineClipChain(
      aItem, aAsr->mParent, aChain, aAppUnitsPerDevPixel, aSc,
      aCache);
  ids = ancestorIds;

  Maybe<ScrollMetadata> metadata = aAsr->mScrollableFrame->ComputeScrollMetadata(
      nullptr, aItem->ReferenceFrame(), ContainerLayerParameters(), nullptr);
  MOZ_ASSERT(metadata);
  FrameMetrics& metrics = metadata->GetMetrics();

  if (!metrics.IsScrollable()) {
    // This item in the chain is a no-op, skip over it
    return ids;
  }

  // Now we need to figure out whether the new clip we're defining should be
  // a child of aChain, or of aAsr->mParent, if we have both as a possibility.
  if (ancestorIds.first && ancestorIds.second) {
    MOZ_ASSERT(aAsr->mParent); // because ancestorIds.first
    MOZ_ASSERT(aChain); // because ancestorIds.second
    if (aChain->mASR && aChain->mASR == aAsr->mParent) {
      // aChain is scrolled by aAsr's parent, so we should use aChain as the
      // ancestor when defining the aAsr scroll layer.
      ancestorIds.first = Nothing();
    } else {
      // This scenario never seems to occur in practice, but if it did it would
      // mean that aChain is scrolled by one of aAsr's ancestors beyond the
      // parent, in which case we should use aAsr->mParent as the ancestor
      // when defining the aAsr scroll layer.
      ancestorIds.second = Nothing();
    }
  }
  // At most one of the ancestor pair should be defined here, and the one that
  // is defined will be the parent clip for the new scrollframe that we're
  // defining.
  MOZ_ASSERT(!(ancestorIds.first && ancestorIds.second));

  LayoutDeviceRect contentRect =
      metrics.GetExpandedScrollableRect() * metrics.GetDevPixelsPerCSSPixel();
  // TODO: check coordinate systems are sane here
  LayoutDeviceRect clipBounds =
      LayoutDeviceRect::FromUnknownRect(metrics.GetCompositionBounds().ToUnknownRect());
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

  mBuilder->DefineScrollLayer(scrollId, ancestorIds.first, ancestorIds.second,
      aSc.ToRelativeLayoutRect(contentRect),
      aSc.ToRelativeLayoutRect(clipBounds));

  ids.first = Some(scrollId);
  return ids;
}

void
ScrollingLayersHelper::DefineAndPushScrollLayers(nsDisplayItem* aItem,
                                                 const ActiveScrolledRoot* aAsr,
                                                 const DisplayItemClipChain* aChain,
                                                 wr::DisplayListBuilder& aBuilder,
                                                 int32_t aAppUnitsPerDevPixel,
                                                 const StackingContextHelper& aStackingContext,
                                                 WebRenderCommandBuilder::ClipIdMap& aCache)
{
  if (!aAsr) {
    return;
  }
  FrameMetrics::ViewID scrollId = nsLayoutUtils::ViewIDForASR(aAsr);
  if (aBuilder.TopmostScrollId() == scrollId) {
    // it's already been pushed, so we don't need to recurse any further.
    return;
  }

  // Find the first clip up the chain that's "outside" aAsr. Any clips
  // that are "inside" aAsr (i.e. that are scrolled by aAsr) will need to be
  // pushed onto the stack after aAsr has been pushed. On the recursive call
  // we need to skip up the clip chain past these clips.
  const DisplayItemClipChain* asrClippedBy = aChain;
  while (asrClippedBy &&
         ActiveScrolledRoot::PickAncestor(asrClippedBy->mASR, aAsr) == aAsr) {
    asrClippedBy = asrClippedBy->mParent;
  }

  // Recurse up the ASR chain to make sure all ancestor scroll layers and their
  // enclosing clips are defined and pushed onto the WR stack.
  DefineAndPushScrollLayers(aItem, aAsr->mParent, asrClippedBy, aBuilder,
      aAppUnitsPerDevPixel, aStackingContext, aCache);

  // Once the ancestors are dealt with, we want to make sure all the clips
  // enclosing aAsr are pushed. All the clips enclosing aAsr->mParent were
  // already taken care of in the recursive call, so DefineAndPushChain will
  // push exactly what we want.
  DefineAndPushChain(asrClippedBy, aBuilder, aStackingContext,
      aAppUnitsPerDevPixel, aCache);
  // Finally, push the ASR itself as a scroll layer. If it's already defined
  // we can skip the expensive step of computing the ScrollMetadata.
  bool pushed = false;
  if (mBuilder->IsScrollLayerDefined(scrollId)) {
    mBuilder->PushScrollLayer(scrollId);
    pushed = true;
  } else {
    Maybe<ScrollMetadata> metadata = aAsr->mScrollableFrame->ComputeScrollMetadata(
        nullptr, aItem->ReferenceFrame(), ContainerLayerParameters(), nullptr);
    MOZ_ASSERT(metadata);
    pushed = DefineAndPushScrollLayer(metadata->GetMetrics(), aStackingContext);
  }
  if (pushed) {
    mPushedClips.push_back(wr::ScrollOrClipId(scrollId));
  }
}

void
ScrollingLayersHelper::DefineAndPushChain(const DisplayItemClipChain* aChain,
                                          wr::DisplayListBuilder& aBuilder,
                                          const StackingContextHelper& aStackingContext,
                                          int32_t aAppUnitsPerDevPixel,
                                          WebRenderCommandBuilder::ClipIdMap& aCache)
{
  if (!aChain) {
    return;
  }
  auto it = aCache.find(aChain);
  Maybe<wr::WrClipId> clipId = (it != aCache.end() ? Some(it->second) : Nothing());
  if (clipId && clipId == aBuilder.TopmostClipId()) {
    // it was already in the cache and pushed on the WR clip stack, so we don't
    // need to recurse any further.
    return;
  }
  // Recurse up the clip chain to make sure all ancestor clips are defined and
  // pushed onto the WR clip stack. Note that the recursion can invalidate the
  // iterator `it`.
  DefineAndPushChain(aChain->mParent, aBuilder, aStackingContext, aAppUnitsPerDevPixel, aCache);

  if (!aChain->mClip.HasClip()) {
    // This item in the chain is a no-op, skip over it
    return;
  }
  if (!clipId || aBuilder.HasExtraClip()) {
    // If we don't have a clip id for this chain item yet, define the clip in WR
    // and save the id
    LayoutDeviceRect clip = LayoutDeviceRect::FromAppUnits(
        aChain->mClip.GetClipRect(), aAppUnitsPerDevPixel);
    nsTArray<wr::ComplexClipRegion> wrRoundedRects;
    aChain->mClip.ToComplexClipRegions(aAppUnitsPerDevPixel, aStackingContext, wrRoundedRects);
    clipId = Some(aBuilder.DefineClip(Nothing(), Nothing(), aStackingContext.ToRelativeLayoutRect(clip), &wrRoundedRects));
    if (!aBuilder.HasExtraClip()) {
      aCache[aChain] = clipId.value();
    }
  }
  // Finally, push the clip onto the WR stack
  MOZ_ASSERT(clipId);
  aBuilder.PushClip(clipId.value());
  mPushedClips.push_back(wr::ScrollOrClipId(clipId.value()));
}

bool
ScrollingLayersHelper::DefineAndPushScrollLayer(const FrameMetrics& aMetrics,
                                                const StackingContextHelper& aStackingContext)
{
  if (!aMetrics.IsScrollable()) {
    return false;
  }
  LayoutDeviceRect contentRect =
      aMetrics.GetExpandedScrollableRect() * aMetrics.GetDevPixelsPerCSSPixel();
  // TODO: check coordinate systems are sane here
  LayoutDeviceRect clipBounds =
      LayoutDeviceRect::FromUnknownRect(aMetrics.GetCompositionBounds().ToUnknownRect());
  // The content rect that we hand to PushScrollLayer should be relative to
  // the same origin as the clipBounds that we hand to PushScrollLayer - that
  // is, both of them should be relative to the stacking context `aStackingContext`.
  // However, when we get the scrollable rect from the FrameMetrics, the origin
  // has nothing to do with the position of the frame but instead represents
  // the minimum allowed scroll offset of the scrollable content. While APZ
  // uses this to clamp the scroll position, we don't need to send this to
  // WebRender at all. Instead, we take the position from the composition
  // bounds.
  contentRect.MoveTo(clipBounds.TopLeft());
  mBuilder->DefineScrollLayer(aMetrics.GetScrollId(), Nothing(), Nothing(),
      aStackingContext.ToRelativeLayoutRect(contentRect),
      aStackingContext.ToRelativeLayoutRect(clipBounds));
  mBuilder->PushScrollLayer(aMetrics.GetScrollId());
  return true;
}

ScrollingLayersHelper::~ScrollingLayersHelper()
{
  if (mPushedClipAndScroll) {
    mBuilder->PopClipAndScrollInfo();
  }
  while (!mPushedClips.empty()) {
    wr::ScrollOrClipId id = mPushedClips.back();
    if (id.is<wr::WrClipId>()) {
      mBuilder->PopClip();
    } else {
      MOZ_ASSERT(id.is<FrameMetrics::ViewID>());
      mBuilder->PopScrollLayer();
    }
    mPushedClips.pop_back();
  }
  return;
}

} // namespace layers
} // namespace mozilla
