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
{
  if (!mLayer->WrManager()->AsyncPanZoomEnabled()) {
    // If APZ is disabled then we don't need to push the scrolling clips
    return;
  }

  Layer* layer = mLayer->GetLayer();
  for (uint32_t i = layer->GetScrollMetadataCount(); i > 0; i--) {
    const FrameMetrics& fm = layer->GetFrameMetrics(i - 1);
    if (!fm.IsScrollable()) {
      return;
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
}

ScrollingLayersHelper::~ScrollingLayersHelper()
{
  if (!mLayer->WrManager()->AsyncPanZoomEnabled()) {
    return;
  }

  Layer* layer = mLayer->GetLayer();
  for (int32_t i = layer->GetScrollMetadataCount(); i > 0; i--) {
    const FrameMetrics& fm = layer->GetFrameMetrics(i - 1);
    if (!fm.IsScrollable()) {
      return;
    }
    mBuilder->PopScrollLayer();
  }
}

} // namespace layers
} // namespace mozilla
