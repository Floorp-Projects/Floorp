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
    LayoutDeviceRect contentRect = fm.GetExpandedScrollableRect()
        * fm.GetDevPixelsPerCSSPixel();
    // TODO: check coordinate systems are sane here
    LayerRect clipBounds = ViewAs<LayerPixel>(
        fm.GetCompositionBounds(),
        PixelCastJustification::MovingDownToChildren);
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
