/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

struct VS_COLOROUTPUT
{
  nointerpolation float4 vColor : COLOR0;
  nointerpolation float4 vClipRect : TEXCOORD0;
  float4 vPosition : SV_Position;
  float2 vLocalPos : TEXCOORD1;
  float3 vMaskCoords : TEXCOORD2;
};

struct VS_COLOROUTPUT_CLIPPED
{
  float4 vPosition : SV_Position;
  nointerpolation float4 vColor : COLOR0;
};
