/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderTextLayer.h"

#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/ScrollingLayersHelper.h"
#include "mozilla/layers/WebRenderBridgeChild.h"

#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
WebRenderTextLayer::RenderLayer(wr::DisplayListBuilder& aBuilder,
                                const StackingContextHelper& aSc)
{
    if (mBounds.IsEmpty()) {
        return;
    }

    ScrollingLayersHelper scroller(this, aBuilder, aSc);

    LayerRect rect = LayerRect::FromUnknownRect(
        // I am not 100% sure this is correct, but it probably is. Because:
        // the bounds are in layer space, and when gecko composites layers it
        // applies the transform to the layer before compositing. However with
        // WebRender compositing, we don't pass the transform on this layer to
        // WR, so WR has no way of knowing about the transformed bounds unless
        // we apply it here. The glyphs that we push to WR should already be
        // taking the transform into account.
        GetTransform().TransformBounds(IntRectToRect(mBounds))
    );
    DumpLayerInfo("TextLayer", rect);

    WrBridge()->PushGlyphs(aBuilder, mGlyphs, mFont, aSc, rect, rect);
}

} // namespace layers
} // namespace mozilla
