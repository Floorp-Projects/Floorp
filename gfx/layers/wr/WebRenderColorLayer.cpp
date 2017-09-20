/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderColorLayer.h"

#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/ScrollingLayersHelper.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderBridgeChild.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
WebRenderColorLayer::RenderLayer(wr::DisplayListBuilder& aBuilder,
                                 wr::IpcResourceUpdateQueue& aResources,
                                 const StackingContextHelper& aSc)
{
  ScrollingLayersHelper scroller(this, aBuilder, aResources, aSc);
  StackingContextHelper sc(aSc, aBuilder, this);

  LayerRect rect = Bounds();
  DumpLayerInfo("ColorLayer", rect);

  wr::LayoutRect r = sc.ToRelativeLayoutRect(rect);
  aBuilder.PushRect(r, r, wr::ToColorF(mColor));
}

} // namespace layers
} // namespace mozilla
