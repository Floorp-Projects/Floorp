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
WebRenderContainerLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  nsTArray<LayerPolygon> children = SortChildrenBy3DZOrder(SortMode::WITHOUT_GEOMETRY);

  gfx::Matrix4x4 transform = GetTransform();
  gfx::Rect relBounds = GetWrRelBounds();
  gfx::Rect overflow(0, 0, relBounds.width, relBounds.height);

  Maybe<WrImageMask> mask = BuildWrMaskLayer();

  wr::MixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  if (gfxPrefs::LayersDump()) {
    printf_stderr("ContainerLayer %p using bounds=%s, overflow=%s, transform=%s, mix-blend-mode=%s\n",
                  this->GetLayer(),
                  Stringify(relBounds).c_str(),
                  Stringify(overflow).c_str(),
                  Stringify(transform).c_str(),
                  Stringify(mixBlendMode).c_str());
  }
  aBuilder.PushStackingContext(wr::ToWrRect(relBounds),
                               wr::ToWrRect(overflow),
                               mask.ptrOr(nullptr),
                               GetLocalOpacity(),
                               //GetLayer()->GetAnimations(),
                               transform,
                               mixBlendMode);
  for (LayerPolygon& child : children) {
    if (child.layer->IsBackfaceHidden()) {
      continue;
    }
    ToWebRenderLayer(child.layer)->RenderLayer(aBuilder);
  }
  aBuilder.PopStackingContext();
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
