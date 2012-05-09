float4x4 mLayerTransform;
float4 vRenderTargetOffset;
float4x4 mProjection;

typedef float4 rect;
rect vTextureCoords;
rect vLayerQuad;
rect vMaskQuad;

texture tex0;
sampler s2D;
sampler s2DWhite;
sampler s2DY;
sampler s2DCb;
sampler s2DCr;
sampler s2DMask;


float fLayerOpacity;
float4 fLayerColor;

struct VS_INPUT {
  float4 vPosition : POSITION;
};

struct VS_OUTPUT {
  float4 vPosition : POSITION;
  float2 vTexCoords : TEXCOORD0;
};

struct VS_OUTPUT_MASK {
  float4 vPosition : POSITION;
  float2 vTexCoords : TEXCOORD0;
  float2 vMaskCoords : TEXCOORD1;
};

struct VS_OUTPUT_MASK_3D {
  float4 vPosition : POSITION;
  float2 vTexCoords : TEXCOORD0;
  float3 vMaskCoords : TEXCOORD1;
};

VS_OUTPUT LayerQuadVS(const VS_INPUT aVertex)
{
  VS_OUTPUT outp;
  outp.vPosition = aVertex.vPosition;
  
  // We use 4 component floats to uniquely describe a rectangle, by the structure
  // of x, y, width, height. This allows us to easily generate the 4 corners
  // of any rectangle from the 4 corners of the 0,0-1,1 quad that we use as the
  // stream source for our LayerQuad vertex shader. We do this by doing:
  // Xout = x + Xin * width
  // Yout = y + Yin * height
  float2 position = vLayerQuad.xy;
  float2 size = vLayerQuad.zw;
  outp.vPosition.x = position.x + outp.vPosition.x * size.x;
  outp.vPosition.y = position.y + outp.vPosition.y * size.y;
  
  outp.vPosition = mul(mLayerTransform, outp.vPosition);
  outp.vPosition.xyz /= outp.vPosition.w;
  outp.vPosition = outp.vPosition - vRenderTargetOffset;
  outp.vPosition.xyz *= outp.vPosition.w;
  
  // adjust our vertices to match d3d9's pixel coordinate system
  // which has pixel centers at integer locations
  outp.vPosition.xy -= 0.5;
  
  outp.vPosition = mul(mProjection, outp.vPosition);

  position = vTextureCoords.xy;
  size = vTextureCoords.zw;
  outp.vTexCoords.x = position.x + aVertex.vPosition.x * size.x;
  outp.vTexCoords.y = position.y + aVertex.vPosition.y * size.y;

  return outp;
}

VS_OUTPUT_MASK LayerQuadVSMask(const VS_INPUT aVertex)
{
  VS_OUTPUT_MASK outp;
  float4 position = float4(0, 0, 0, 1);
  
  // We use 4 component floats to uniquely describe a rectangle, by the structure
  // of x, y, width, height. This allows us to easily generate the 4 corners
  // of any rectangle from the 4 corners of the 0,0-1,1 quad that we use as the
  // stream source for our LayerQuad vertex shader. We do this by doing:
  // Xout = x + Xin * width
  // Yout = y + Yin * height
  float2 size = vLayerQuad.zw;
  position.x = vLayerQuad.x + aVertex.vPosition.x * size.x;
  position.y = vLayerQuad.y + aVertex.vPosition.y * size.y;
  
  position = mul(mLayerTransform, position);
  outp.vPosition.w = position.w;
  outp.vPosition.xyz = position.xyz / position.w;
  outp.vPosition = outp.vPosition - vRenderTargetOffset;
  outp.vPosition.xyz *= outp.vPosition.w;
  
  // adjust our vertices to match d3d9's pixel coordinate system
  // which has pixel centers at integer locations
  outp.vPosition.xy -= 0.5;
  
  outp.vPosition = mul(mProjection, outp.vPosition);

  // calculate the position on the mask texture
  outp.vMaskCoords.x = (position.x - vMaskQuad.x) / vMaskQuad.z;
  outp.vMaskCoords.y = (position.y - vMaskQuad.y) / vMaskQuad.w;

  size = vTextureCoords.zw;
  outp.vTexCoords.x = vTextureCoords.x + aVertex.vPosition.x * size.x;
  outp.vTexCoords.y = vTextureCoords.y + aVertex.vPosition.y * size.y;

  return outp;
}

VS_OUTPUT_MASK_3D LayerQuadVSMask3D(const VS_INPUT aVertex)
{
  VS_OUTPUT_MASK_3D outp;
  float4 position = float4(0, 0, 0, 1);
  
  // We use 4 component floats to uniquely describe a rectangle, by the structure
  // of x, y, width, height. This allows us to easily generate the 4 corners
  // of any rectangle from the 4 corners of the 0,0-1,1 quad that we use as the
  // stream source for our LayerQuad vertex shader. We do this by doing:
  // Xout = x + Xin * width
  // Yout = y + Yin * height
  float2 size = vLayerQuad.zw;
  position.x = vLayerQuad.x + aVertex.vPosition.x * size.x;
  position.y = vLayerQuad.y + aVertex.vPosition.y * size.y;
  
  position = mul(mLayerTransform, position);
  outp.vPosition.w = position.w;
  outp.vPosition.xyz = position.xyz / position.w;
  outp.vPosition = outp.vPosition - vRenderTargetOffset;
  outp.vPosition.xyz *= outp.vPosition.w;
  
  // adjust our vertices to match d3d9's pixel coordinate system
  // which has pixel centers at integer locations
  outp.vPosition.xy -= 0.5;
  
  outp.vPosition = mul(mProjection, outp.vPosition);

  // calculate the position on the mask texture
  position.xyz /= position.w;
  outp.vMaskCoords.x = (position.x - vMaskQuad.x) / vMaskQuad.z;
  outp.vMaskCoords.y = (position.y - vMaskQuad.y) / vMaskQuad.w;
  // correct for perspective correct interpolation, see comment in D3D10 shader
  outp.vMaskCoords.z = 1;
  outp.vMaskCoords *= position.w;

  size = vTextureCoords.zw;
  outp.vTexCoords.x = vTextureCoords.x + aVertex.vPosition.x * size.x;
  outp.vTexCoords.y = vTextureCoords.y + aVertex.vPosition.y * size.y;

  return outp;
}

