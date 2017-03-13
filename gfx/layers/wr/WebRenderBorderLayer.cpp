/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderBorderLayer.h"

#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

/* static */void
WebRenderBorderLayer::CreateWebRenderCommands(wr::DisplayListBuilder& aBuilder,
                                              WebRenderLayer* aLayer,
                                              BorderColors& aColors,
                                              BorderCorners& aCorners,
                                              BorderWidths& aWidths,
                                              BorderStyles& aBorderStyles,
                                              Rect aRect,
                                              Rect aClipRect,
                                              Rect aRelBounds,
                                              Rect aOverflow)
{
  aBuilder.PushStackingContext(wr::ToWrRect(aRelBounds),
                               wr::ToWrRect(aOverflow),
                               nullptr,
                               1.0f,
                               aLayer->GetLayer()->GetTransform(),
                               wr::MixBlendMode::Normal);
  aBuilder.PushBorder(wr::ToWrRect(aRect), aBuilder.BuildClipRegion(wr::ToWrRect(aClipRect)),
                      wr::ToWrBorderWidths(aWidths[0], aWidths[1], aWidths[2], aWidths[3]),
                      wr::ToWrBorderSide(aColors[0], aBorderStyles[0]),
                      wr::ToWrBorderSide(aColors[1], aBorderStyles[1]),
                      wr::ToWrBorderSide(aColors[2], aBorderStyles[2]),
                      wr::ToWrBorderSide(aColors[3], aBorderStyles[3]),
                      wr::ToWrBorderRadius(aCorners[eCornerTopLeft], aCorners[eCornerTopRight],
                                           aCorners[eCornerBottomLeft], aCorners[eCornerBottomRight]));
  aBuilder.PopStackingContext();
}

void
WebRenderBorderLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  gfx::Rect relBounds = GetWrRelBounds();
  gfx::Rect overflow(0, 0, relBounds.width, relBounds.height);

  gfx::Rect rect = GetWrBoundsRect();
  gfx::Rect clipRect = GetWrClipRect(rect);

  DumpLayerInfo("BorderLayer", rect);

  aBuilder.PushStackingContext(wr::ToWrRect(relBounds),
                               wr::ToWrRect(overflow),
                               nullptr,
                               1.0f,
                               //GetAnimations(),
                               GetTransform(),
                               wr::MixBlendMode::Normal);
  aBuilder.PushBorder(wr::ToWrRect(rect), aBuilder.BuildClipRegion(wr::ToWrRect(clipRect)),
                      wr::ToWrBorderWidths(mWidths[0], mWidths[1], mWidths[2], mWidths[3]),
                      wr::ToWrBorderSide(mColors[0], mBorderStyles[0]),
                      wr::ToWrBorderSide(mColors[1], mBorderStyles[1]),
                      wr::ToWrBorderSide(mColors[2], mBorderStyles[2]),
                      wr::ToWrBorderSide(mColors[3], mBorderStyles[3]),
                      wr::ToWrBorderRadius(mCorners[eCornerTopLeft], mCorners[eCornerTopRight],
                                           mCorners[eCornerBottomLeft], mCorners[eCornerBottomRight]));
  aBuilder.PopStackingContext();
}

} // namespace layers
} // namespace mozilla
