/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderContainerLayer.h"

#include <inttypes.h>
#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

void
WebRenderContainerLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  nsTArray<LayerPolygon> children = SortChildrenBy3DZOrder(SortMode::WITHOUT_GEOMETRY);

  gfx::Matrix4x4 transform = GetTransform();
  gfx::Matrix4x4* maybeTransform = &transform;
  float opacity = GetLocalOpacity();
  float* maybeOpacity = &opacity;
  uint64_t animationsId = 0;

  if (gfxPrefs::WebRenderOMTAEnabled() &&
      GetAnimations().Length()) {
    MOZ_ASSERT(GetCompositorAnimationsId());

    animationsId = GetCompositorAnimationsId();
    CompositorAnimations anim;
    anim.animations() = GetAnimations();
    anim.id() = animationsId;
    WrBridge()->AddWebRenderParentCommand(OpAddCompositorAnimations(anim));

    if (!HasOpacityAnimation()) {
      maybeOpacity = nullptr;
    }
    if (!HasTransformAnimation()) {
      maybeTransform = nullptr;
    }
  }

  StackingContextHelper sc(aBuilder, this, animationsId, maybeOpacity, maybeTransform);

  LayerRect rect = Bounds();
  DumpLayerInfo("ContainerLayer", rect);

  Maybe<WrImageMask> mask = BuildWrMaskLayer(true);
  aBuilder.PushClip(sc.ToRelativeWrRect(rect), mask.ptrOr(nullptr));

  for (LayerPolygon& child : children) {
    if (child.layer->IsBackfaceHidden()) {
      continue;
    }
    ToWebRenderLayer(child.layer)->RenderLayer(aBuilder);
  }
  aBuilder.PopClip();
}

void
WebRenderRefLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  gfx::Matrix4x4 transform;// = GetTransform();
  gfx::Rect relBounds = TransformedVisibleBoundsRelativeToParent();

  WrClipRegion clipRegion = aBuilder.BuildClipRegion(wr::ToWrRect(relBounds));

  if (gfxPrefs::LayersDump()) {
    printf_stderr("RefLayer %p (%" PRIu64 ") using bounds/overflow=%s, transform=%s\n",
                  this->GetLayer(),
                  mId,
                  Stringify(relBounds).c_str(),
                  Stringify(transform).c_str());
  }

  aBuilder.PushIFrame(wr::ToWrRect(relBounds), clipRegion, wr::AsPipelineId(mId));
}

} // namespace layers
} // namespace mozilla
