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
  LayerIntRegion visibleRegion = GetVisibleRegion();
  LayerIntRect bounds = visibleRegion.GetBounds();
  Rect rect(0, 0, bounds.width, bounds.height);
  Rect clip;
  if (GetClipRect().isSome()) {
      clip = RelativeToVisible(transform.Inverse().TransformBounds(IntRectToRect(GetClipRect().ref().ToUnknownRect())));
  } else {
      clip = rect;
  }

  gfx::Rect relBounds = VisibleBoundsRelativeToParent();
  if (!transform.IsIdentity()) {
    // WR will only apply the 'translate' of the transform, so we need to do the scale/rotation manually.
    gfx::Matrix4x4 boundTransform = transform;
    boundTransform._41 = 0.0f;
    boundTransform._42 = 0.0f;
    boundTransform._43 = 0.0f;
    relBounds.MoveTo(boundTransform.TransformPoint(relBounds.TopLeft()));
  }

  gfx::Rect overflow(0, 0, relBounds.width, relBounds.height);
  WrMixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetMixBlendMode());

  Maybe<WrImageMask> mask = buildMaskLayer();

  if (gfxPrefs::LayersDump()) {
    printf_stderr("ColorLayer %p using bounds=%s, overflow=%s, transform=%s, rect=%s, clip=%s, mix-blend-mode=%s\n",
                  this->GetLayer(),
                  Stringify(relBounds).c_str(),
                  Stringify(overflow).c_str(),
                  Stringify(transform).c_str(),
                  Stringify(rect).c_str(),
                  Stringify(clip).c_str(),
                  Stringify(mixBlendMode).c_str());
  }

  aBuilder.PushStackingContext(wr::ToWrRect(relBounds),
                              wr::ToWrRect(overflow),
                              mask.ptrOr(nullptr),
                              1.0f,
                              //GetAnimations(),
                              transform,
                              mixBlendMode);
  aBuilder.PushRect(wr::ToWrRect(rect), wr::ToWrRect(clip), wr::ToWrColor(mColor));
  aBuilder.PopStackingContext();
}

} // namespace layers
} // namespace mozilla
