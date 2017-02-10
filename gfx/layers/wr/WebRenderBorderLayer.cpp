/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderBorderLayer.h"

#include "WebRenderLayersLogging.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
WebRenderBorderLayer::RenderLayer()
{
  WrScrollFrameStackingContextGenerator scrollFrames(this);

  Rect rect = RelativeToVisible(mRect.ToUnknownRect());
  Rect clip;
  if (GetClipRect().isSome()) {
    clip = RelativeToTransformedVisible(IntRectToRect(GetClipRect().ref().ToUnknownRect()));
  } else {
    clip = rect;
  }

  Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  Rect overflow(0, 0, relBounds.width, relBounds.height);
  Matrix4x4 transform;// = GetTransform();

  if (gfxPrefs::LayersDump()) {
    printf_stderr("BorderLayer %p using bounds=%s, overflow=%s, transform=%s, rect=%s, clip=%s\n",
                  this->GetLayer(),
                  Stringify(relBounds).c_str(),
                  Stringify(overflow).c_str(),
                  Stringify(transform).c_str(),
                  Stringify(rect).c_str(),
                  Stringify(clip).c_str());
  }

  WrBridge()->AddWebRenderCommand(
      OpDPPushStackingContext(wr::ToWrRect(relBounds),
                              wr::ToWrRect(overflow),
                              Nothing(),
                              1.0f,
                              GetAnimations(),
                              transform,
                              WrMixBlendMode::Normal,
                              FrameMetrics::NULL_SCROLL_ID));
  WrBridge()->AddWebRenderCommand(
    OpDPPushBorder(wr::ToWrRect(rect), wr::ToWrRect(clip),
                   wr::ToWrBorderSide(mWidths[0], mColors[0], mBorderStyles[0]),
                   wr::ToWrBorderSide(mWidths[1], mColors[1], mBorderStyles[1]),
                   wr::ToWrBorderSide(mWidths[2], mColors[2], mBorderStyles[2]),
                   wr::ToWrBorderSide(mWidths[3], mColors[3], mBorderStyles[3]),
                   wr::ToWrBorderRadius(mCorners[0], mCorners[1], mCorners[3], mCorners[2])));
  WrBridge()->AddWebRenderCommand(OpDPPopStackingContext());
}

} // namespace layers
} // namespace mozilla
