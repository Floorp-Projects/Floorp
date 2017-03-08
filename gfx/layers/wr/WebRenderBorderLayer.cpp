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
                               WrMixBlendMode::Normal);

  aBuilder.PushBorder(wr::ToWrRect(aRect), wr::ToWrRect(aClipRect),
                      wr::ToWrBorderSide(aWidths[0], aColors[0], aBorderStyles[0]),
                      wr::ToWrBorderSide(aWidths[1], aColors[1], aBorderStyles[1]),
                      wr::ToWrBorderSide(aWidths[2], aColors[2], aBorderStyles[2]),
                      wr::ToWrBorderSide(aWidths[3], aColors[3], aBorderStyles[3]),
                      wr::ToWrBorderRadius(aCorners[0], aCorners[1],
                                           aCorners[3], aCorners[2]));

  aBuilder.PopStackingContext();
}

void
WebRenderBorderLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  WrScrollFrameStackingContextGenerator scrollFrames(this);

  Rect rect = GetWrBoundsRect();
  Rect clip = GetWrClipRect(rect);
  Rect relBounds = GetWrRelBounds();
  Rect overflow(0, 0, relBounds.width, relBounds.height);

  DumpLayerInfo("BorderLayer", rect);

  aBuilder.PushStackingContext(wr::ToWrRect(relBounds),
                               wr::ToWrRect(overflow),
                               nullptr,
                               1.0f,
                               //GetAnimations(),
                               GetTransform(),
                               WrMixBlendMode::Normal);
  aBuilder.PushBorder(wr::ToWrRect(rect), wr::ToWrRect(clip),
                      wr::ToWrBorderSide(mWidths[0], mColors[0], mBorderStyles[0]),
                      wr::ToWrBorderSide(mWidths[1], mColors[1], mBorderStyles[1]),
                      wr::ToWrBorderSide(mWidths[2], mColors[2], mBorderStyles[2]),
                      wr::ToWrBorderSide(mWidths[3], mColors[3], mBorderStyles[3]),
                      wr::ToWrBorderRadius(mCorners[0], mCorners[1], mCorners[3], mCorners[2]));
  aBuilder.PopStackingContext();
}

} // namespace layers
} // namespace mozilla
