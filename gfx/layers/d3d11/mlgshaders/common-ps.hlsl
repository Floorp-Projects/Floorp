/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common.hlsl"

sampler sSampler : register(ps, s0);
sampler sMaskSampler : register(ps, s1);

Texture2D tMaskTexture : register(ps, t4);

cbuffer MaskInformation : register(b0)
{
  float sOpacity : packoffset(c0.x);
  uint sHasMask : packoffset(c0.y);
};

float2 MaskCoordsToUV(float3 aMaskCoords)
{
  return aMaskCoords.xy / aMaskCoords.z;
}

float ReadMaskWithOpacity(float3 aMaskCoords, float aOpacity)
{
  if (!sHasMask) {
    return aOpacity;
  }

  float2 uv = MaskCoordsToUV(aMaskCoords);
  float r = tMaskTexture.Sample(sMaskSampler, uv).r;
  return min(aOpacity, r);
}

float ReadMask(float3 aMaskCoords)
{
  return ReadMaskWithOpacity(aMaskCoords, sOpacity);
}
