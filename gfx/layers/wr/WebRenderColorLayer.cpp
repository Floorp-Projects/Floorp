/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderColorLayer.h"

#include "WebRenderLayersLogging.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
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

  gfx::Matrix4x4 transform;// = GetTransform();
  gfx::Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  gfx::Rect overflow(0, 0, relBounds.width, relBounds.height);
  WrMixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  Maybe<WrImageMask> mask = buildMaskLayer();

  if (gfxPrefs::LayersDump()) {
    printf_stderr("ColorLayer %p using bounds=%s, overflow=%s, transform=%s, rect=%s, clip=%s, mix-blend-mode=%s\n",
                  this->GetLayer(),
                  Stringify(relBounds).c_str(),
                  Stringify(overflow).c_str(),
                  Stringify(transform).c_str(),
                  Stringify(rect).c_str(),
                  Stringify(clip).c_str(),
                  Stringify(mixBlendMode).c_str());
  }

  WrBridge()->AddWebRenderCommand(
      OpDPPushStackingContext(wr::ToWrRect(relBounds),
                              wr::ToWrRect(overflow),
                              mask,
                              1.0f,
                              GetAnimations(),
                              transform,
                              mixBlendMode,
                              FrameMetrics::NULL_SCROLL_ID));
  WrBridge()->AddWebRenderCommand(
    OpDPPushRect(wr::ToWrRect(rect), wr::ToWrRect(clip), wr::ToWrColor(mColor)));
  WrBridge()->AddWebRenderCommand(OpDPPopStackingContext());
}

} // namespace layers
} // namespace mozilla
