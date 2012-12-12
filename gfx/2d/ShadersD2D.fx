// We store vertex coordinates and the quad shape in a constant buffer, this is
// easy to update and allows us to use a single call to set the x, y, w, h of
// the quad.
// The QuadDesc and TexCoords both work as follows:
// The x component is the quad left point, the y component is the top point
// the z component is the width, and the w component is the height. The quad
// are specified in viewport coordinates, i.e. { -1.0f, 1.0f, 2.0f, -2.0f }
// would cover the entire viewport (which runs from <-1.0f, 1.0f> left to right
// and <-1.0f, 1.0f> -bottom- to top. The TexCoords desc is specified in texture
// space <0, 1.0f> left to right and top to bottom. The input vertices of the
// shader stage always form a rectangle from {0, 0} - {1, 1}
cbuffer cb0
{
    float4 QuadDesc;
    float4 TexCoords;
    float4 MaskTexCoords;
    float4 TextColor;
}

cbuffer cb1
{
    float4 BlurOffsetsH[3];
    float4 BlurOffsetsV[3];
    float4 BlurWeights[3];
    float4 ShadowColor;
}

cbuffer cb2
{
    float3x3 DeviceSpaceToUserSpace;
    float2 dimensions;
    // Precalculate as much as we can!
    float3 diff;
    float2 center1;
    float A;
    float radius1;
    float sq_radius1;
}

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
    float2 MaskTexCoord : TEXCOORD1;
};

struct VS_RADIAL_OUTPUT
{
    float4 Position : SV_Position;
    float2 MaskTexCoord : TEXCOORD0;
    float2 PixelCoord : TEXCOORD1;
};

struct PS_TEXT_OUTPUT
{
    float4 color;
    float4 alpha;
};

Texture2D tex;
Texture2D bcktex;
Texture2D mask;
uint blendop;

sampler sSampler = sampler_state {
    Filter = MIN_MAG_MIP_LINEAR;
    Texture = tex;
    AddressU = Clamp;
    AddressV = Clamp;
};

sampler sBckSampler = sampler_state {
    Filter = MIN_MAG_MIP_LINEAR;
    Texture = bcktex;
    AddressU = Clamp;
    AddressV = Clamp;
};

sampler sWrapSampler = sampler_state {
    Filter = MIN_MAG_MIP_LINEAR;
    Texture = tex;
    AddressU = Wrap;
    AddressV = Wrap;
};

sampler sMirrorSampler = sampler_state {
    Filter = MIN_MAG_MIP_LINEAR;
    Texture = tex;
    AddressU = Mirror;
    AddressV = Mirror;
};

sampler sMaskSampler = sampler_state {
    Filter = MIN_MAG_MIP_LINEAR;
    Texture = mask;
    AddressU = Clamp;
    AddressV = Clamp;
};

sampler sShadowSampler = sampler_state {
    Filter = MIN_MAG_MIP_LINEAR;
    Texture = tex;
    AddressU = Border;
    AddressV = Border;
    BorderColor = float4(0, 0, 0, 0);
};

RasterizerState TextureRast
{
  ScissorEnable = True;
  CullMode = None;
};

BlendState ShadowBlendH
{
  BlendEnable[0] = False;
  RenderTargetWriteMask[0] = 0xF;
};

BlendState ShadowBlendV
{
  BlendEnable[0] = True;
  SrcBlend = One;
  DestBlend = Inv_Src_Alpha;
  BlendOp = Add;
  SrcBlendAlpha = One;
  DestBlendAlpha = Inv_Src_Alpha;
  BlendOpAlpha = Add;
  RenderTargetWriteMask[0] = 0xF;
};

BlendState bTextBlend
{
  AlphaToCoverageEnable = FALSE;
  BlendEnable[0] = TRUE;
  SrcBlend = Src1_Color;
  DestBlend = Inv_Src1_Color;
  BlendOp = Add;
  SrcBlendAlpha = Src1_Alpha;
  DestBlendAlpha = Inv_Src1_Alpha;
  BlendOpAlpha = Add;
  RenderTargetWriteMask[0] = 0x0F; // All
};

