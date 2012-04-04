typedef float4 rect;
cbuffer PerLayer {
  rect vTextureCoords;
  rect vLayerQuad;
  float fLayerOpacity;
  float4x4 mLayerTransform;
}

cbuffer PerOccasionalLayer {
  float4 vRenderTargetOffset;
  float4 fLayerColor;
}

cbuffer PerLayerManager {
  float4x4 mProjection;
}

BlendState Premul
{
  AlphaToCoverageEnable = FALSE;
  BlendEnable[0] = TRUE;
  SrcBlend = One;
  DestBlend = Inv_Src_Alpha;
  BlendOp = Add;
  SrcBlendAlpha = One;
  DestBlendAlpha = Inv_Src_Alpha;
  BlendOpAlpha = Add;
  RenderTargetWriteMask[0] = 0x0F; // All
};

BlendState NonPremul
{
  AlphaToCoverageEnable = FALSE;
  BlendEnable[0] = TRUE;
  SrcBlend = Src_Alpha;
  DestBlend = Inv_Src_Alpha;
  BlendOp = Add;
  SrcBlendAlpha = One;
  DestBlendAlpha = Inv_Src_Alpha;
  BlendOpAlpha = Add;
  RenderTargetWriteMask[0] = 0x0F; // All
};

BlendState NoBlendDual
{
  AlphaToCoverageEnable = FALSE;
  BlendEnable[0] = FALSE;
  BlendEnable[1] = FALSE;
  RenderTargetWriteMask[0] = 0x0F; // All
  RenderTargetWriteMask[1] = 0x0F; // All
};

BlendState ComponentAlphaBlend
{
  AlphaToCoverageEnable = FALSE;
  BlendEnable[0] = TRUE;
  SrcBlend = One;
  DestBlend = Inv_Src1_Color;
  BlendOp = Add;
  SrcBlendAlpha = One;
  DestBlendAlpha = Inv_Src_Alpha;
  BlendOpAlpha = Add;
  RenderTargetWriteMask[0] = 0x0F; // All
};

RasterizerState LayerRast
{
  ScissorEnable = True;
  CullMode = None;
};

Texture2D tRGB;
Texture2D tY;
Texture2D tCb;
Texture2D tCr;
Texture2D tRGBWhite;

SamplerState LayerTextureSamplerLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState LayerTextureSamplerPoint
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

struct VS_INPUT {
  float2 vPosition : POSITION;
};

struct VS_OUTPUT {
  float4 vPosition : SV_Position;
  float2 vTexCoords : TEXCOORD0;
};

struct PS_OUTPUT {
  float4 vSrc;
  float4 vAlpha;
};

struct PS_DUAL_OUTPUT {
  float4 vOutput1 : SV_Target0;
  float4 vOutput2 : SV_Target1;
};

VS_OUTPUT LayerQuadVS(const VS_INPUT aVertex)
{
  VS_OUTPUT outp;
  outp.vPosition.z = 0;
  outp.vPosition.w = 1;
  
  // We use 4 component floats to uniquely describe a rectangle, by the structure
  // of x, y, width, height. This allows us to easily generate the 4 corners
  // of any rectangle from the 4 corners of the 0,0-1,1 quad that we use as the
  // stream source for our LayerQuad vertex shader. We do this by doing:
  // Xout = x + Xin * width
  // Yout = y + Yin * height
  float2 position = vLayerQuad.xy;
  float2 size = vLayerQuad.zw;
  outp.vPosition.x = position.x + aVertex.vPosition.x * size.x;
  outp.vPosition.y = position.y + aVertex.vPosition.y * size.y;

  outp.vPosition = mul(mLayerTransform, outp.vPosition);
  outp.vPosition.xyz /= outp.vPosition.w;
  outp.vPosition = outp.vPosition - vRenderTargetOffset;
  outp.vPosition.xyz *= outp.vPosition.w;
  
  outp.vPosition = mul(mProjection, outp.vPosition);

  position = vTextureCoords.xy;
  size = vTextureCoords.zw;
  outp.vTexCoords.x = position.x + aVertex.vPosition.x * size.x;
  outp.vTexCoords.y = position.y + aVertex.vPosition.y * size.y;
  return outp;
}

float4 RGBAShaderLinear(const VS_OUTPUT aVertex) : SV_Target
{
  return tRGB.Sample(LayerTextureSamplerLinear, aVertex.vTexCoords) * fLayerOpacity;
}

float4 RGBAShaderPoint(const VS_OUTPUT aVertex) : SV_Target
{
  return tRGB.Sample(LayerTextureSamplerPoint, aVertex.vTexCoords) * fLayerOpacity;
}

float4 RGBShaderLinear(const VS_OUTPUT aVertex) : SV_Target
{
  float4 result;
  result = tRGB.Sample(LayerTextureSamplerLinear, aVertex.vTexCoords) * fLayerOpacity;
  result.a = fLayerOpacity;
  return result;
}

