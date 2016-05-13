/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ColorLayerComposite.h"
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Color
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorTypes.h"  // for DiagnosticFlags::COLOR
#include "mozilla/layers/Effects.h"     // for Effect, EffectChain, etc
#include "mozilla/mozalloc.h"           // for operator delete, etc

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
ColorLayerComposite::RenderLayer(const IntRect& aClipRect)
{
  Rect rect(GetBounds());
  const Matrix4x4& transform = GetEffectiveTransform();

  RenderWithAllMasks(this, mCompositor, aClipRect,
                     [&](EffectChain& effectChain, const Rect& clipRect) {
    GenEffectChain(effectChain);
    mCompositor->DrawQuad(rect, clipRect, effectChain, GetEffectiveOpacity(),
                          transform);
  });

  mCompositor->DrawDiagnostics(DiagnosticFlags::COLOR, rect, Rect(aClipRect),
                               transform);
}

void
ColorLayerComposite::GenEffectChain(EffectChain& aEffect)
{
  aEffect.mLayerRef = this;
  aEffect.mPrimaryEffect = new EffectSolidColor(GetColor());
}

} // namespace layers
} // namespace mozilla
