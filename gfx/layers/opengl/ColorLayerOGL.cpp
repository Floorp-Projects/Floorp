/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ColorLayerOGL.h"

namespace mozilla {
namespace layers {

static void
RenderColorLayer(ColorLayer* aLayer, LayerManagerOGL *aManager,
                 const nsIntPoint& aOffset)
{
  if (aManager->CompositingDisabled()) {
    return;
  }

  aManager->MakeCurrent();

  // XXX we might be able to improve performance by using glClear

  nsIntRect visibleRect = aLayer->GetEffectiveVisibleRegion().GetBounds();

  /* Multiply color by the layer opacity, as the shader
   * ignores layer opacity and expects a final color to
   * write to the color buffer.  This saves a needless
   * multiply in the fragment shader.
   */
  gfxRGBA color(aLayer->GetColor());
  float opacity = aLayer->GetEffectiveOpacity() * color.a;
  color.r *= opacity;
  color.g *= opacity;
  color.b *= opacity;
  color.a = opacity;

  ShaderProgramOGL *program = aManager->GetProgram(gl::ColorLayerProgramType,
                                                   aLayer->GetMaskLayer());
  program->Activate();
  program->SetLayerQuadRect(visibleRect);
  program->SetLayerTransform(aLayer->GetEffectiveTransform());
  program->SetRenderOffset(aOffset);
  program->SetRenderColor(color);
  program->LoadMask(aLayer->GetMaskLayer());

  aManager->BindAndDrawQuad(program);
}

void
ColorLayerOGL::RenderLayer(int,
                           const nsIntPoint& aOffset)
{
  RenderColorLayer(this, mOGLManager, aOffset);
}

} /* layers */
} /* mozilla */
