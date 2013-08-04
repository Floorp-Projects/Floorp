/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ColorLayerD3D9.h"

namespace mozilla {
namespace layers {

Layer*
ColorLayerD3D9::GetLayer()
{
  return this;
}

static void
RenderColorLayerD3D9(ColorLayer* aLayer, LayerManagerD3D9 *aManager)
{
  // XXX we might be able to improve performance by using
  // IDirect3DDevice9::Clear
  if (aManager->CompositingDisabled()) {
    return;
  }

  nsIntRect bounds = aLayer->GetBounds();

  aManager->device()->SetVertexShaderConstantF(
    CBvLayerQuad,
    ShaderConstantRect(bounds.x,
                       bounds.y,
                       bounds.width,
                       bounds.height),
    1);

  const gfx3DMatrix& transform = aLayer->GetEffectiveTransform();
  aManager->device()->SetVertexShaderConstantF(CBmLayerTransform, &transform._11, 4);

  gfxRGBA layerColor(aLayer->GetColor());
  float color[4];
  float opacity = aLayer->GetEffectiveOpacity() * layerColor.a;
  // output color is premultiplied, so we need to adjust all channels.
  // mColor is not premultiplied.
  color[0] = (float)(layerColor.r * opacity);
  color[1] = (float)(layerColor.g * opacity);
  color[2] = (float)(layerColor.b * opacity);
  color[3] = (float)(opacity);

  aManager->device()->SetPixelShaderConstantF(CBvColor, color, 1);

  aManager->SetShaderMode(DeviceManagerD3D9::SOLIDCOLORLAYER,
                          aLayer->GetMaskLayer());

  aManager->device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
}

void
ColorLayerD3D9::RenderLayer()
{
  return RenderColorLayerD3D9(this, mD3DManager);
}

} /* layers */
} /* mozilla */