VS_OUTPUT SampleTextureVS(float3 pos : POSITION)
{
    VS_OUTPUT Output;
    Output.Position.w = 1.0f;
    Output.Position.x = pos.x * QuadDesc.z + QuadDesc.x;
    Output.Position.y = pos.y * QuadDesc.w + QuadDesc.y;
    Output.Position.z = 0;
    Output.TexCoord.x = pos.x * TexCoords.z + TexCoords.x;
    Output.TexCoord.y = pos.y * TexCoords.w + TexCoords.y;
    Output.MaskTexCoord.x = pos.x * MaskTexCoords.z + MaskTexCoords.x;
    Output.MaskTexCoord.y = pos.y * MaskTexCoords.w + MaskTexCoords.y;
    return Output;
}

VS_RADIAL_OUTPUT SampleRadialVS(float3 pos : POSITION)
{
    VS_RADIAL_OUTPUT Output;
    Output.Position.w = 1.0f;
    Output.Position.x = pos.x * QuadDesc.z + QuadDesc.x;
    Output.Position.y = pos.y * QuadDesc.w + QuadDesc.y;
    Output.Position.z = 0;
    Output.MaskTexCoord.x = pos.x * MaskTexCoords.z + MaskTexCoords.x;
    Output.MaskTexCoord.y = pos.y * MaskTexCoords.w + MaskTexCoords.y;

    // For the radial gradient pixel shader we need to pass in the pixel's
    // coordinates in user space for the color to be correctly determined.

    Output.PixelCoord.x = ((Output.Position.x + 1.0f) / 2.0f) * dimensions.x;
    Output.PixelCoord.y = ((1.0f - Output.Position.y) / 2.0f) * dimensions.y;
    Output.PixelCoord.xy = mul(float3(Output.PixelCoord.x, Output.PixelCoord.y, 1.0f), DeviceSpaceToUserSpace).xy;
    return Output;
}

float Screen(float a, float b)
{
  return 1 - ((1 - a)*(1 - b));
}

static float RedLuminance = 0.3f;
static float GreenLuminance = 0.59f;
static float BlueLuminance = 0.11f;

float Lum(float3 C)
{
  return RedLuminance * C.r + GreenLuminance * C.g + BlueLuminance * C.b;
}

float3 ClipColor(float3 C)
{
  float L = Lum(C);
  float n = min(min(C.r, C.g), C.b);
  float x = max(max(C.r, C.g), C.b);

  if(n < 0)
    C = L + (((C - L) * L) / (L - n));

  if(x > 1)
    C = L + ((C - L) * (1 - L) / (x - L));

  return C;
}

float3 SetLum(float3 C, float l)
{
  float d = l - Lum(C);
  C = C + d;
  return ClipColor(C);
}

float Sat(float3 C)
{
  return max(C.r, max(C.g, C.b)) - min(C.r, min(C.g, C.b));
}

void SetSatComponents(inout float minComp, inout float midComp, inout float maxComp, in float satVal)
{
  midComp -= minComp;
  maxComp -= minComp;
  minComp = 0.0;
  if (maxComp > 0.0)
  {
    midComp *= satVal/maxComp;
    maxComp = satVal;
  }
}

float3 SetSat(float3 color, in float satVal)
{
  if (color.x <= color.y) {
    if (color.y <= color.z) {
      // x <= y <= z
      SetSatComponents(color.x, color.y, color.z, satVal);
    }
    else {
      if (color.x <= color.z) {
        // x <= z <= y
        SetSatComponents(color.x, color.z, color.y, satVal);
      }
      else {
        // z <= x <= y
        SetSatComponents(color.z, color.x, color.y, satVal);
      }
    }
  }
  else {
    if (color.x <= color.z) {
      // y <= x <= z
      SetSatComponents(color.y, color.x, color.z, satVal);
    }
    else {
      if (color.y <= color.z) {
        // y <= z <= x
        SetSatComponents(color.y, color.z, color.x, satVal);
      }
      else {
        // z <= y <= x
        SetSatComponents(color.z, color.y, color.x, satVal);
      }
    }
  }

  return color;
}

