/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/Effects.h"
#include "mozilla/layers/CompositorParent.h"

namespace mozilla {
namespace layers {

/* static */ LayersBackend Compositor::sBackend = LAYERS_NONE;
/* static */ LayersBackend
Compositor::GetBackend()
{
  AssertOnCompositorThread();
  return sBackend;
}

/* static */ void
Compositor::AssertOnCompositorThread()
{
  MOZ_ASSERT(CompositorParent::CompositorLoop() ==
             MessageLoop::current(),
             "Can only call this from the compositor thread!");
}

void
Compositor::DrawDiagnostics(DiagnosticFlags aFlags,
                            const gfx::Rect& rect,
                            const gfx::Rect& aClipRect,
                            const gfx::Matrix4x4& aTransform,
                            const gfx::Point& aOffset)
{
  if (!(mDiagnosticTypes & DIAGNOSTIC_TILE_BORDERS) && (aFlags & DIAGNOSTIC_TILE)) {
    return;
  }

  if (!(mDiagnosticTypes & DIAGNOSTIC_LAYER_BORDERS)) {
    return;
  }

  int lWidth = 2;
  float opacity = 0.7;

  gfx::Color color;
  if (aFlags & DIAGNOSTIC_CONTENT) {
    color = gfx::Color(0.0, 1.0, 0.0, 1.0); // green
    if (aFlags & DIAGNOSTIC_COMPONENT_ALPHA) {
      color = gfx::Color(0.0, 1.0, 1.0, 1.0); // greenish blue
    }
  } else if (aFlags & DIAGNOSTIC_IMAGE) {
    color = gfx::Color(0.5, 0.0, 0.0, 1.0); // red
  } else if (aFlags & DIAGNOSTIC_COLOR) {
    color = gfx::Color(0.0, 0.0, 1.0, 1.0); // blue
  } else if (aFlags & DIAGNOSTIC_CONTAINER) {
    color = gfx::Color(0.8, 0.0, 0.8, 1.0); // purple
  }

  // make tile borders a bit more transparent to keep layer borders readable.
  if (aFlags & DIAGNOSTIC_TILE || aFlags & DIAGNOSTIC_BIGIMAGE) {
    lWidth = 1;
    opacity = 0.5;
    color.r *= 0.7;
    color.g *= 0.7;
    color.b *= 0.7;
  }

  EffectChain effects;

  effects.mPrimaryEffect = new EffectSolidColor(color);
  // left
  this->DrawQuad(gfx::Rect(rect.x, rect.y,
                           lWidth, rect.height),
                 aClipRect, effects, opacity,
                 aTransform, aOffset);
  // top
  this->DrawQuad(gfx::Rect(rect.x + lWidth, rect.y,
                           rect.width - 2 * lWidth, lWidth),
                 aClipRect, effects, opacity,
                 aTransform, aOffset);
  // right
  this->DrawQuad(gfx::Rect(rect.x + rect.width - lWidth, rect.y,
                           lWidth, rect.height),
                 aClipRect, effects, opacity,
                 aTransform, aOffset);
  // bottom
  this->DrawQuad(gfx::Rect(rect.x + lWidth, rect.y + rect.height-lWidth,
                           rect.width - 2 * lWidth, lWidth),
                 aClipRect, effects, opacity,
                 aTransform, aOffset);
}

} // namespace
} // namespace
