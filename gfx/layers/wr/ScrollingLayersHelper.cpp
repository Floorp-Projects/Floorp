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
                                             const StackingContextHelper& aStackingContext)
  : mLayer(aLayer)
  , mBuilder(&aBuilder)
  , mPushedLayerLocalClip(false)
{
  if (!mLayer->WrManager()->AsyncPanZoomEnabled()) {
    // If APZ is disabled then we don't need to push the scrolling clips. We
    // still want to push the layer's local clip though.
    PushLayerLocalClip(aStackingContext);
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
      PushLayerClip(clip.ref(), aStackingContext);
    }

    const FrameMetrics& fm = layer->GetFrameMetrics(i - 1);
    if (layer->GetIsFixedPosition() &&
        layer->GetFixedPositionScrollContainerId() == fm.GetScrollId()) {
      // If the layer contents are fixed for this metadata onwards, we need
      // to insert the layer's local clip at this point in the clip tree,
      // as a child of whatever's on the stack.
      PushLayerLocalClip(aStackingContext);
    }

    if (!fm.IsScrollable()) {
      continue;
    }
    LayerRect contentRect = ViewAs<LayerPixel>(
        fm.GetExpandedScrollableRect() * fm.GetDevPixelsPerCSSPixel(),
        PixelCastJustification::WebRenderHasUnitResolution);
    // TODO: check coordinate systems are sane here
    LayerRect clipBounds = ViewAs<LayerPixel>(
        fm.GetCompositionBounds(),
        PixelCastJustification::MovingDownToChildren);
    // The content rect that we hand to PushScrollLayer should be relative to
    // the same origin as the clipBounds that we hand to PushScrollLayer - that
    // is, both of them should be relative to the stacking context `aStackingContext`.
    // However, when we get the scrollable rect from the FrameMetrics, it has
    // a nominal top-left of 0,0 (maybe different for RTL pages?) and so to
    // get it in the same coordinate space we're going to shift it by the
    // composition bounds top-left.
    contentRect.MoveBy(clipBounds.TopLeft());
    mBuilder->PushScrollLayer(fm.GetScrollId(),
        aStackingContext.ToRelativeWrRect(contentRect),
        aStackingContext.ToRelativeWrRect(clipBounds));
  }

  // The scrolled clip on the layer is "inside" all of the scrollable metadatas
  // on that layer. That is, the clip scrolls along with the content in
  // child layers. So we need to apply this after pushing all the scroll layers,
  // which we do above.
  if (const Maybe<LayerClip>& scrolledClip = layer->GetScrolledClip()) {
    PushLayerClip(scrolledClip.ref(), aStackingContext);
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
    PushLayerLocalClip(aStackingContext);
  }
}

void
ScrollingLayersHelper::PushLayerLocalClip(const StackingContextHelper& aStackingContext)
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
    Maybe<WrImageMask> mask = mLayer->BuildWrMaskLayer(aStackingContext);
    LayerRect clipRect = ViewAs<LayerPixel>(clip.ref(),
        PixelCastJustification::MovingDownToChildren);
    mBuilder->PushClip(aStackingContext.ToRelativeWrRect(clipRect), mask.ptrOr(nullptr));
    mPushedLayerLocalClip = true;
  }
}

void
ScrollingLayersHelper::PushLayerClip(const LayerClip& aClip,
                                     const StackingContextHelper& aSc)
{
  LayerRect clipRect = IntRectToRect(ViewAs<LayerPixel>(aClip.GetClipRect(),
        PixelCastJustification::MovingDownToChildren));
  Maybe<WrImageMask> mask;
  if (Maybe<size_t> maskLayerIndex = aClip.GetMaskLayerIndex()) {
    Layer* maskLayer = mLayer->GetLayer()->GetAncestorMaskLayerAt(maskLayerIndex.value());
    WebRenderLayer* maskWrLayer = WebRenderLayer::ToWebRenderLayer(maskLayer);
    // TODO: check this transform is correct in all cases
    mask = maskWrLayer->RenderMaskLayer(aSc, maskLayer->GetTransform());
  }
  mBuilder->PushClip(aSc.ToRelativeWrRect(clipRect), mask.ptrOr(nullptr));
}

ScrollingLayersHelper::~ScrollingLayersHelper()
{
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
