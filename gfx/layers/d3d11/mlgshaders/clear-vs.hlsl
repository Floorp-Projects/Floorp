/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common-vs.hlsl"
#include "clear-common.hlsl"

struct VS_CLEAR_IN
{
  float2 vPos : POSITION;
  int4 vRect : TEXCOORD0;
};

// Note: we use slot 2 so we don't have to rebind the layer slot (1) to run
// this shader.
cbuffer ClearConstants : register(b2) {
  int sDepth : packoffset(c0.x);
};

VS_CLEAR_OUT ClearVS(const VS_CLEAR_IN aInput)
{
  float4 rect = float4(aInput.vRect.x, aInput.vRect.y, aInput.vRect.z, aInput.vRect.w);
  float4 screenPos = float4(UnitQuadToRect(aInput.vPos, rect), 0, 1);
  float4 worldPos = mul(WorldTransform, screenPos);
  worldPos.z = ComputeDepth(worldPos, sDepth);

  VS_CLEAR_OUT output;
  output.vPosition = worldPos;
  return output;
}
