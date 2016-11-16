/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderColorLayer.h"

#include "LayersLogging.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "webrender.h"

namespace mozilla {
namespace layers {

void
WebRenderColorLayer::RenderLayer()
{
  WRScrollFrameStackingContextGenerator scrollFrames(this);

  gfx::Rect rect = RelativeToParent(GetTransform().TransformBounds(IntRectToRect(mBounds)));
  gfx::Rect clip;
  if (GetClipRect().isSome()) {
      clip = RelativeToParent(IntRectToRect(GetClipRect().ref().ToUnknownRect()));
  } else {
      clip = rect;
  }
  if (gfxPrefs::LayersDump()) printf_stderr("ColorLayer %p using rect:%s clip:%s\n", this, Stringify(rect).c_str(), Stringify(clip).c_str());
  WRBridge()->SendDPPushRect(toWrRect(rect), toWrRect(clip),
                  mColor.r, mColor.g, mColor.b, mColor.a);
}

} // namespace layers
} // namespace mozilla
