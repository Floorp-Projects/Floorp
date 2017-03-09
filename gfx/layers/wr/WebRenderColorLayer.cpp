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
  WrScrollFrameStackingContextGenerator scrollFrames(this);

  gfx::Matrix4x4 transform = GetTransform();
  Rect rect = GetWrBoundsRect();
  Rect clip = GetWrClipRect(rect);

  gfx::Rect relBounds = GetWrRelBounds();
  gfx::Rect overflow(0, 0, relBounds.width, relBounds.height);
  WrMixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  Maybe<WrImageMask> mask = buildMaskLayer();
  WrClipRegion clipRegion = aBuilder.BuildClipRegion(wr::ToWrRect(clip));

  DumpLayerInfo("ColorLayer", rect);

  aBuilder.PushStackingContext(wr::ToWrRect(relBounds),
                              wr::ToWrRect(overflow),
                              mask.ptrOr(nullptr),
                              1.0f,
                              //GetAnimations(),
                              transform,
                              mixBlendMode);
  aBuilder.PushRect(wr::ToWrRect(rect), clipRegion, wr::ToWrColor(mColor));
  aBuilder.PopStackingContext();
}

} // namespace layers
} // namespace mozilla
