float4x4 mLayerTransform;
float4 vRenderTargetOffset;
float4x4 mProjection;

typedef float4 rect;
rect vTextureCoords;
rect vLayerQuad;

texture tex0;
sampler s2D;
sampler s2DWhite;
sampler s2DY;
sampler s2DCb;
sampler s2DCr;

float fLayerOpacity;
float4 fLayerColor;

struct VS_INPUT {
  float4 vPosition : POSITION;
};

struct VS_OUTPUT {
  float4 vPosition : POSITION;
  float2 vTexCoords : TEXCOORD0;
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