float4 SampleBlendTextureSeparablePS_1( VS_OUTPUT In) : SV_Target
{
  float4 output = tex.Sample(sSampler, In.TexCoord);
  float4 background = bcktex.Sample(sBckSampler, In.TexCoord);
  if((output.a == 0) || (background.a == 0))
	return output;

  output.rgb /= output.a;
  background.rgb /= background.a;

  float4 retval = output;

  if(blendop == 1) { // multiply
      retval.rgb = output.rgb * background.rgb;
  } else if(blendop == 2) {
      retval.rgb = output.rgb + background.rgb - output.rgb * background.rgb;
  } else if(blendop == 3) {
    if(background.r <= 0.5)
      retval.r  = 2*background.r * output.r;
    else
      retval.r  = Screen(output.r, 2 * background.r - 1);
    if(background.g <= 0.5)
      retval.g  = 2 * background.g * output.g;
    else
      retval.g  = Screen(output.g, 2 * background.g - 1);
    if(background.b <= 0.5)
      retval.b  = 2 * background.b * output.b;
    else
      retval.b  = Screen(output.b, 2 * background.b - 1);
  } else if(blendop == 4) {
      retval.rgb = min(output.rgb, background.rgb);
  } else if(blendop == 5) {
    retval.rgb = max(output.rgb, background.rgb);
  } else {
    if(output.r > 0)
      retval.r = 1 - min(1, (1 - background.r) / output.r);
    else
      retval.r = background.r ? 1 : 0;
    if(output.g > 0)
       retval.g = 1 - min(1, (1 - background.g) / output.g);
    else
      retval.g = background.g ? 1 : 0;
    if(output.b > 0)
     retval.b = 1 - min(1, (1 - background.b) / output.b);
    else
      retval.b = background.b ? 1 : 0;
  }

  output.rgb = ((1 - background.a) * output.rgb + background.a * retval.rgb) * output.a;
  return output;
}

float4 SampleBlendTextureSeparablePS_2( VS_OUTPUT In) : SV_Target
{
  float4 output = tex.Sample(sSampler, In.TexCoord);
  float4 background = bcktex.Sample(sBckSampler, In.TexCoord);
  if((output.a == 0) || (background.a == 0))
	return output;

  output.rgb /= output.a;
  background.rgb /= background.a;

  float4 retval = output;

  if(blendop == 7) {
    if(output.r > 0)
      retval.r = 1 - min(1, (1 - background.r) / output.r);
    else
      retval.r = background.r ? 1 : 0;
    if(output.g > 0)
      retval.g = 1 - min(1, (1 - background.g) / output.g);
    else
      retval.g = background.g ? 1 : 0;
    if(output.b > 0)
      retval.b = 1 - min(1, (1 - background.b) / output.b);
    else
      retval.b = background.b ? 1 : 0;
  } else if(blendop == 8) {
    if(output.r <= 0.5)
      retval.r = 2 * output.r * background.r;
	else
      retval.r = Screen(background.r, 2 * output.r -1);
    if(output.g <= 0.5)
      retval.g = 2 * output.g * background.g;
    else
      retval.g = Screen(background.g, 2 * output.g -1);
    if(output.b <= 0.5)
      retval.b = 2 * output.b * background.b;
    else
      retval.b = Screen(background.b, 2 * output.b -1);
  } else if(blendop == 9){
    float D;
    if(background.r <= 0.25)
      D = ((16 * background.r - 12) * background.r + 4) * background.r;
    else
      D = sqrt(background.r);
    if(output.r <= 0.5)
      retval.r = background.r - (1 - 2 * output.r) * background.r * (1 - background.r);
    else
      retval.r = background.r + (2 * output.r - 1) * (D - background.r);

    if(background.g <= 0.25)
      D = ((16 * background.g - 12) * background.g + 4) * background.g;
    else
      D = sqrt(background.g);
    if(output.g <= 0.5)
      retval.g = background.g - (1 - 2 * output.g) * background.g * (1 - background.g);
    else
      retval.g = background.g + (2 * output.g - 1) * (D - background.g);

    if(background.b <= 0.25)
      D = ((16 * background.b - 12) * background.b + 4) * background.b;
    else
      D = sqrt(background.b);

    if(output.b <= 0.5)
      retval.b = background.b - (1 - 2 * output.b) * background.b * (1 - background.b);
    else
      retval.b = background.b + (2 * output.b - 1) * (D - background.b);
  } else if(blendop == 10) {
    retval.rgb = abs(output.rgb - background.rgb);
  } else {
    retval.rgb = output.rgb + background.rgb - 2 * output.rgb * background.rgb;
  }

  output.rgb = ((1 - background.a) * output.rgb + background.a * retval.rgb) * output.a;
  return output;
}

