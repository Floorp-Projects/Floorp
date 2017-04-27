/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderColorLayer.h"

#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/WebRenderBridgeChild.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
WebRenderColorLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  gfx::Matrix4x4 transform = GetTransform();
  gfx::Rect relBounds = GetWrRelBounds();
  LayerRect rect = GetWrBoundsRect();

  LayerRect clipRect = GetWrClipRect(rect);
  Maybe<WrImageMask> mask = BuildWrMaskLayer(true);
  WrClipRegion clip = aBuilder.BuildClipRegion(wr::ToWrRect(clipRect), mask.ptrOr(nullptr));

  wr::MixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  DumpLayerInfo("ColorLayer", rect);

  aBuilder.PushStackingContext(wr::ToWrRect(relBounds),
                              1.0f,
                              transform,
                              mixBlendMode);
  aBuilder.PushRect(wr::ToWrRect(rect), clip, wr::ToWrColor(mColor));
  aBuilder.PopStackingContext();
}

} // namespace layers
} // namespace mozilla
