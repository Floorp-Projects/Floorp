/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderColorLayer.h"

#include "LayersLogging.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/layers/WebRenderBridgeChild.h"

namespace mozilla {
namespace layers {

void
WebRenderColorLayer::RenderLayer()
{
  WrScrollFrameStackingContextGenerator scrollFrames(this);

  LayerIntRegion visibleRegion = GetVisibleRegion();
  LayerIntRect bounds = visibleRegion.GetBounds();
  Rect rect = RelativeToVisible(IntRectToRect(bounds.ToUnknownRect()));
  Rect clip;
  if (GetClipRect().isSome()) {
      clip = RelativeToTransformedVisible(IntRectToRect(GetClipRect().ref().ToUnknownRect()));
  } else {
      clip = rect;
  }
  if (gfxPrefs::LayersDump()) printf_stderr("ColorLayer %p using rect:%s clip:%s\n", this, Stringify(rect).c_str(), Stringify(clip).c_str());


  gfx::Matrix4x4 transform;// = GetTransform();
  gfx::Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  gfx::Rect overflow(0, 0, relBounds.width, relBounds.height);

  Maybe<WrImageMask> mask = buildMaskLayer();

  WrBridge()->AddWebRenderCommand(
      OpDPPushStackingContext(wr::ToWrRect(relBounds), wr::ToWrRect(overflow), mask, transform, FrameMetrics::NULL_SCROLL_ID));
  WrBridge()->AddWebRenderCommand(
    OpDPPushRect(wr::ToWrRect(rect), wr::ToWrRect(clip), mColor.r, mColor.g, mColor.b, mColor.a));

  if (gfxPrefs::LayersDump()) printf_stderr("ColorLayer %p using %s as bounds, %s as overflow, %s for transform\n", this, Stringify(relBounds).c_str(), Stringify(overflow).c_str(), Stringify(transform).c_str());
  WrBridge()->AddWebRenderCommand(OpDPPopStackingContext());
}

} // namespace layers
} // namespace mozilla
