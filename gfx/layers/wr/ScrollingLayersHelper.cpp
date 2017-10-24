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
  , mCache(aCache)
{
  int32_t auPerDevPixel = aItem->Frame()->PresContext()->AppUnitsPerDevPixel();

  // There are two ASR chains here that we need to be fully defined. One is the
  // ASR chain pointed to by aItem->GetActiveScrolledRoot(). The other is the
  // ASR chain pointed to by aItem->GetClipChain()->mASR. We pick the leafmost
  // of these two chains because that one will include the other.
  // The leafmost clip is trivially going to be aItem->GetClipChain().
  // So we call DefineClipChain with these two leafmost things, and it will
  // recursively define all the clips and scroll layers with the appropriate
  // parents, but will not actually push anything onto the WR stack.
  const ActiveScrolledRoot* leafmostASR = aItem->GetActiveScrolledRoot();
  if (aItem->GetClipChain()) {
    leafmostASR = ActiveScrolledRoot::PickDescendant(leafmostASR,
        aItem->GetClipChain()->mASR);
  }
  auto ids = DefineClipChain(aItem, leafmostASR, aItem->GetClipChain(),
      auPerDevPixel, aStackingContext);

  // Now that stuff is defined, we need to ensure the right items are on the
  // stack. We need this primarily for the WR display items that will be
  // generated while processing aItem. However those display items only care
  // about the topmost clip on the stack. If that were all we cared about we
  // would only need to push one thing here and we would be done. However, we
  // also care about the ScrollingLayersHelper instance that might be created
  // for nested display items, in the case where aItem is a wrapper item. The
  // nested ScrollingLayersHelper may rely on things like TopmostScrollId and
  // TopmostClipId, so now we need to push at most two things onto the stack.

  FrameMetrics::ViewID leafmostId = ids.first.valueOr(FrameMetrics::NULL_SCROLL_ID);
  FrameMetrics::ViewID scrollId = aItem->GetActiveScrolledRoot()
      ? nsLayoutUtils::ViewIDForASR(aItem->GetActiveScrolledRoot())
      : FrameMetrics::NULL_SCROLL_ID;
  // If the leafmost ASR is not the same as the item's ASR then we are dealing
  // with a case where the item's clip chain is scrolled by something other than
  // the item's ASR. So for those cases we need to use the ClipAndScroll API.
  bool needClipAndScroll = (leafmostId != scrollId);

  // If we don't need a ClipAndScroll, ensure the item's ASR is at the top of
  // the scroll stack
  if (!needClipAndScroll && mBuilder->TopmostScrollId() != scrollId) {
    MOZ_ASSERT(leafmostId == scrollId); // because !needClipAndScroll
    mBuilder->PushScrollLayer(scrollId);
    mPushedClips.push_back(wr::ScrollOrClipId(scrollId));
  }
  // And ensure the leafmost clip, if scrolled by that ASR, is at the top of the
  // stack.
  if (ids.second && aItem->GetClipChain()->mASR == leafmostASR) {
    mBuilder->PushClip(ids.second.ref());
    mPushedClips.push_back(wr::ScrollOrClipId(ids.second.ref()));
  }
  // If we need the ClipAndScroll, we want to replace the topmost scroll layer
  // with the item's ASR but preseve the topmost clip (which is scrolled by
  // some other ASR).
  if (needClipAndScroll) {
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
                                       const StackingContextHelper& aStackingContext)
{
  // This is the main entry point for defining the clip chain for a display
  // item. This function recursively walks up the ASR chain and the display
  // item's clip chain to define all the ASRs and clips necessary. Each level
  // of the recursion defines one item, if it hasn't been defined already.
  // The |aAsr| and |aChain| parameters are the important ones to track during
  // the recursion; the rest of the parameters don't change.
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
    return RecurseAndDefineClip(aItem, aAsr, aChain, aAppUnitsPerDevPixel, aStackingContext);
  }
  if (aAsr) {
    return RecurseAndDefineAsr(aItem, aAsr, aChain, aAppUnitsPerDevPixel, aStackingContext);
  }

  MOZ_ASSERT(!aChain && !aAsr);

  return std::make_pair(Nothing(), Nothing());
}

