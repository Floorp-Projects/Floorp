/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderContainerLayer.h"

#include <inttypes.h>
#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

void
WebRenderContainerLayer::RenderLayer()
{
  WrScrollFrameStackingContextGenerator scrollFrames(this);

  nsTArray<LayerPolygon> children = SortChildrenBy3DZOrder(SortMode::WITHOUT_GEOMETRY);
  gfx::Matrix4x4 transform = GetTransform();
  gfx::Rect relBounds = VisibleBoundsRelativeToParent();
  if (!transform.IsIdentity()) {
    // WR will only apply the 'translate' of the transform, so we need to do the scale/rotation manually.
    gfx::Matrix4x4 boundTransform = transform;
    boundTransform._41 = 0.0f;
    boundTransform._42 = 0.0f;
    boundTransform._43 = 0.0f;
    relBounds.MoveTo(boundTransform.TransformPoint(relBounds.TopLeft()));
  }

  gfx::Rect overflow(0, 0, relBounds.width, relBounds.height);
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

  WrBridge()->AddWebRenderCommand(OpDPPushIframe(wr::ToWrRect(relBounds), wr::ToWrRect(relBounds), wr::AsPipelineId(mId)));
}

} // namespace layers
} // namespace mozilla
