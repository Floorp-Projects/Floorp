/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "common-vs.hlsl"
#include "color-common.hlsl"

struct VS_INPUT {
  float2 vPos : POSITION;
};

cbuffer Buffer0 : register(b0) {
  float4 aValue0;
};
cbuffer Buffer1 : register(b1) {
  float4 aValue1;
};
cbuffer Buffer2 : register(b2) {
  float4 aValue2;
};

VS_COLOROUTPUT_CLIPPED TestConstantBuffersVS(VS_INPUT aInput)
{
  // Draw to the entire viewport.
  float2 pos = UnitQuadToRect(aInput.vPos, float4(-1, -1, 2, 2));

  VS_COLOROUTPUT_CLIPPED output;
  output.vPosition = float4(pos, 0, 1);
  output.vColor = float4(aValue0.r, aValue1.g, aValue2.b, 1.0);
  return output;
}