std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>>
ScrollingLayersHelper::RecurseAndDefineClip(nsDisplayItem* aItem,
                                            const ActiveScrolledRoot* aAsr,
                                            const DisplayItemClipChain* aChain,
                                            int32_t aAppUnitsPerDevPixel,
                                            const StackingContextHelper& aSc)
{
  MOZ_ASSERT(aChain);

  // This will hold our return value
  std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>> ids;

  if (mBuilder->HasExtraClip()) {
    // We can't use mCache directly. However if there's an out-of-band clip that
    // was pushed on top of aChain, we should return the id for that OOB clip,
    // so that anything we want to define as a descendant of aChain we actually
    // end up defining as a descendant of the OOB clip.
    ids.second = mBuilder->GetCacheOverride(aChain);
  } else {
    auto it = mCache.find(aChain);
    if (it != mCache.end()) {
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
      aItem, aAsr, aChain->mParent, aAppUnitsPerDevPixel, aSc);
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
  } else {
    MOZ_ASSERT(!ancestorIds.second);
    // If aChain->mASR is already the topmost scroll layer on the stack, but
    // but there was another clip pushed *on top* of that ASR, then that clip
    // shares the ASR, and we need to make our clip a child of that clip, which
    // in turn will already be a descendant of the correct ASR.
    // This covers the cases where e.g. the Gecko display list has nested items,
    // and the clip chain on the nested item implicitly extends from the clip
    // chain on the containing wrapper item. In this case the aChain->mParent
    // pointer will be null for the nested item but the containing wrapper's
    // clip will be on the stack already and we can pick it up from there.
    // Another way of thinking about this is that if the clip chain were
    // "fully completed" then aChain->mParent wouldn't be null but would point
    // to the clip corresponding to mBuilder->TopmostClipId(), and we would
    // have gone into the |aChain->mParent->mASR == aAsr| branch above.
    FrameMetrics::ViewID scrollId = aChain->mASR ? nsLayoutUtils::ViewIDForASR(aChain->mASR) : FrameMetrics::NULL_SCROLL_ID;
    if (mBuilder->TopmostScrollId() == scrollId && mBuilder->TopmostIsClip()) {
      ancestorIds.first = Nothing();
      ancestorIds.second = mBuilder->TopmostClipId();
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
    mCache[aChain] = clipId;
  }

  ids.second = Some(clipId);
  return ids;
}

std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>>
ScrollingLayersHelper::RecurseAndDefineAsr(nsDisplayItem* aItem,
                                           const ActiveScrolledRoot* aAsr,
                                           const DisplayItemClipChain* aChain,
                                           int32_t aAppUnitsPerDevPixel,
                                           const StackingContextHelper& aSc)
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
        auto it = mCache.find(aChain);
        if (it == mCache.end()) {
          // Degenerate case, where there are two clip chain items that are
          // fundamentally the same but are different objects and so we can't
          // find it in the cache via hashing. Linear search for it instead.
          // XXX This shouldn't happen very often but it might still turn out
          // to be a performance cliff, so we should figure out a better way to
          // deal with this.
          for (it = mCache.begin(); it != mCache.end(); it++) {
            if (DisplayItemClipChain::Equal(aChain, it->first)) {
              break;
            }
          }
        }
        // If |it == mCache.end()| here then we have run into a case where the
        // scroll layer was previously defined a specific parent clip, and
        // now here it has a different parent clip. Gecko can create display
        // lists like this because it treats the ASR chain and clipping chain
        // more independently, but we can't yet represent this in WR. This is
        // tracked by bug 1409442. For now we'll just leave ids.second as
        // Nothing() which will effectively ignore the clip |aChain|. Once WR
        // supports multiple ancestors on a scroll layer we can deal with this
        // better. The layout/reftests/text/wordwrap-08.html has a Text display
        // item that exercises this case.
        if (it != mCache.end()) {
          ids.second = Some(it->second);
        }
      }
    }
    return ids;
  }

  // If not, recurse to ensure all the ancestors are defined
  auto ancestorIds = DefineClipChain(
      aItem, aAsr->mParent, aChain, aAppUnitsPerDevPixel, aSc);
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
}

} // namespace layers
} // namespace mozilla
