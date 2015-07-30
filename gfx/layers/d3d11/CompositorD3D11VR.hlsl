/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* These constants should match mozilla:gfx::VRDistortionConstants */
float4 VREyeToSource : register(vs, c0);
float4 VRDestinationScaleAndOffset : register(vs, c1);

/* This is the input undistorted texture */
Texture2D Texture    : register(ps, t0);

/* This maps to mozilla::gfx::VRDistortionVertex in gfxVR.h.
 * It's shared amongst all of the different rendering types;
 * some might not be in use for a particular distortion effect.
 */
struct VS_VR_INPUT {
  float2 vPosition : POSITION;
  float2 vTexCoord0 : TEXCOORD0;
  float2 vTexCoord1 : TEXCOORD1;
  float2 vTexCoord2 : TEXCOORD2;
  float4 vGenericAttribs : COLOR0;
};

struct VS_VR_OUTPUT {
  float4 vPosition : SV_Position;
  float3 vTexCoord0 : TEXCOORD0;
  float3 vTexCoord1 : TEXCOORD1;
  float3 vTexCoord2 : TEXCOORD2;
  float4 vGenericAttribs : COLOR;
};

SamplerState Linear
{
  Filter = MIN_MAG_MIP_LINEAR;
  AddressU = Clamp;
  AddressV = Clamp;
};

/*
 * Oculus050 basic distortion, with chroma aberration correction
 */
VS_VR_OUTPUT Oculus050VRDistortionVS(const VS_VR_INPUT aVertex)
{
  VS_VR_OUTPUT res;

  float2 tc0 = aVertex.vTexCoord0 * VREyeToSource.zw + VREyeToSource.xy;
  float2 tc1 = aVertex.vTexCoord1 * VREyeToSource.zw + VREyeToSource.xy;
  float2 tc2 = aVertex.vTexCoord2 * VREyeToSource.zw + VREyeToSource.xy;

  //res.vPosition.xy = aVertex.vPosition.xy;
  res.vPosition.xy = aVertex.vPosition.xy * VRDestinationScaleAndOffset.zw + VRDestinationScaleAndOffset.xy;
  res.vPosition.zw = float2(0.5, 1.0);

  res.vTexCoord0 = float3(tc0, 1);
  res.vTexCoord1 = float3(tc1, 1);
  res.vTexCoord2 = float3(tc2, 1);

  res.vGenericAttribs = aVertex.vGenericAttribs;
  
  return res;
}

float4 Oculus050VRDistortionPS(const VS_VR_OUTPUT aVertex) : SV_Target
{
  float resR = Texture.Sample(Linear, aVertex.vTexCoord0.xy).r;
  float resG = Texture.Sample(Linear, aVertex.vTexCoord1.xy).g;
  float resB = Texture.Sample(Linear, aVertex.vTexCoord2.xy).b;

  return float4(resR * aVertex.vGenericAttribs.r,
                resG * aVertex.vGenericAttribs.r,
                resB * aVertex.vGenericAttribs.r,
                1.0);
}
