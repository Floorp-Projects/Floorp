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
}

cbuffer cb1
{
    float4 BlurOffsetsH[3];
    float4 BlurOffsetsV[3];
    float4 BlurWeights[3];
    float4 ShadowColor;
}

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

Texture2D tex;

sampler sSampler = sampler_state {
    Filter = MIN_MAG_MIP_LINEAR;
    Texture = tex;
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
  ScissorEnable = False;
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

VS_OUTPUT SampleTextureVS(float3 pos : POSITION)
{
    VS_OUTPUT Output;
    Output.Position.w = 1.0f;
    Output.Position.x = pos.x * QuadDesc.z + QuadDesc.x;
    Output.Position.y = pos.y * QuadDesc.w + QuadDesc.y;
    Output.Position.z = 0;
    Output.TexCoord.x = pos.x * TexCoords.z + TexCoords.x;
    Output.TexCoord.y = pos.y * TexCoords.w + TexCoords.y;
    return Output;
}

float4 SampleTexturePS( VS_OUTPUT In) : SV_Target
{
    return tex.Sample(sSampler, In.TexCoord);
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
}