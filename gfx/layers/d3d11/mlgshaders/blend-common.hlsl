/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common-vs.hlsl"

struct VS_BLEND_OUTPUT
{
  float4 vPosition : SV_Position;
  float2 vTexCoords : TEXCOORD0;
  float2 vBackdropCoords : TEXCOORD1;
  float2 vLocalPos : TEXCOORD2;
  float3 vMaskCoords : TEXCOORD3;
  nointerpolation float4 vClipRect : TEXCOORD4;
};
