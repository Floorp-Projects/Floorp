/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BlendShaderConstants.h"

typedef float4 rect;

float4x4 mLayerTransform : register(vs, c0);
float4x4 mProjection : register(vs, c4);
float4 vRenderTargetOffset : register(vs, c8);
rect vTextureCoords : register(vs, c9);
rect vLayerQuad : register(vs, c10);

float4 fLayerColor : register(ps, c0);
float fLayerOpacity : register(ps, c1);

// x = layer type
// y = mask type
// z = blend op
// w = is premultiplied

float fCoefficient : register(ps, c3);

row_major float3x3 mYuvColorMatrix : register(ps, c4);

sampler sSampler : register(ps, s0);

// The mix-blend mega shader uses all variables, so we have to make sure they
// are assigned fixed slots.
Texture2D tRGB : register(ps, t0);
Texture2D tY : register(ps, t1);
Texture2D tCb : register(ps, t2);
Texture2D tCr : register(ps, t3);

struct VS_INPUT {
  float2 vPosition : POSITION;
};

struct VS_TEX_INPUT {
  float2 vPosition : POSITION;
  float2 vTexCoords : TEXCOORD0;
};

struct VS_OUTPUT {
  float4 vPosition : SV_Position;
  float2 vTexCoords : TEXCOORD0;
};

struct PS_OUTPUT {
  float4 vSrc;
  float4 vAlpha;
};

float2 TexCoords(const float2 aPosition)
{
  float2 result;
  const float2 size = vTextureCoords.zw;
  result.x = vTextureCoords.x + aPosition.x * size.x;
  result.y = vTextureCoords.y + aPosition.y * size.y;

  return result;
}

SamplerState LayerTextureSamplerLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

float4 TransformedPosition(float2 aInPosition)
{
  // the current vertex's position on the quad
  // [x,y,0,1] is mandated by the CSS Transforms spec as the point value to transform
  float4 position = float4(0, 0, 0, 1);

  // We use 4 component floats to uniquely describe a rectangle, by the structure
  // of x, y, width, height. This allows us to easily generate the 4 corners
  // of any rectangle from the 4 corners of the 0,0-1,1 quad that we use as the
  // stream source for our LayerQuad vertex shader. We do this by doing:
  // Xout = x + Xin * width
  // Yout = y + Yin * height
  float2 size = vLayerQuad.zw;
  position.x = vLayerQuad.x + aInPosition.x * size.x;
  position.y = vLayerQuad.y + aInPosition.y * size.y;

  position = mul(mLayerTransform, position);

  return position;
}

float4 VertexPosition(float4 aTransformedPosition)
{
  float4 result;
  result.w = aTransformedPosition.w;
  result.xyz = aTransformedPosition.xyz / aTransformedPosition.w;
  result -= vRenderTargetOffset;
  result.xyz *= result.w;

  result = mul(mProjection, result);

  return result;
}

VS_OUTPUT LayerQuadVS(const VS_INPUT aVertex)
{
  VS_OUTPUT outp;
  float4 position = TransformedPosition(aVertex.vPosition);

  outp.vPosition = VertexPosition(position);
  outp.vTexCoords = TexCoords(aVertex.vPosition.xy);

  return outp;
}

/* From Rec601:
[R]   [1.1643835616438356,  0.0,                 1.5960267857142858]      [ Y -  16]
[G] = [1.1643835616438358, -0.3917622900949137, -0.8129676472377708]    x [Cb - 128]
[B]   [1.1643835616438356,  2.017232142857143,   8.862867620416422e-17]   [Cr - 128]

For [0,1] instead of [0,255], and to 5 places:
[R]   [1.16438,  0.00000,  1.59603]   [ Y - 0.06275]
[G] = [1.16438, -0.39176, -0.81297] x [Cb - 0.50196]
[B]   [1.16438,  2.01723,  0.00000]   [Cr - 0.50196]

From Rec709:
[R]   [1.1643835616438356,  4.2781193979771426e-17, 1.7927410714285714]     [ Y -  16]
[G] = [1.1643835616438358, -0.21324861427372963,   -0.532909328559444]    x [Cb - 128]
[B]   [1.1643835616438356,  2.1124017857142854,     0.0]                    [Cr - 128]

For [0,1] instead of [0,255], and to 5 places:
[R]   [1.16438,  0.00000,  1.79274]   [ Y - 0.06275]
[G] = [1.16438, -0.21325, -0.53291] x [Cb - 0.50196]
[B]   [1.16438,  2.11240,  0.00000]   [Cr - 0.50196]
*/
float4 CalculateYCbCrColor(const float2 aTexCoords)
{
  float3 yuv = float3(
    tY.Sample(sSampler, aTexCoords).r,
    tCb.Sample(sSampler, aTexCoords).r,
    tCr.Sample(sSampler, aTexCoords).r);
  yuv = yuv * fCoefficient - float3(0.06275, 0.50196, 0.50196);

  return float4(mul(mYuvColorMatrix, yuv), 1.0);
}

float4 CalculateNV12Color(const float2 aTexCoords)
{
  float3 yuv = float3(
    tY.Sample(sSampler, aTexCoords).r,
    tCb.Sample(sSampler, aTexCoords).r,
    tCb.Sample(sSampler, aTexCoords).g);
  yuv = yuv * fCoefficient - float3(0.06275, 0.50196, 0.50196);

  return float4(mul(mYuvColorMatrix, yuv), 1.0);
}

float4 SolidColorShader(const VS_OUTPUT aVertex) : SV_Target
{
  return fLayerColor;
}

float4 RGBAShader(const VS_OUTPUT aVertex) : SV_Target
{
  return tRGB.Sample(sSampler, aVertex.vTexCoords) * fLayerOpacity;
}

float4 RGBShader(const VS_OUTPUT aVertex) : SV_Target
{
  float4 result;
  result = tRGB.Sample(sSampler, aVertex.vTexCoords) * fLayerOpacity;
  result.a = fLayerOpacity;
  return result;
}

float4 YCbCrShader(const VS_OUTPUT aVertex) : SV_Target
{
  return CalculateYCbCrColor(aVertex.vTexCoords) * fLayerOpacity;
}

float4 NV12Shader(const VS_OUTPUT aVertex) : SV_Target
{
  return CalculateNV12Color(aVertex.vTexCoords) * fLayerOpacity;
}
