/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderContainerLayer.h"

#include <inttypes.h>
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "WebRenderLayersLogging.h"

namespace mozilla {
namespace layers {

void
WebRenderContainerLayer::RenderLayer()
{
  WrScrollFrameStackingContextGenerator scrollFrames(this);

  nsTArray<LayerPolygon> children = SortChildrenBy3DZOrder(SortMode::WITHOUT_GEOMETRY);

  gfx::Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  gfx::Rect overflow(0, 0, relBounds.width, relBounds.height);
  gfx::Matrix4x4 transform;// = GetTransform();
  WrMixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());
  Maybe<WrImageMask> mask = buildMaskLayer();

  if (gfxPrefs::LayersDump()) {
    printf_stderr("ContainerLayer %p using bounds=%s, overflow=%s, transform=%s, mix-blend-mode=%s\n",
                  this->GetLayer(),
                  Stringify(relBounds).c_str(),
                  Stringify(overflow).c_str(),
                  Stringify(transform).c_str(),
                  Stringify(mixBlendMode).c_str());
  }

  WrBridge()->AddWebRenderCommand(
    OpDPPushStackingContext(wr::ToWrRect(relBounds),
                            wr::ToWrRect(overflow),
                            mask,
                            GetLocalOpacity(),
                            GetLayer()->GetAnimations(),
                            transform,
                            mixBlendMode,
                            FrameMetrics::NULL_SCROLL_ID));
  for (LayerPolygon& child : children) {
    if (child.layer->IsBackfaceHidden()) {
      continue;
    }
    ToWebRenderLayer(child.layer)->RenderLayer();
  }
  WrBridge()->AddWebRenderCommand(
    OpDPPopStackingContext());
}

void
WebRenderRefLayer::RenderLayer()
{
  WrScrollFrameStackingContextGenerator scrollFrames(this);

  gfx::Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  gfx::Matrix4x4 transform;// = GetTransform();

  if (gfxPrefs::LayersDump()) {
    printf_stderr("RefLayer %p (%" PRIu64 ") using bounds/overflow=%s, transform=%s\n",
                  this->GetLayer(),
                  mId,
                  Stringify(relBounds).c_str(),
                  Stringify(transform).c_str());
  }

  WrBridge()->AddWebRenderCommand(OpDPPushIframe(wr::ToWrRect(relBounds), wr::ToWrRect(relBounds), wr::PipelineId(mId)));
}

} // namespace layers
} // namespace mozilla
