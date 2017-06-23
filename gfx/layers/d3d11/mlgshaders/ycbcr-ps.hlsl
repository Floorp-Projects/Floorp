/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "common-ps.hlsl"
#include "textured-common.hlsl"

Texture2D tY : register(ps, t0);
Texture2D tCb : register(ps, t1);
Texture2D tCr : register(ps, t2);

cbuffer YCbCrBuffer : register(b1) {
  row_major float3x3 YuvColorMatrix;
};

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
float4 CalculateYCbCrColor(float3 rgb)
{
  return float4(
    mul(YuvColorMatrix,
        float3(
          rgb.r - 0.06275,
          rgb.g - 0.50196,
          rgb.b - 0.50196)),
    1.0);
}

float4 CalculateIMC4Color(const float2 aTexCoords)
{
  float3 yuv = float3(
    tY.Sample(sSampler, aTexCoords).r,
    tCb.Sample(sSampler, aTexCoords).r,
    tCr.Sample(sSampler, aTexCoords).r);
  return CalculateYCbCrColor(yuv);
}

float4 CalculateNV12Color(const float2 aTexCoords)
{
  float y = tY.Sample(sSampler, aTexCoords).r;
  float2 cbcr = tCb.Sample(sSampler, aTexCoords).rg;
  return CalculateYCbCrColor(float3(y, cbcr));
}

float4 TexturedQuadIMC4(const VS_SAMPLEOUTPUT_CLIPPED aInput) : SV_Target
{
  return CalculateIMC4Color(aInput.vTexCoords) * sOpacity;
}

float4 TexturedQuadNV12(const VS_SAMPLEOUTPUT_CLIPPED aInput) : SV_Target
{
  return CalculateNV12Color(aInput.vTexCoords) * sOpacity;
}

float4 TexturedVertexIMC4(const VS_SAMPLEOUTPUT aInput) : SV_Target
{
  if (!RectContainsPoint(aInput.vClipRect, aInput.vPosition.xy)) {
    return float4(0, 0, 0, 0);
  }

  float alpha = ReadMask(aInput.vMaskCoords);
  return CalculateIMC4Color(aInput.vTexCoords) * alpha;
}

float4 TexturedVertexNV12(const VS_SAMPLEOUTPUT aInput) : SV_Target
{
  if (!RectContainsPoint(aInput.vClipRect, aInput.vPosition.xy)) {
    return float4(0, 0, 0, 0);
  }

  float alpha = ReadMask(aInput.vMaskCoords);
  return CalculateNV12Color(aInput.vTexCoords) * alpha;
}