float4 RGBShaderPoint(const VS_OUTPUT aVertex) : SV_Target
{
  float4 result;
  result = tRGB.Sample(LayerTextureSamplerPoint, aVertex.vTexCoords) * fLayerOpacity;
  result.a = fLayerOpacity;
  return result;
}

float4 YCbCrShader(const VS_OUTPUT aVertex) : SV_Target
{
  float4 yuv;
  float4 color;

  yuv.r = tCr.Sample(LayerTextureSamplerLinear, aVertex.vTexCoords).r - 0.5;
  yuv.g = tY.Sample(LayerTextureSamplerLinear, aVertex.vTexCoords).r - 0.0625;
  yuv.b = tCb.Sample(LayerTextureSamplerLinear, aVertex.vTexCoords).r - 0.5;

  color.r = yuv.g * 1.164 + yuv.r * 1.596;
  color.g = yuv.g * 1.164 - 0.813 * yuv.r - 0.391 * yuv.b;
  color.b = yuv.g * 1.164 + yuv.b * 2.018;
  color.a = 1.0f;
 
  return color * fLayerOpacity;
}

PS_OUTPUT ComponentAlphaShader(const VS_OUTPUT aVertex) : SV_Target
{
  PS_OUTPUT result;

  result.vSrc = tRGB.Sample(LayerTextureSamplerLinear, aVertex.vTexCoords);
  result.vAlpha = 1.0 - tRGBWhite.Sample(LayerTextureSamplerLinear, aVertex.vTexCoords) + result.vSrc;
  result.vSrc.a = result.vAlpha.g;
  result.vSrc *= fLayerOpacity;
  result.vAlpha *= fLayerOpacity;
  return result;
}

float4 SolidColorShader(const VS_OUTPUT aVertex) : SV_Target
{
  return fLayerColor;
}

PS_DUAL_OUTPUT AlphaExtractionPrepareShader(const VS_OUTPUT aVertex)
{
  PS_DUAL_OUTPUT result;
  result.vOutput1 = float4(0, 0, 0, 1);
  result.vOutput2 = float4(1, 1, 1, 1);
  return result;
}

technique10 RenderRGBLayerPremul
{
    pass P0
    {
        SetRasterizerState( LayerRast );
        SetBlendState( Premul, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader( CompileShader( vs_4_0_level_9_3, LayerQuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_3, RGBShaderLinear() ) );
    }
}

technique10 RenderRGBLayerPremulPoint
{
    pass P0
    {
        SetRasterizerState( LayerRast );
        SetBlendState( Premul, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader( CompileShader( vs_4_0_level_9_3, LayerQuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_3, RGBShaderPoint() ) );
    }
}

technique10 RenderRGBALayerPremul
{
    pass P0
    {
        SetRasterizerState( LayerRast );
        SetBlendState( Premul, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader( CompileShader( vs_4_0_level_9_3, LayerQuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_3, RGBAShaderLinear() ) );
    }
}

technique10 RenderRGBALayerNonPremul
{
    pass P0
    {
        SetRasterizerState( LayerRast );
        SetBlendState( NonPremul, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader( CompileShader( vs_4_0_level_9_3, LayerQuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_3, RGBAShaderLinear() ) );
    }
}

technique10 RenderRGBALayerPremulPoint
{
    pass P0
    {
        SetRasterizerState( LayerRast );
        SetBlendState( Premul, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader( CompileShader( vs_4_0_level_9_3, LayerQuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_3, RGBAShaderPoint() ) );
    }
}

technique10 RenderRGBALayerNonPremulPoint
{
    pass P0
    {
        SetRasterizerState( LayerRast );
        SetBlendState( NonPremul, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader( CompileShader( vs_4_0_level_9_3, LayerQuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_3, RGBAShaderPoint() ) );
    }
}

technique10 RenderYCbCrLayer
{
    pass P0
    {
        SetRasterizerState( LayerRast );
        SetBlendState( Premul, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader( CompileShader( vs_4_0_level_9_3, LayerQuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_3, YCbCrShader() ) );
    }
}

technique10 RenderComponentAlphaLayer
{
	Pass P0
	{
	    SetRasterizerState( LayerRast );
        SetBlendState( ComponentAlphaBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader( CompileShader( vs_4_0_level_9_3, LayerQuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_3, ComponentAlphaShader() ) );
	}

}

technique10 RenderSolidColorLayer
{
    pass P0
    {
        SetRasterizerState( LayerRast );
        SetBlendState( Premul, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader( CompileShader( vs_4_0_level_9_3, LayerQuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_3, SolidColorShader() ) );
    }
}

technique10 PrepareAlphaExtractionTextures
{
    pass P0
    {
        SetRasterizerState( LayerRast );
        SetBlendState( NoBlendDual, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetVertexShader( CompileShader( vs_4_0_level_9_3, LayerQuadVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0_level_9_3, AlphaExtractionPrepareShader() ) );
    }
}
