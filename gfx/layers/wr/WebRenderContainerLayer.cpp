/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderContainerLayer.h"

#include <inttypes.h>
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "LayersLogging.h"

namespace mozilla {
namespace layers {

void
WebRenderContainerLayer::RenderLayer()
{
  WRScrollFrameStackingContextGenerator scrollFrames(this);

  nsTArray<LayerPolygon> children = SortChildrenBy3DZOrder(SortMode::WITHOUT_GEOMETRY);

  gfx::Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  gfx::Rect overflow(0, 0, relBounds.width, relBounds.height);
  gfx::Matrix4x4 transform;// = GetTransform();
  if (gfxPrefs::LayersDump()) printf_stderr("ContainerLayer %p using %s as bounds, %s as overflow, %s as transform\n", this, Stringify(relBounds).c_str(), Stringify(overflow).c_str(), Stringify(transform).c_str());

  Maybe<WRImageMask> mask = buildMaskLayer();

  WRBridge()->AddWebRenderCommand(
    OpDPPushStackingContext(ToWRRect(relBounds), ToWRRect(overflow), mask, transform, FrameMetrics::NULL_SCROLL_ID));
  for (LayerPolygon& child : children) {
    if (child.layer->IsBackfaceHidden()) {
      continue;
    }
    ToWebRenderLayer(child.layer)->RenderLayer();
  }
  WRBridge()->AddWebRenderCommand(
    OpDPPopStackingContext());
}

void
WebRenderRefLayer::RenderLayer()
{
  WRScrollFrameStackingContextGenerator scrollFrames(this);

  gfx::Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  gfx::Matrix4x4 transform;// = GetTransform();
  if (gfxPrefs::LayersDump()) printf_stderr("RefLayer %p (%" PRIu64 ") using %s as bounds/overflow, %s as transform\n", this, mId, Stringify(relBounds).c_str(), Stringify(transform).c_str());
  WRBridge()->AddWebRenderCommand(OpDPPushIframe(ToWRRect(relBounds), ToWRRect(relBounds), mId));
}

} // namespace layers
} // namespace mozilla