float4 SampleBlendTextureNonSeparablePS( VS_OUTPUT In) : SV_Target
{
  float4 output = tex.Sample(sSampler, In.TexCoord);
  float4 background = bcktex.Sample(sBckSampler, In.TexCoord);
  if((output.a == 0) || (background.a == 0))
	return output;

  output.rgb /= output.a;
  background.rgb /= background.a;

  float4 retval = output;

  if(blendop == 12) {
    retval.rgb = SetLum(SetSat(output.rgb, Sat(background.rgb)), Lum(background.rgb));
  } else if(blendop == 13) {
    retval.rgb = SetLum(SetSat(background.rgb, Sat(output.rgb)), Lum(background.rgb));
  } else if(blendop == 14) {
    retval.rgb = SetLum(output.rgb, Lum(background.rgb));
  } else {
    retval.rgb = SetLum(background.rgb, Lum(output.rgb));
  }

  output.rgb = ((1 - background.a) * output.rgb + background.a * retval.rgb) * output.a;
  return output;
}


float4 SampleTexturePS( VS_OUTPUT In) : SV_Target
{
  return tex.Sample(sSampler, In.TexCoord);
}

float4 SampleMaskTexturePS( VS_OUTPUT In) : SV_Target
{
    return tex.Sample(sSampler, In.TexCoord) * mask.Sample(sMaskSampler, In.MaskTexCoord).a;
}

float4 SampleRadialGradientPS(VS_RADIAL_OUTPUT In, uniform sampler aSampler) : SV_Target
{
    // Radial gradient painting is defined as the set of circles whose centers
    // are described by C(t) = (C2 - C1) * t + C1; with radii
    // R(t) = (R2 - R1) * t + R1; for R(t) > 0. This shader solves the
    // quadratic equation that arises when calculating t for pixel (x, y).
    //
    // A more extensive derrivation can be found in the pixman radial gradient
    // code.
 
    float2 p = In.PixelCoord;
    float3 dp = float3(p - center1, radius1);

    // dpx * dcx + dpy * dcy + r * dr
    float B = dot(dp, diff);

    float C = pow(dp.x, 2) + pow(dp.y, 2) - sq_radius1;

    float det = pow(B, 2) - A * C;

    if (det < 0) {
      return float4(0, 0, 0, 0);
    }

    float sqrt_det = sqrt(abs(det));

    float2 t = (B + float2(sqrt_det, -sqrt_det)) / A;

    float2 isValid = step(float2(-radius1, -radius1), t * diff.z);

    if (max(isValid.x, isValid.y) <= 0) {
      return float4(0, 0, 0, 0);
    }

    float upper_t = lerp(t.y, t.x, isValid.x);

    float4 output = tex.Sample(aSampler, float2(upper_t, 0.5));
    // Premultiply
    output.rgb *= output.a;
    // Multiply the output color by the input mask for the operation.
    output *= mask.Sample(sMaskSampler, In.MaskTexCoord).a;
    return output;
};

