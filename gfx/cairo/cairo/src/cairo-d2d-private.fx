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
    float4 TextColor;
}

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 color;
    float4 alpha;
};

Texture2D tex;

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

sampler sSampler = sampler_state {
    Texture = tex;
    AddressU = Clamp;
    AddressV = Clamp;
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

PS_OUTPUT SampleTextTexturePS( VS_OUTPUT In) : SV_Target
{
    PS_OUTPUT output;
    output.color = TextColor;
    output.alpha.rgba = tex.Sample(sSampler, In.TexCoord).bgrg * TextColor.a;
    return output;
};

technique10 SampleTexture
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleTextureVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleTexturePS()));
    }
}

technique10 SampleTextTexture
{
    pass P0
    {
        SetBlendState(bTextBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader(CompileShader(vs_4_0_level_9_3, SampleTextureVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0_level_9_3, SampleTextTexturePS()));
    }
}
