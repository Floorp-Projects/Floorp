/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ColorLayerComposite.h"
#include "mozilla/RefPtr.h"                  // for RefPtr
#include "mozilla/gfx/Matrix.h"              // for Matrix4x4
#include "mozilla/gfx/Point.h"               // for Point
#include "mozilla/gfx/Rect.h"                // for Rect
#include "mozilla/gfx/Types.h"               // for Color
#include "mozilla/layers/Compositor.h"       // for Compositor
#include "mozilla/layers/CompositorTypes.h"  // for DiagnosticFlags::COLOR
#include "mozilla/layers/Effects.h"          // for Effect, EffectChain, etc
#include "mozilla/mozalloc.h"                // for operator delete, etc

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void ColorLayerComposite::RenderLayer(const gfx::IntRect& aClipRect,
                                      const Maybe<gfx::Polygon>& aGeometry) {
  const Matrix4x4& transform = GetEffectiveTransform();

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  // On desktop we want to draw a single rectangle to avoid possible
  // seams if we're resampling. On mobile we'd prefer to use the accurate
  // region for better performance.
  LayerIntRegion drawRegion = GetLocalVisibleRegion();
#else
  LayerIntRegion drawRegion = ViewAs<LayerPixel>(GetBounds());
#endif

  for (auto iter = drawRegion.RectIter(); !iter.Done(); iter.Next()) {
    const LayerIntRect& rect = iter.Get();
    Rect graphicsRect(rect.X(), rect.Y(), rect.Width(), rect.Height());

    RenderWithAllMasks(this, mCompositor, aClipRect,
                       [&](EffectChain& effectChain, const IntRect& clipRect) {
                         GenEffectChain(effectChain);

                         mCompositor->DrawGeometry(
                             graphicsRect, clipRect, effectChain,
                             GetEffectiveOpacity(), transform, aGeometry);
                       });
  }

  Rect rect(GetBounds());
  mCompositor->DrawDiagnostics(DiagnosticFlags::COLOR, rect, aClipRect,
                               transform);
}

void ColorLayerComposite::GenEffectChain(EffectChain& aEffect) {
  aEffect.mLayerRef = this;
  aEffect.mPrimaryEffect = new EffectSolidColor(GetColor());
}

}  // namespace layers
}  // namespace mozilla
