/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ScrollingLayersHelper.h"

#include "FrameMetrics.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderLayer.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "UnitTransforms.h"

namespace mozilla {
namespace layers {

ScrollingLayersHelper::ScrollingLayersHelper(WebRenderLayer* aLayer,
                                             wr::DisplayListBuilder& aBuilder,
                                             wr::IpcResourceUpdateQueue& aResources,
                                             const StackingContextHelper& aStackingContext)
  : mLayer(aLayer)
  , mBuilder(&aBuilder)
  , mPushedLayerLocalClip(false)
  , mPushedClipAndScroll(false)
{
  if (!mLayer->WrManager()->AsyncPanZoomEnabled()) {
    // If APZ is disabled then we don't need to push the scrolling clips. We
    // still want to push the layer's local clip though.
    PushLayerLocalClip(aStackingContext, aResources);
    return;
  }

  Layer* layer = mLayer->GetLayer();
  for (uint32_t i = layer->GetScrollMetadataCount(); i > 0; i--) {
    const ScrollMetadata& metadata = layer->GetScrollMetadata(i - 1);
    // The scroll clip on a given metadata is affected by all async transforms
    // from metadatas "above" it, but not the async transform on the metadata
    // itself. Therefore we need to push this clip before we push the
    // corresponding scroll layer, so that when we set an async scroll position
    // on the scroll layer, the clip isn't affected by it.
    if (const Maybe<LayerClip>& clip = metadata.GetScrollClip()) {
      PushLayerClip(clip.ref(), aStackingContext, aResources);
    }

    const FrameMetrics& fm = layer->GetFrameMetrics(i - 1);
    if (layer->GetIsFixedPosition() &&
        layer->GetFixedPositionScrollContainerId() == fm.GetScrollId()) {
      // If the layer contents are fixed for this metadata onwards, we need
      // to insert the layer's local clip at this point in the clip tree,
      // as a child of whatever's on the stack.
      PushLayerLocalClip(aStackingContext, aResources);
    }

    DefineAndPushScrollLayer(fm, aStackingContext);
  }

  // The scrolled clip on the layer is "inside" all of the scrollable metadatas
  // on that layer. That is, the clip scrolls along with the content in
  // child layers. So we need to apply this after pushing all the scroll layers,
  // which we do above.
  if (const Maybe<LayerClip>& scrolledClip = layer->GetScrolledClip()) {
    PushLayerClip(scrolledClip.ref(), aStackingContext, aResources);
  }

  // If the layer is marked as fixed-position, it is fixed relative to something
  // (the scroll layer referred to by GetFixedPositionScrollContainerId, hereafter
  // referred to as the "scroll container"). What this really means is that we
  // don't want this content to scroll with any scroll layer on the stack up to
  // and including the scroll container, but we do want it to scroll with any
  // ancestor scroll layers.
  // Also, the local clip on the layer (defined by layer->GetClipRect() and
  // layer->GetMaskLayer()) also need to be fixed relative to the scroll
  // container. This is why we inserted it into the clip tree during the
  // loop above when we encountered the scroll container.
  // At this point we do a PushClipAndScrollInfo that maintains
  // the current non-scrolling clip stack, but resets the scrolling clip stack
  // to the ancestor of the scroll container.
  if (layer->GetIsFixedPosition()) {
    FrameMetrics::ViewID fixedFor = layer->GetFixedPositionScrollContainerId();
    Maybe<FrameMetrics::ViewID> scrollsWith = mBuilder->ParentScrollIdFor(fixedFor);
    Maybe<wr::WrClipId> clipId = mBuilder->TopmostClipId();
    // Default to 0 if there is no ancestor, because 0 refers to the root scrollframe.
    mBuilder->PushClipAndScrollInfo(scrollsWith.valueOr(0), clipId.ptrOr(nullptr));
  } else {
    PushLayerLocalClip(aStackingContext, aResources);
  }
}

ScrollingLayersHelper::ScrollingLayersHelper(nsDisplayItem* aItem,
                                             wr::DisplayListBuilder& aBuilder,
                                             const StackingContextHelper& aStackingContext,
                                             WebRenderLayerManager::ClipIdMap& aCache,
                                             bool aApzEnabled)
  : mLayer(nullptr)
  , mBuilder(&aBuilder)
  , mPushedLayerLocalClip(false)
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

void
ScrollingLayersHelper::DefineAndPushScrollLayers(nsDisplayItem* aItem,
                                                 const ActiveScrolledRoot* aAsr,
                                                 const DisplayItemClipChain* aChain,
                                                 wr::DisplayListBuilder& aBuilder,
                                                 int32_t aAppUnitsPerDevPixel,
                                                 const StackingContextHelper& aStackingContext,
                                                 WebRenderLayerManager::ClipIdMap& aCache)
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
                                          WebRenderLayerManager::ClipIdMap& aCache)
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
  if (!clipId) {
    // If we don't have a clip id for this chain item yet, define the clip in WR
    // and save the id
    LayoutDeviceRect clip = LayoutDeviceRect::FromAppUnits(
        aChain->mClip.GetClipRect(), aAppUnitsPerDevPixel);
    nsTArray<wr::WrComplexClipRegion> wrRoundedRects;
    aChain->mClip.ToWrComplexClipRegions(aAppUnitsPerDevPixel, aStackingContext, wrRoundedRects);
    clipId = Some(aBuilder.DefineClip(aStackingContext.ToRelativeLayoutRect(clip), &wrRoundedRects));
    aCache[aChain] = clipId.value();
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
  LayerRect contentRect = ViewAs<LayerPixel>(
      aMetrics.GetExpandedScrollableRect() * aMetrics.GetDevPixelsPerCSSPixel(),
      PixelCastJustification::WebRenderHasUnitResolution);
  // TODO: check coordinate systems are sane here
  LayerRect clipBounds = ViewAs<LayerPixel>(
      aMetrics.GetCompositionBounds(),
      PixelCastJustification::MovingDownToChildren);
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
  mBuilder->DefineScrollLayer(aMetrics.GetScrollId(),
      aStackingContext.ToRelativeLayoutRect(contentRect),
      aStackingContext.ToRelativeLayoutRect(clipBounds));
  mBuilder->PushScrollLayer(aMetrics.GetScrollId());
  return true;
}

void
ScrollingLayersHelper::PushLayerLocalClip(const StackingContextHelper& aStackingContext,
                                          wr::IpcResourceUpdateQueue& aResources)
{
  Layer* layer = mLayer->GetLayer();
  Maybe<ParentLayerRect> clip;
  if (const Maybe<ParentLayerIntRect>& rect = layer->GetClipRect()) {
    clip = Some(IntRectToRect(rect.ref()));
  } else if (layer->GetMaskLayer()) {
    // this layer has a mask, but no clip rect. so let's use the transformed
    // visible bounds as the clip rect.
    clip = Some(layer->GetLocalTransformTyped().TransformBounds(mLayer->Bounds()));
  }
  if (clip) {
    Maybe<wr::WrImageMask> mask = mLayer->BuildWrMaskLayer(aStackingContext, aResources);
    LayerRect clipRect = ViewAs<LayerPixel>(clip.ref(),
        PixelCastJustification::MovingDownToChildren);
    mBuilder->PushClip(mBuilder->DefineClip(
        aStackingContext.ToRelativeLayoutRect(clipRect), nullptr, mask.ptrOr(nullptr)));
    mPushedLayerLocalClip = true;
  }
}

void
ScrollingLayersHelper::PushLayerClip(const LayerClip& aClip,
                                     const StackingContextHelper& aSc,
                                     wr::IpcResourceUpdateQueue& aResources)
{
  LayerRect clipRect = IntRectToRect(ViewAs<LayerPixel>(aClip.GetClipRect(),
        PixelCastJustification::MovingDownToChildren));
  Maybe<wr::WrImageMask> mask;
  if (Maybe<size_t> maskLayerIndex = aClip.GetMaskLayerIndex()) {
    Layer* maskLayer = mLayer->GetLayer()->GetAncestorMaskLayerAt(maskLayerIndex.value());
    WebRenderLayer* maskWrLayer = WebRenderLayer::ToWebRenderLayer(maskLayer);
    // TODO: check this transform is correct in all cases
    mask = maskWrLayer->RenderMaskLayer(aSc, maskLayer->GetTransform(), aResources);
  }
  mBuilder->PushClip(mBuilder->DefineClip(
      aSc.ToRelativeLayoutRect(clipRect), nullptr, mask.ptrOr(nullptr)));
}

ScrollingLayersHelper::~ScrollingLayersHelper()
{
  if (!mLayer) {
    // For layers-free mode.
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

  Layer* layer = mLayer->GetLayer();
  if (!mLayer->WrManager()->AsyncPanZoomEnabled()) {
    if (mPushedLayerLocalClip) {
      mBuilder->PopClip();
    }
    return;
  }

  if (layer->GetIsFixedPosition()) {
    mBuilder->PopClipAndScrollInfo();
  } else if (mPushedLayerLocalClip) {
    mBuilder->PopClip();
  }
  if (layer->GetScrolledClip()) {
    mBuilder->PopClip();
  }
  for (uint32_t i = 0; i < layer->GetScrollMetadataCount(); i++) {
    const FrameMetrics& fm = layer->GetFrameMetrics(i);
    if (fm.IsScrollable()) {
      mBuilder->PopScrollLayer();
    }
    if (layer->GetIsFixedPosition() &&
        layer->GetFixedPositionScrollContainerId() == fm.GetScrollId() &&
        mPushedLayerLocalClip) {
      mBuilder->PopClip();
    }
    const ScrollMetadata& metadata = layer->GetScrollMetadata(i);
    if (metadata.GetScrollClip()) {
      mBuilder->PopClip();
    }
  }
}

} // namespace layers
} // namespace mozilla