float4 ComponentPass1Shader(const VS_OUTPUT aVertex) : COLOR
{
  float4 src = tex2D(s2D, aVertex.vTexCoords);
  float4 alphas = 1.0 - tex2D(s2DWhite, aVertex.vTexCoords) + src;
  alphas.a = alphas.g;
  return alphas * fLayerOpacity;
}

float4 ComponentPass2Shader(const VS_OUTPUT aVertex) : COLOR
{
  float4 src = tex2D(s2D, aVertex.vTexCoords);
  float4 alphas = 1.0 - tex2D(s2DWhite, aVertex.vTexCoords) + src;
  src.a = alphas.g;
  return src * fLayerOpacity;
}

float4 RGBAShader(const VS_OUTPUT aVertex) : COLOR
{
  return tex2D(s2D, aVertex.vTexCoords) * fLayerOpacity;
}

float4 RGBShader(const VS_OUTPUT aVertex) : COLOR
{
  float4 result;
  result = tex2D(s2D, aVertex.vTexCoords);
  result.a = 1.0;
  return result * fLayerOpacity;
}

float4 YCbCrShader(const VS_OUTPUT aVertex) : COLOR
{
  float4 yuv;
  float4 color;

  yuv.r = tex2D(s2DCr, aVertex.vTexCoords).r - 0.5;
  yuv.g = tex2D(s2DY, aVertex.vTexCoords).r - 0.0625;
  yuv.b = tex2D(s2DCb, aVertex.vTexCoords).r - 0.5;

  color.r = yuv.g * 1.164 + yuv.r * 1.596;
  color.g = yuv.g * 1.164 - 0.813 * yuv.r - 0.391 * yuv.b;
  color.b = yuv.g * 1.164 + yuv.b * 2.018;
  color.a = 1.0f;
 
  return color * fLayerOpacity;
}

float4 SolidColorShader(const VS_OUTPUT aVertex) : COLOR
{
  return fLayerColor;
}

float4 ComponentPass1ShaderMask(const VS_OUTPUT_MASK aVertex) : COLOR
{
  float4 src = tex2D(s2D, aVertex.vTexCoords);
  float4 alphas = 1.0 - tex2D(s2DWhite, aVertex.vTexCoords) + src;
  alphas.a = alphas.g;
  float2 maskCoords = aVertex.vMaskCoords;
  float mask = tex2D(s2DMask, maskCoords).a;
  return alphas * fLayerOpacity * mask;
}

float4 ComponentPass2ShaderMask(const VS_OUTPUT_MASK aVertex) : COLOR
{
  float4 src = tex2D(s2D, aVertex.vTexCoords);
  float4 alphas = 1.0 - tex2D(s2DWhite, aVertex.vTexCoords) + src;
  src.a = alphas.g;
  float2 maskCoords = aVertex.vMaskCoords;
  float mask = tex2D(s2DMask, maskCoords).a;
  return src * fLayerOpacity * mask;
}

float4 RGBAShaderMask(const VS_OUTPUT_MASK aVertex) : COLOR
{
  float2 maskCoords = aVertex.vMaskCoords;
  float mask = tex2D(s2DMask, maskCoords).a;
  return tex2D(s2D, aVertex.vTexCoords) * fLayerOpacity * mask;
}

float4 RGBAShaderMask3D(const VS_OUTPUT_MASK_3D aVertex) : COLOR
{
  float2 maskCoords = aVertex.vMaskCoords.xy / aVertex.vMaskCoords.z;
  float mask = tex2D(s2DMask, maskCoords).a;
  return tex2D(s2D, aVertex.vTexCoords) * fLayerOpacity * mask;
}

float4 RGBShaderMask(const VS_OUTPUT_MASK aVertex) : COLOR
{
  float4 result;
  result = tex2D(s2D, aVertex.vTexCoords);
  result.a = 1.0;
  float2 maskCoords = aVertex.vMaskCoords;
  float mask = tex2D(s2DMask, maskCoords).a;
  return result * fLayerOpacity * mask;
}

float4 YCbCrShaderMask(const VS_OUTPUT_MASK aVertex) : COLOR
{
  float4 yuv;
  float4 color;

  yuv.r = tex2D(s2DCr, aVertex.vTexCoords).r - 0.5;
  yuv.g = tex2D(s2DY, aVertex.vTexCoords).r - 0.0625;
  yuv.b = tex2D(s2DCb, aVertex.vTexCoords).r - 0.5;

  color.r = yuv.g * 1.164 + yuv.r * 1.596;
  color.g = yuv.g * 1.164 - 0.813 * yuv.r - 0.391 * yuv.b;
  color.b = yuv.g * 1.164 + yuv.b * 2.018;
  color.a = 1.0f;
 
  float2 maskCoords = aVertex.vMaskCoords;
  float mask = tex2D(s2DMask, maskCoords).a;
  return color * fLayerOpacity * mask;
}

float4 SolidColorShaderMask(const VS_OUTPUT_MASK aVertex) : COLOR
{
  float2 maskCoords = aVertex.vMaskCoords;
  float mask = tex2D(s2DMask, maskCoords).a;
  return fLayerColor * mask;
}