float4 SampleRadialGradientA0PS( VS_RADIAL_OUTPUT In, uniform sampler aSampler ) : SV_Target
{
    // This simpler shader is used for the degenerate case where A is 0,
    // i.e. we're actually solving a linear equation.

    float2 p = In.PixelCoord;
    float3 dp = float3(p - center1, radius1);

    // dpx * dcx + dpy * dcy + r * dr
    float B = dot(dp, diff);

    float C = pow(dp.x, 2) + pow(dp.y, 2) - pow(radius1, 2);

    float t = 0.5 * C / B;

    if (-radius1 >= t * diff.z) {
      return float4(0, 0, 0, 0);
    }

    float4 output = tex.Sample(aSampler, float2(t, 0.5));
    // Premultiply
    output.rgb *= output.a;
    // Multiply the output color by the input mask for the operation.
    output *= mask.Sample(sMaskSampler, In.MaskTexCoord).a;
    return output;
};

float4 SampleShadowHPS( VS_OUTPUT In) : SV_Target
{
    float outputStrength = 0;

    outputStrength += BlurWeights[0].x * tex.Sample(sShadowSampler, float2(In.TexCoord.x + BlurOffsetsH[0].x, In.TexCoord.y)).a;
    outputStrength += BlurWeights[0].y * tex.Sample(sShadowSampler, float2(In.TexCoord.x + BlurOffsetsH[0].y, In.TexCoord.y)).a;
    outputStrength += BlurWeights[0].z * tex.Sample(sShadowSampler, float2(In.TexCoord.x + BlurOffsetsH[0].z, In.TexCoord.y)).a;
    outputStrength += BlurWeights[0].w * tex.Sample(sShadowSampler, float2(In.TexCoord.x + BlurOffsetsH[0].w, In.TexCoord.y)).a;
    outputStrength += BlurWeights[1].x * tex.Sample(sShadowSampler, float2(In.TexCoord.x + BlurOffsetsH[1].x, In.TexCoord.y)).a;
    outputStrength += BlurWeights[1].y * tex.Sample(sShadowSampler, float2(In.TexCoord.x + BlurOffsetsH[1].y, In.TexCoord.y)).a;
    outputStrength += BlurWeights[1].z * tex.Sample(sShadowSampler, float2(In.TexCoord.x + BlurOffsetsH[1].z, In.TexCoord.y)).a;
    outputStrength += BlurWeights[1].w * tex.Sample(sShadowSampler, float2(In.TexCoord.x + BlurOffsetsH[1].w, In.TexCoord.y)).a;
    outputStrength += BlurWeights[2].x * tex.Sample(sShadowSampler, float2(In.TexCoord.x + BlurOffsetsH[2].x, In.TexCoord.y)).a;

    return ShadowColor * outputStrength;
};

float4 SampleShadowVPS( VS_OUTPUT In) : SV_Target
{
    float4 outputColor = float4(0, 0, 0, 0);

    outputColor += BlurWeights[0].x * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[0].x));
    outputColor += BlurWeights[0].y * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[0].y));
    outputColor += BlurWeights[0].z * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[0].z));
    outputColor += BlurWeights[0].w * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[0].w));
    outputColor += BlurWeights[1].x * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[1].x));
    outputColor += BlurWeights[1].y * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[1].y));
    outputColor += BlurWeights[1].z * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[1].z));
    outputColor += BlurWeights[1].w * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[1].w));
    outputColor += BlurWeights[2].x * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[2].x));

    return outputColor;
};

float4 SampleMaskShadowVPS( VS_OUTPUT In) : SV_Target
{
    float4 outputColor = float4(0, 0, 0, 0);

    outputColor += BlurWeights[0].x * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[0].x));
    outputColor += BlurWeights[0].y * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[0].y));
    outputColor += BlurWeights[0].z * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[0].z));
    outputColor += BlurWeights[0].w * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[0].w));
    outputColor += BlurWeights[1].x * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[1].x));
    outputColor += BlurWeights[1].y * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[1].y));
    outputColor += BlurWeights[1].z * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[1].z));
    outputColor += BlurWeights[1].w * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[1].w));
    outputColor += BlurWeights[2].x * tex.Sample(sShadowSampler, float2(In.TexCoord.x, In.TexCoord.y + BlurOffsetsV[2].x));

    return outputColor * mask.Sample(sMaskSampler, In.MaskTexCoord).a;
};

