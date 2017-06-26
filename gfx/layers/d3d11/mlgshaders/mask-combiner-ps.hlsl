/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common.hlsl"
#include "mask-combiner-common.hlsl"

sampler sSampler;

Texture2D tMaskTexture : register(ps, t0);

float4 MaskCombinerPS(VS_MASKOUTPUT aInput) : SV_Target
{
  float4 value = tMaskTexture.Sample(sSampler, aInput.vTexCoords);
  return float4(value.r, 0, 0, value.r);
}
