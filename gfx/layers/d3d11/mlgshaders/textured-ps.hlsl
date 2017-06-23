/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common.hlsl"
#include "common-ps.hlsl"
#include "textured-common.hlsl"

Texture2D simpleTex : register(ps, t0);

float4 FixRGBOpacity(float4 color, float alpha) {
  return float4(color.rgb * alpha, alpha);
}

// Fast cases that don't require complex clipping.
float4 TexturedQuadRGBA(const VS_SAMPLEOUTPUT_CLIPPED aInput) : SV_Target
{
  return simpleTex.Sample(sSampler, aInput.vTexCoords) * sOpacity;
}
float4 TexturedQuadRGB(const VS_SAMPLEOUTPUT_CLIPPED aInput) : SV_Target
{
  return FixRGBOpacity(simpleTex.Sample(sSampler, aInput.vTexCoords), sOpacity);
}

// PaintedLayer common case.
float4 TexturedVertexRGBA(const VS_SAMPLEOUTPUT aInput) : SV_Target
{
  if (!RectContainsPoint(aInput.vClipRect, aInput.vPosition.xy)) {
    return float4(0, 0, 0, 0);
  }

  float alpha = ReadMask(aInput.vMaskCoords);
  return simpleTex.Sample(sSampler, aInput.vTexCoords) * alpha;
}

// ImageLayers.
float4 TexturedVertexRGB(const VS_SAMPLEOUTPUT aInput) : SV_Target
{
  if (!RectContainsPoint(aInput.vClipRect, aInput.vPosition.xy)) {
    return float4(0, 0, 0, 0);
  }

  float alpha = ReadMask(aInput.vMaskCoords);
  return FixRGBOpacity(simpleTex.Sample(sSampler, aInput.vTexCoords), alpha);
}