PS_TEXT_OUTPUT SampleTextTexturePS( VS_OUTPUT In) : SV_Target
{
    PS_TEXT_OUTPUT output;
    output.color = float4(TextColor.r, TextColor.g, TextColor.b, 1.0);
    output.alpha.rgba = tex.Sample(sSampler, In.TexCoord).bgrg * TextColor.a;
    return output;
};

PS_TEXT_OUTPUT SampleTextTexturePSMasked( VS_OUTPUT In) : SV_Target
{
    PS_TEXT_OUTPUT output;
    
    float maskValue = mask.Sample(sMaskSampler, In.MaskTexCoord).a;

    output.color = float4(TextColor.r, TextColor.g, TextColor.b, 1.0);
    output.alpha.rgba = tex.Sample(sSampler, In.TexCoord).bgrg * TextColor.a * maskValue;
    
    return output;
};

technique10 SampleTexture
{
    pass P0
    {
        SetRasterizerState(TextureRast);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleTextureVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleTexturePS()));
    }
}

technique10 SampleTextureForSeparableBlending_1
{
    pass P0
    {
        SetRasterizerState(TextureRast);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleTextureVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleBlendTextureSeparablePS_1()));
    }
}

technique10 SampleTextureForSeparableBlending_2
{
    pass P0
    {
        SetRasterizerState(TextureRast);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleTextureVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleBlendTextureSeparablePS_2()));
    }
}

technique10 SampleTextureForNonSeparableBlending
{
    pass P0
    {
        SetRasterizerState(TextureRast);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleTextureVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleBlendTextureNonSeparablePS()));
    }
}

technique10 SampleRadialGradient
{
    pass APos
    {
        SetRasterizerState(TextureRast);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleRadialVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleRadialGradientPS( sSampler )));
    }
    pass A0
    {
        SetRasterizerState(TextureRast);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleRadialVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleRadialGradientA0PS( sSampler )));
    }
    pass APosWrap
    {
        SetRasterizerState(TextureRast);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleRadialVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleRadialGradientPS( sWrapSampler )));
    }
    pass A0Wrap
    {
        SetRasterizerState(TextureRast);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleRadialVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleRadialGradientA0PS( sWrapSampler )));
    }
    pass APosMirror
    {
        SetRasterizerState(TextureRast);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleRadialVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleRadialGradientPS( sMirrorSampler )));
    }
    pass A0Mirror
    {
        SetRasterizerState(TextureRast);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleRadialVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleRadialGradientA0PS( sMirrorSampler )));
    }
}

technique10 SampleMaskedTexture
{
    pass P0
    {
        SetRasterizerState(TextureRast);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleTextureVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleMaskTexturePS()));
    }
}

technique10 SampleTextureWithShadow
{
    // Horizontal pass
    pass P0
    {
        SetRasterizerState(TextureRast);
        SetBlendState(ShadowBlendH, float4(1.0f, 1.0f, 1.0f, 1.0f), 0xffffffff);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleTextureVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleShadowHPS()));
    }
    // Vertical pass
    pass P1
    {
        SetRasterizerState(TextureRast);
        SetBlendState(ShadowBlendV, float4(1.0f, 1.0f, 1.0f, 1.0f), 0xffffffff);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleTextureVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleShadowVPS()));
    }
    // Vertical pass - used when using a mask
    pass P2
    {
        SetRasterizerState(TextureRast);
        SetBlendState(ShadowBlendV, float4(1.0f, 1.0f, 1.0f, 1.0f), 0xffffffff);
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleTextureVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleMaskShadowVPS()));
    }
}

technique10 SampleTextTexture
{
    pass Unmasked
    {
        SetRasterizerState(TextureRast);
        SetBlendState(bTextBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleTextureVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleTextTexturePS()));
    }
    pass Masked
    {
        SetRasterizerState(TextureRast);
        SetBlendState(bTextBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleTextureVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleTextTexturePSMasked()));
    }
}

