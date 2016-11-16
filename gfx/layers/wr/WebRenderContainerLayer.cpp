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

  AutoTArray<Layer*, 12> children;
  SortChildrenBy3DZOrder(children);

  gfx::Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  gfx::Matrix4x4 transform;// = GetTransform();
  if (gfxPrefs::LayersDump()) printf_stderr("ContainerLayer %p using %s as bounds/overflow, %s as transform\n", this, Stringify(relBounds).c_str(), Stringify(transform).c_str());

  WRBridge()->SendPushDLBuilder();
  for (Layer* child : children) {
    ToWebRenderLayer(child)->RenderLayer();
  }
  WRBridge()->SendPopDLBuilder(toWrRect(relBounds), toWrRect(relBounds), transform, FrameMetrics::NULL_SCROLL_ID);
}

void
WebRenderRefLayer::RenderLayer()
{
  WRScrollFrameStackingContextGenerator scrollFrames(this);

  gfx::Rect relBounds = TransformedVisibleBoundsRelativeToParent();
  gfx::Matrix4x4 transform;// = GetTransform();
  if (gfxPrefs::LayersDump()) printf_stderr("RefLayer %p (%" PRIu64 ") using %s as bounds/overflow, %s as transform\n", this, mId, Stringify(relBounds).c_str(), Stringify(transform).c_str());
  WRBridge()->SendDPPushIframe(toWrRect(relBounds), toWrRect(relBounds), mId);
}

} // namespace layers
} // namespace mozilla
