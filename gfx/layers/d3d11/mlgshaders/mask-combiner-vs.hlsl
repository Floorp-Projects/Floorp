/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common-vs.hlsl"
#include "mask-combiner-common.hlsl"

struct VS_MASKINPUT
{
  // Note, the input is 
  float2 vPos : POSITION;
  float4 vTexCoords : POSITION1;
};

VS_MASKOUTPUT MaskCombinerVS(VS_MASKINPUT aInput)
{
  float4 position = float4(
    aInput.vPos.x * 2.0f - 1.0f,
    1.0f - (aInput.vPos.y * 2.0f),
    0, 1);

  VS_MASKOUTPUT output;
  output.vPosition = position;
  output.vTexCoords = UnitQuadToRect(aInput.vPos, aInput.vTexCoords);
  return output;
}
