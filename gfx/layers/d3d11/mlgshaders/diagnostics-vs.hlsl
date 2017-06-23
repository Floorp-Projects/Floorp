/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common-vs.hlsl"
#include "textured-common.hlsl"
#include "diagnostics-common.hlsl"

struct VS_DIAGINPUT
{
  float2 vPos : POSITION;
  float4 vRect : TEXCOORD0;
  float4 vTexCoords : TEXCOORD1;
};

VS_DIAGOUTPUT DiagnosticTextVS(const VS_DIAGINPUT aInput)
{
  float2 pos = UnitQuadToRect(aInput.vPos, aInput.vRect);
  float2 texCoord = UnitQuadToRect(aInput.vPos, aInput.vTexCoords);

  VS_DIAGOUTPUT output;
  output.vPosition = mul(WorldTransform, float4(pos, 0, 1));
  output.vTexCoord = texCoord;
  return output;
}
