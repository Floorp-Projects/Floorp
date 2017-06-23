/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common.hlsl"
#include "common-ps.hlsl"
#include "textured-common.hlsl"

Texture2D texOnBlack : register(ps, t0);
Texture2D texOnWhite : register(ps, t1);

struct PS_OUTPUT {
  float4 vSrc;
  float4 vAlpha;
};

PS_OUTPUT ComponentAlphaQuadPS(const VS_SAMPLEOUTPUT_CLIPPED aInput) : SV_Target
{
  PS_OUTPUT result;
  result.vSrc = texOnBlack.Sample(sSampler, aInput.vTexCoords);
  result.vAlpha = 1.0 - texOnWhite.Sample(sSampler, aInput.vTexCoords) + result.vSrc;
  result.vSrc.a = result.vAlpha.g;
  result.vSrc *= sOpacity;
  result.vAlpha *= sOpacity;
  return result;
}

PS_OUTPUT ComponentAlphaVertexPS(const VS_SAMPLEOUTPUT aInput) : SV_Target
{
  PS_OUTPUT result;
  if (!RectContainsPoint(aInput.vClipRect, aInput.vPosition.xy)) {
    result.vSrc = float4(0, 0, 0, 0);
    result.vAlpha = float4(0, 0, 0, 0);
    return result;
  }

  float alpha = ReadMask(aInput.vMaskCoords);

  result.vSrc = texOnBlack.Sample(sSampler, aInput.vTexCoords);
  result.vAlpha = 1.0 - texOnWhite.Sample(sSampler, aInput.vTexCoords) + result.vSrc;
  result.vSrc.a = result.vAlpha.g;
  result.vSrc *= alpha;
  result.vAlpha *= alpha;
  return result;
}
