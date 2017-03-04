/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderTextLayer.h"

#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/WebRenderBridgeChild.h"

#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
WebRenderTextLayer::RenderLayer()
{
    if (mBounds.IsEmpty()) {
        return;
    }

    gfx::Rect rect = RelativeToParent(GetTransform().TransformBounds(IntRectToRect(mBounds)));
    gfx::Rect clip;
    if (GetClipRect().isSome()) {
      clip = RelativeToParent(IntRectToRect(GetClipRect().ref().ToUnknownRect()));
    } else {
      clip = rect;
    }

    if (gfxPrefs::LayersDump()) {
        printf_stderr("TextLayer %p using rect=%s, clip=%s\n",
                      this->GetLayer(),
                      Stringify(rect).c_str(),
                      Stringify(clip).c_str());
    }

    nsTArray<WebRenderCommand> commands;
    mGlyphHelper.BuildWebRenderCommands(WrBridge(), commands, mGlyphs, mFont,
                                        GetOffsetToParent(), rect, clip);
    WrBridge()->AddWebRenderCommands(commands);
}

} // namespace layers
} // namespace mozilla
