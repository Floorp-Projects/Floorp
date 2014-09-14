/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
  * This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Texture2D InputTexture : register(t0);
SamplerState InputSampler : register(s0);
Texture2D GradientTexture : register(t1);
SamplerState GradientSampler : register(s1);

cbuffer constants : register(b0)
{
    // Precalculate as much as we can!
    float3 diff : packoffset(c0.x);
    float2 center1 : packoffset(c1.x);
    float A : packoffset(c1.z);
    float radius1 : packoffset(c1.w);
    float sq_radius1 : packoffset(c2.x);

    // The next two values are used for a hack to compensate for an apparent
    // bug in D2D where the GradientSampler SamplerState doesn't get the
    // correct addressing modes.
    float repeat_correct : packoffset(c2.y);
    float allow_odd : packoffset(c2.z);

    float3x2 transform : packoffset(c3.x);
}

float4 SampleRadialGradientPS(
    float4 clipSpaceOutput  : SV_POSITION,
    float4 sceneSpaceOutput : SCENE_POSITION,
    float4 texelSpaceInput0 : TEXCOORD0
    ) : SV_Target
{
  // Radial gradient painting is defined as the set of circles whose centers
  // are described by C(t) = (C2 - C1) * t + C1; with radii
  // R(t) = (R2 - R1) * t + R1; for R(t) > 0. This shader solves the
  // quadratic equation that arises when calculating t for pixel (x, y).
  //
  // A more extensive derrivation can be found in the pixman radial gradient
  // code.

  float2 p = float2(sceneSpaceOutput.x * transform._11 + sceneSpaceOutput.y * transform._21 + transform._31,
                    sceneSpaceOutput.x * transform._12 + sceneSpaceOutput.y * transform._22 + transform._32);
  float3 dp = float3(p - center1, radius1);

  // dpx * dcx + dpy * dcy + r * dr
  float B = dot(dp, diff);

  float C = pow(dp.x, 2) + pow(dp.y, 2) - sq_radius1;

  float det = pow(B, 2) - A * C;

  float sqrt_det = sqrt(abs(det));

  float2 t = (B + float2(sqrt_det, -sqrt_det)) / A;

  float2 isValid = step(float2(-radius1, -radius1), t * diff.z);

  float upper_t = lerp(t.y, t.x, isValid.x);

  // Addressing mode bug work-around.. first let's see if we should consider odd repetitions separately.
  float oddeven = abs(fmod(floor(upper_t), 2)) * allow_odd;

  // Now let's calculate even or odd addressing in a branchless manner.
  float upper_t_repeated = ((upper_t - floor(upper_t)) * (1.0f - oddeven)) + ((ceil(upper_t) - upper_t) * oddeven);

  float4 output = GradientTexture.Sample(GradientSampler, float2(upper_t * (1.0f - repeat_correct) + upper_t_repeated * repeat_correct, 0.5));
  // Premultiply
  output.rgb *= output.a;
  // Multiply the output color by the input mask for the operation.
  output *= InputTexture.Sample(InputSampler, texelSpaceInput0.xy);

  // In order to compile for PS_4_0_level_9_3 we need to be branchless.
  // This is essentially returning nothing, i.e. bailing early if:
  // det < 0 || max(isValid.x, isValid.y) <= 0
  return output * abs(step(max(isValid.x, isValid.y), 0) - 1.0f) * step(0, det);
};

float4 SampleRadialGradientA0PS(
    float4 clipSpaceOutput  : SV_POSITION,
    float4 sceneSpaceOutput : SCENE_POSITION,
    float4 texelSpaceInput0 : TEXCOORD0
    ) : SV_Target
{
  // This simpler shader is used for the degenerate case where A is 0,
  // i.e. we're actually solving a linear equation.

  float2 p = float2(sceneSpaceOutput.x * transform._11 + sceneSpaceOutput.y * transform._21 + transform._31,
                    sceneSpaceOutput.x * transform._12 + sceneSpaceOutput.y * transform._22 + transform._32);
  float3 dp = float3(p - center1, radius1);

  // dpx * dcx + dpy * dcy + r * dr
  float B = dot(dp, diff);

  float C = pow(dp.x, 2) + pow(dp.y, 2) - pow(radius1, 2);

  float t = 0.5 * C / B;

  // Addressing mode bug work-around.. first let's see if we should consider odd repetitions separately.
  float oddeven = abs(fmod(floor(t), 2)) * allow_odd;

  // Now let's calculate even or odd addressing in a branchless manner.
  float t_repeated = ((t - floor(t)) * (1.0f - oddeven)) + ((ceil(t) - t) * oddeven);

  float4 output = GradientTexture.Sample(GradientSampler, float2(t * (1.0f - repeat_correct) + t_repeated * repeat_correct, 0.5));
  // Premultiply
  output.rgb *= output.a;
  // Multiply the output color by the input mask for the operation.
  output *= InputTexture.Sample(InputSampler, texelSpaceInput0.xy);

  // In order to compile for PS_4_0_level_9_3 we need to be branchless.
  // This is essentially returning nothing, i.e. bailing early if:
  // -radius1 >= t * diff.z
  return output * abs(step(t * diff.z, -radius1) - 1.0f);
};

