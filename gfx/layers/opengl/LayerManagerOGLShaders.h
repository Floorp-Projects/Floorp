/* AUTOMATICALLY GENERATED from LayerManagerOGLShaders.txt */
/* DO NOT EDIT! */

static const char sLayerVS[] = "/* sLayerVS */\n\
/* Vertex Shader */\n\
uniform mat4 uMatrixProj;\n\
uniform mat4 uLayerQuadTransform;\n\
uniform mat4 uLayerTransform;\n\
uniform mat4 uTextureTransform;\n\
uniform vec4 uRenderTargetOffset;\n\
attribute vec4 aVertexCoord;\n\
attribute vec2 aTexCoord;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
void main()\n\
{\n\
vec4 finalPosition = aVertexCoord;\n\
finalPosition = uLayerQuadTransform * finalPosition;\n\
finalPosition = uLayerTransform * finalPosition;\n\
finalPosition.xyz /= finalPosition.w;\n\
\n\
\n\
finalPosition = finalPosition - uRenderTargetOffset;\n\
finalPosition.xyz *= finalPosition.w;\n\
finalPosition = uMatrixProj * finalPosition;\n\
vTexCoord = (uTextureTransform * vec4(aTexCoord.x, aTexCoord.y, 0.0, 1.0)).xy;\n\
gl_Position = finalPosition;\n\
}\n\
";

static const char sLayerMaskVS[] = "/* sLayerMaskVS */\n\
/* Vertex Shader */\n\
uniform mat4 uMatrixProj;\n\
uniform mat4 uLayerQuadTransform;\n\
uniform mat4 uLayerTransform;\n\
uniform mat4 uTextureTransform;\n\
uniform vec4 uRenderTargetOffset;\n\
attribute vec4 aVertexCoord;\n\
attribute vec2 aTexCoord;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform mat4 uMaskQuadTransform;\n\
varying vec2 vMaskCoord;\n\
\n\
void main()\n\
{\n\
vec4 finalPosition = aVertexCoord;\n\
finalPosition = uLayerQuadTransform * finalPosition;\n\
finalPosition = uLayerTransform * finalPosition;\n\
finalPosition.xyz /= finalPosition.w;\n\
vMaskCoord = (uMaskQuadTransform * finalPosition).xy;\n\
\n\
finalPosition = finalPosition - uRenderTargetOffset;\n\
finalPosition.xyz *= finalPosition.w;\n\
finalPosition = uMatrixProj * finalPosition;\n\
vTexCoord = (uTextureTransform * vec4(aTexCoord.x, aTexCoord.y, 0.0, 1.0)).xy;\n\
gl_Position = finalPosition;\n\
}\n\
";

static const char sLayerMask3DVS[] = "/* sLayerMask3DVS */\n\
/* Vertex Shader */\n\
uniform mat4 uMatrixProj;\n\
uniform mat4 uLayerQuadTransform;\n\
uniform mat4 uLayerTransform;\n\
uniform mat4 uTextureTransform;\n\
uniform vec4 uRenderTargetOffset;\n\
attribute vec4 aVertexCoord;\n\
attribute vec2 aTexCoord;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform mat4 uMaskQuadTransform;\n\
varying vec3 vMaskCoord;\n\
\n\
void main()\n\
{\n\
vec4 finalPosition = aVertexCoord;\n\
finalPosition = uLayerQuadTransform * finalPosition;\n\
finalPosition = uLayerTransform * finalPosition;\n\
finalPosition.xyz /= finalPosition.w;\n\
vMaskCoord.xy = (uMaskQuadTransform * vec4(finalPosition.xyz, 1.0)).xy;\n\
// correct for perspective correct interpolation, see comment in D3D10 shader\n\
vMaskCoord.z = 1.0;\n\
vMaskCoord *= finalPosition.w;\n\
\n\
finalPosition = finalPosition - uRenderTargetOffset;\n\
finalPosition.xyz *= finalPosition.w;\n\
finalPosition = uMatrixProj * finalPosition;\n\
vTexCoord = (uTextureTransform * vec4(aTexCoord.x, aTexCoord.y, 0.0, 1.0)).xy;\n\
gl_Position = finalPosition;\n\
}\n\
";

static const char sSolidColorLayerFS[] = "/* sSolidColorLayerFS */\n\
#define NO_LAYER_OPACITY 1\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform vec4 uRenderColor;\n\
void main()\n\
{\n\
float mask = 1.0;\n\
\n\
gl_FragColor = mask * uRenderColor;\n\
}\n\
";

static const char sSolidColorLayerMaskFS[] = "/* sSolidColorLayerMaskFS */\n\
#define NO_LAYER_OPACITY 1\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
uniform vec4 uRenderColor;\n\
void main()\n\
{\n\
float mask = texture2D(uMaskTexture, vMaskCoord).r;\n\
\n\
gl_FragColor = mask * uRenderColor;\n\
}\n\
";

static const char sRGBATextureLayerFS[] = "/* sRGBATextureLayerFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform sampler2D uTexture;\n\
void main()\n\
{\n\
float mask = 1.0;\n\
\n\
gl_FragColor = texture2D(uTexture, vTexCoord) * uLayerOpacity * mask;\n\
}\n\
";

static const char sRGBATextureLayerMaskFS[] = "/* sRGBATextureLayerMaskFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
uniform sampler2D uTexture;\n\
void main()\n\
{\n\
float mask = texture2D(uMaskTexture, vMaskCoord).r;\n\
\n\
gl_FragColor = texture2D(uTexture, vTexCoord) * uLayerOpacity * mask;\n\
}\n\
";

static const char sRGBATextureLayerMask3DFS[] = "/* sRGBATextureLayerMask3DFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec3 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
uniform sampler2D uTexture;\n\
void main()\n\
{\n\
vec2 maskCoords = vMaskCoord.xy / vMaskCoord.z;\n\
float mask = texture2D(uMaskTexture, maskCoords).r;\n\
\n\
gl_FragColor = texture2D(uTexture, vTexCoord) * uLayerOpacity * mask;\n\
}\n\
";

static const char sRGBARectTextureLayerFS[] = "/* sRGBARectTextureLayerFS */\n\
#extension GL_ARB_texture_rectangle : enable\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
/* This should not be used on GL ES */\n\
#ifndef GL_ES\n\
uniform sampler2DRect uTexture;\n\
uniform vec2 uTexCoordMultiplier;\n\
void main()\n\
{\n\
float mask = 1.0;\n\
\n\
gl_FragColor = texture2DRect(uTexture, vec2(vTexCoord * uTexCoordMultiplier)) * uLayerOpacity * mask;\n\
}\n\
#else\n\
void main()\n\
{\n\
gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n\
}\n\
#endif\n\
";

static const char sRGBARectTextureLayerMaskFS[] = "/* sRGBARectTextureLayerMaskFS */\n\
#extension GL_ARB_texture_rectangle : enable\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
/* This should not be used on GL ES */\n\
#ifndef GL_ES\n\
uniform sampler2DRect uTexture;\n\
uniform vec2 uTexCoordMultiplier;\n\
void main()\n\
{\n\
float mask = texture2D(uMaskTexture, vMaskCoord).r;\n\
\n\
gl_FragColor = texture2DRect(uTexture, vec2(vTexCoord * uTexCoordMultiplier)) * uLayerOpacity * mask;\n\
}\n\
#else\n\
void main()\n\
{\n\
gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n\
}\n\
#endif\n\
";

static const char sRGBARectTextureLayerMask3DFS[] = "/* sRGBARectTextureLayerMask3DFS */\n\
#extension GL_ARB_texture_rectangle : enable\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec3 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
/* This should not be used on GL ES */\n\
#ifndef GL_ES\n\
uniform sampler2DRect uTexture;\n\
uniform vec2 uTexCoordMultiplier;\n\
void main()\n\
{\n\
vec2 maskCoords = vMaskCoord.xy / vMaskCoord.z;\n\
float mask = texture2D(uMaskTexture, maskCoords).r;\n\
\n\
gl_FragColor = texture2DRect(uTexture, vec2(vTexCoord * uTexCoordMultiplier)) * uLayerOpacity * mask;\n\
}\n\
#else\n\
void main()\n\
{\n\
gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n\
}\n\
#endif\n\
";

static const char sRGBXRectTextureLayerFS[] = "/* sRGBXRectTextureLayerFS */\n\
#extension GL_ARB_texture_rectangle : enable\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
/* This should not be used on GL ES */\n\
#ifndef GL_ES\n\
uniform sampler2DRect uTexture;\n\
uniform vec2 uTexCoordMultiplier;\n\
void main()\n\
{\n\
float mask = 1.0;\n\
\n\
gl_FragColor = vec4(texture2DRect(uTexture, vec2(vTexCoord * uTexCoordMultiplier)).rgb, 1.0) * uLayerOpacity * mask;\n\
}\n\
#else\n\
void main()\n\
{\n\
gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n\
}\n\
#endif\n\
";

static const char sRGBXRectTextureLayerMaskFS[] = "/* sRGBXRectTextureLayerMaskFS */\n\
#extension GL_ARB_texture_rectangle : enable\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
/* This should not be used on GL ES */\n\
#ifndef GL_ES\n\
uniform sampler2DRect uTexture;\n\
uniform vec2 uTexCoordMultiplier;\n\
void main()\n\
{\n\
float mask = texture2D(uMaskTexture, vMaskCoord).r;\n\
\n\
gl_FragColor = vec4(texture2DRect(uTexture, vec2(vTexCoord * uTexCoordMultiplier)).rgb, 1.0) * uLayerOpacity * mask;\n\
}\n\
#else\n\
void main()\n\
{\n\
gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n\
}\n\
#endif\n\
";

static const char sRGBXRectTextureLayerMask3DFS[] = "/* sRGBXRectTextureLayerMask3DFS */\n\
#extension GL_ARB_texture_rectangle : enable\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec3 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
/* This should not be used on GL ES */\n\
#ifndef GL_ES\n\
uniform sampler2DRect uTexture;\n\
uniform vec2 uTexCoordMultiplier;\n\
void main()\n\
{\n\
vec2 maskCoords = vMaskCoord.xy / vMaskCoord.z;\n\
float mask = texture2D(uMaskTexture, maskCoords).r;\n\
\n\
gl_FragColor = vec4(texture2DRect(uTexture, vec2(vTexCoord * uTexCoordMultiplier)).rgb, 1.0) * uLayerOpacity * mask;\n\
}\n\
#else\n\
void main()\n\
{\n\
gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n\
}\n\
#endif\n\
";

static const char sBGRARectTextureLayerFS[] = "/* sBGRARectTextureLayerFS */\n\
#extension GL_ARB_texture_rectangle : enable\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform sampler2DRect uTexture;\n\
uniform vec2 uTexCoordMultiplier;\n\
void main()\n\
{\n\
gl_FragColor = texture2DRect(uTexture, vec2(vTexCoord * uTexCoordMultiplier)).bgra * uLayerOpacity;\n\
}\n\
";

static const char sRGBAExternalTextureLayerFS[] = "/* sRGBAExternalTextureLayerFS */\n\
#extension GL_OES_EGL_image_external : require\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform samplerExternalOES uTexture;\n\
void main()\n\
{\n\
float mask = 1.0;\n\
\n\
gl_FragColor = texture2D(uTexture, vTexCoord) * uLayerOpacity * mask;\n\
}\n\
";

static const char sRGBAExternalTextureLayerMaskFS[] = "/* sRGBAExternalTextureLayerMaskFS */\n\
#extension GL_OES_EGL_image_external : require\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
uniform samplerExternalOES uTexture;\n\
void main()\n\
{\n\
float mask = texture2D(uMaskTexture, vMaskCoord).r;\n\
\n\
gl_FragColor = texture2D(uTexture, vTexCoord) * uLayerOpacity * mask;\n\
}\n\
";

static const char sRGBAExternalTextureLayerMask3DFS[] = "/* sRGBAExternalTextureLayerMask3DFS */\n\
#extension GL_OES_EGL_image_external : require\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec3 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
uniform samplerExternalOES uTexture;\n\
void main()\n\
{\n\
vec2 maskCoords = vMaskCoord.xy / vMaskCoord.z;\n\
float mask = texture2D(uMaskTexture, maskCoords).r;\n\
\n\
gl_FragColor = texture2D(uTexture, vTexCoord) * uLayerOpacity * mask;\n\
}\n\
";

static const char sBGRATextureLayerFS[] = "/* sBGRATextureLayerFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform sampler2D uTexture;\n\
void main()\n\
{\n\
float mask = 1.0;\n\
\n\
gl_FragColor = texture2D(uTexture, vTexCoord).bgra * uLayerOpacity * mask;\n\
}\n\
";

static const char sBGRATextureLayerMaskFS[] = "/* sBGRATextureLayerMaskFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
uniform sampler2D uTexture;\n\
void main()\n\
{\n\
float mask = texture2D(uMaskTexture, vMaskCoord).r;\n\
\n\
gl_FragColor = texture2D(uTexture, vTexCoord).bgra * uLayerOpacity * mask;\n\
}\n\
";

static const char sRGBXTextureLayerFS[] = "/* sRGBXTextureLayerFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform sampler2D uTexture;\n\
void main()\n\
{\n\
float mask = 1.0;\n\
\n\
gl_FragColor = vec4(texture2D(uTexture, vTexCoord).rgb, 1.0) * uLayerOpacity * mask;\n\
}\n\
";

static const char sRGBXTextureLayerMaskFS[] = "/* sRGBXTextureLayerMaskFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
uniform sampler2D uTexture;\n\
void main()\n\
{\n\
float mask = texture2D(uMaskTexture, vMaskCoord).r;\n\
\n\
gl_FragColor = vec4(texture2D(uTexture, vTexCoord).rgb, 1.0) * uLayerOpacity * mask;\n\
}\n\
";

static const char sBGRXTextureLayerFS[] = "/* sBGRXTextureLayerFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform sampler2D uTexture;\n\
void main()\n\
{\n\
float mask = 1.0;\n\
\n\
gl_FragColor = vec4(texture2D(uTexture, vTexCoord).bgr, 1.0) * uLayerOpacity * mask;\n\
}\n\
";

static const char sBGRXTextureLayerMaskFS[] = "/* sBGRXTextureLayerMaskFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
uniform sampler2D uTexture;\n\
void main()\n\
{\n\
float mask = texture2D(uMaskTexture, vMaskCoord).r;\n\
\n\
gl_FragColor = vec4(texture2D(uTexture, vTexCoord).bgr, 1.0) * uLayerOpacity * mask;\n\
}\n\
";

static const char sUnifiedLayerVS[] = "/* sUnifiedLayerVS */\n\
/* Vertex Shader */\n\
uniform mat4 uMatrixProj;\n\
uniform mat4 uLayerQuadTransform;\n\
uniform mat4 uLayerTransform;\n\
uniform vec4 uRenderTargetOffset;\n\
attribute vec4 aVertexCoord;\n\
#ifndef ENABLE_RENDER_COLOR\n\
uniform mat4 uTextureTransform;\n\
attribute vec2 aTexCoord;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
#endif\n\
#ifdef ENABLE_MASK\n\
uniform mat4 uMaskQuadTransform;\n\
varying vec3 vMaskCoord;\n\
#endif\n\
void main()\n\
{\n\
vec4 finalPosition = aVertexCoord;\n\
finalPosition = uLayerQuadTransform * finalPosition;\n\
finalPosition = uLayerTransform * finalPosition;\n\
finalPosition.xyz /= finalPosition.w;\n\
#ifdef ENABLE_MASK\n\
vMaskCoord.xy = (uMaskQuadTransform * vec4(finalPosition.xyz, 1.0)).xy;\n\
// correct for perspective correct interpolation, see comment in D3D10 shader\n\
vMaskCoord.z = 1.0;\n\
vMaskCoord *= finalPosition.w;\n\
#endif\n\
finalPosition = finalPosition - uRenderTargetOffset;\n\
finalPosition.xyz *= finalPosition.w;\n\
finalPosition = uMatrixProj * finalPosition;\n\
#ifndef ENABLE_RENDER_COLOR\n\
vTexCoord = (uTextureTransform * vec4(aTexCoord.x, aTexCoord.y, 0.0, 1.0)).xy;\n\
#endif\n\
gl_Position = finalPosition;\n\
}\n\
";

static const char sUnifiedLayerFS[] = "/* sUnifiedLayerFS */\n\
#ifdef GL_ES\n\
precision lowp float;\n\
#endif\n\
#ifdef ENABLE_RENDER_COLOR\n\
uniform vec4 uRenderColor;\n\
#else\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
#ifdef ENABLE_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
#endif\n\
#ifdef ENABLE_TEXTURE_RECT\n\
#extension GL_ARB_texture_rectangle : require\n\
#define sampler2D sampler2DRect\n\
#define texture2D texture2DRect\n\
#endif\n\
#ifdef ENABLE_TEXTURE_EXTERNAL\n\
#extension GL_OES_EGL_image_external : require\n\
#define sampler2D samplerExternalOES\n\
#endif\n\
#ifdef ENABLE_TEXTURE_YCBCR\n\
uniform sampler2D uYTexture;\n\
uniform sampler2D uCbTexture;\n\
uniform sampler2D uCrTexture;\n\
#else\n\
#ifdef ENABLE_TEXTURE_COMPONENT_ALPHA\n\
uniform sampler2D uBlackTexture;\n\
uniform sampler2D uWhiteTexture;\n\
uniform bool uTexturePass2;\n\
#else\n\
uniform sampler2D uTexture;\n\
#endif\n\
#endif\n\
#ifdef ENABLE_BLUR\n\
uniform bool uBlurAlpha;\n\
uniform vec2 uBlurRadius;\n\
uniform vec2 uBlurOffset;\n\
#define GAUSSIAN_KERNEL_HALF_WIDTH 11\n\
#define GAUSSIAN_KERNEL_STEP 0.2\n\
uniform float uBlurGaussianKernel[GAUSSIAN_KERNEL_HALF_WIDTH];\n\
#endif\n\
#ifdef ENABLE_COLOR_MATRIX\n\
uniform mat4 uColorMatrix;\n\
uniform vec4 uColorMatrixVector;\n\
#endif\n\
#ifdef ENABLE_MASK\n\
varying vec3 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
#endif\n\
vec4 sample(vec2 coord)\n\
{\n\
vec4 color;\n\
#ifdef ENABLE_TEXTURE_YCBCR\n\
float y = texture2D(uYTexture, coord).r;\n\
float cb = texture2D(uCbTexture, coord).r;\n\
float cr = texture2D(uCrTexture, coord).r;\n\
y = (y - 0.0625) * 1.164;\n\
cb = cb - 0.5;\n\
cr = cr - 0.5;\n\
color.r = y + cr * 1.596;\n\
color.g = y - 0.813 * cr - 0.391 * cb;\n\
color.b = y + cb * 2.018;\n\
color.a = 1.0;\n\
#else\n\
#ifdef ENABLE_TEXTURE_COMPONENT_ALPHA\n\
vec3 onBlack = texture2D(uBlackTexture, coord).rgb;\n\
vec3 onWhite = texture2D(uWhiteTexture, coord).rgb;\n\
vec4 alphas = (1.0 - onWhite + onBlack).rgbg;\n\
if (uTexturePass2)\n\
color = vec4(onBlack, alphas.a);\n\
else\n\
color = alphas;\n\
#else\n\
color = texture2D(uTexture, coord);\n\
#endif\n\
#endif\n\
#ifdef ENABLE_TEXTURE_RB_SWAP\n\
color = color.bgra;\n\
#endif\n\
#ifdef ENABLE_TEXTURE_NO_ALPHA\n\
color = vec4(color.rgb, 1.0);\n\
#endif\n\
return color;\n\
}\n\
#ifdef ENABLE_BLUR\n\
vec4 sampleAtRadius(vec2 coord, float radius) {\n\
coord += uBlurOffset;\n\
coord += radius * uBlurRadius;\n\
if (coord.x < 0. || coord.y < 0. || coord.x > 1. || coord.y > 1.)\n\
return vec4(0, 0, 0, 0);\n\
return sample(coord);\n\
}\n\
vec4 blur(vec4 color, vec2 coord) {\n\
vec4 total = color * uBlurGaussianKernel[0];\n\
for (int i = 1; i < GAUSSIAN_KERNEL_HALF_WIDTH; ++i) {\n\
float r = float(i) * GAUSSIAN_KERNEL_STEP;\n\
float k = uBlurGaussianKernel[i];\n\
total += sampleAtRadius(coord, r) * k;\n\
total += sampleAtRadius(coord, -r) * k;\n\
}\n\
if (uBlurAlpha) {\n\
color *= total.a;\n\
} else {\n\
color = total;\n\
}\n\
return color;\n\
}\n\
#endif\n\
void main()\n\
{\n\
vec4 color;\n\
#ifdef ENABLE_RENDER_COLOR\n\
color = uRenderColor;\n\
#else\n\
color = sample(vTexCoord);\n\
#ifdef ENABLE_BLUR\n\
color = blur(color, vTexCoord);\n\
#endif\n\
#ifdef ENABLE_COLOR_MATRIX\n\
color = uColorMatrix * vec4(color.rgb / color.a, color.a) + uColorMatrixVector;\n\
color.rgb *= color.a;\n\
#endif\n\
#ifdef ENABLE_OPACITY\n\
color *= uLayerOpacity;\n\
#endif\n\
#endif\n\
#ifdef ENABLE_MASK\n\
vec2 maskCoords = vMaskCoord.xy / vMaskCoord.z;\n\
color *= texture2D(uMaskTexture, maskCoords).r;\n\
#endif\n\
gl_FragColor = color;\n\
}\n\
";

static const char sComponentPass1FS[] = "/* sComponentPass1FS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform sampler2D uBlackTexture;\n\
uniform sampler2D uWhiteTexture;\n\
void main()\n\
{\n\
vec3 onBlack = texture2D(uBlackTexture, vTexCoord).bgr;\n\
vec3 onWhite = texture2D(uWhiteTexture, vTexCoord).bgr;\n\
vec4 alphas = (1.0 - onWhite + onBlack).rgbg;\n\
float mask = 1.0;\n\
\n\
gl_FragColor = alphas * uLayerOpacity * mask;\n\
}\n\
";

static const char sComponentPassMask1FS[] = "/* sComponentPassMask1FS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
uniform sampler2D uBlackTexture;\n\
uniform sampler2D uWhiteTexture;\n\
void main()\n\
{\n\
vec3 onBlack = texture2D(uBlackTexture, vTexCoord).bgr;\n\
vec3 onWhite = texture2D(uWhiteTexture, vTexCoord).bgr;\n\
vec4 alphas = (1.0 - onWhite + onBlack).rgbg;\n\
float mask = texture2D(uMaskTexture, vMaskCoord).r;\n\
\n\
gl_FragColor = alphas * uLayerOpacity * mask;\n\
}\n\
";

static const char sComponentPass1RGBFS[] = "/* sComponentPass1RGBFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform sampler2D uBlackTexture;\n\
uniform sampler2D uWhiteTexture;\n\
void main()\n\
{\n\
vec3 onBlack = texture2D(uBlackTexture, vTexCoord).rgb;\n\
vec3 onWhite = texture2D(uWhiteTexture, vTexCoord).rgb;\n\
vec4 alphas = (1.0 - onWhite + onBlack).rgbg;\n\
float mask = 1.0;\n\
\n\
gl_FragColor = alphas * uLayerOpacity * mask;\n\
}\n\
";

static const char sComponentPassMask1RGBFS[] = "/* sComponentPassMask1RGBFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
uniform sampler2D uBlackTexture;\n\
uniform sampler2D uWhiteTexture;\n\
void main()\n\
{\n\
vec3 onBlack = texture2D(uBlackTexture, vTexCoord).rgb;\n\
vec3 onWhite = texture2D(uWhiteTexture, vTexCoord).rgb;\n\
vec4 alphas = (1.0 - onWhite + onBlack).rgbg;\n\
float mask = texture2D(uMaskTexture, vMaskCoord).r;\n\
\n\
gl_FragColor = alphas * uLayerOpacity * mask;\n\
}\n\
";

static const char sComponentPass2FS[] = "/* sComponentPass2FS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform sampler2D uBlackTexture;\n\
uniform sampler2D uWhiteTexture;\n\
void main()\n\
{\n\
vec3 onBlack = texture2D(uBlackTexture, vTexCoord).bgr;\n\
vec3 onWhite = texture2D(uWhiteTexture, vTexCoord).bgr;\n\
vec4 alphas = (1.0 - onWhite + onBlack).rgbg;\n\
float mask = 1.0;\n\
\n\
gl_FragColor = vec4(onBlack, alphas.a) * uLayerOpacity * mask;\n\
}\n\
";

static const char sComponentPassMask2FS[] = "/* sComponentPassMask2FS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
uniform sampler2D uBlackTexture;\n\
uniform sampler2D uWhiteTexture;\n\
void main()\n\
{\n\
vec3 onBlack = texture2D(uBlackTexture, vTexCoord).bgr;\n\
vec3 onWhite = texture2D(uWhiteTexture, vTexCoord).bgr;\n\
vec4 alphas = (1.0 - onWhite + onBlack).rgbg;\n\
float mask = texture2D(uMaskTexture, vMaskCoord).r;\n\
\n\
gl_FragColor = vec4(onBlack, alphas.a) * uLayerOpacity * mask;\n\
}\n\
";

static const char sComponentPass2RGBFS[] = "/* sComponentPass2RGBFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
uniform sampler2D uBlackTexture;\n\
uniform sampler2D uWhiteTexture;\n\
void main()\n\
{\n\
vec3 onBlack = texture2D(uBlackTexture, vTexCoord).rgb;\n\
vec3 onWhite = texture2D(uWhiteTexture, vTexCoord).rgb;\n\
vec4 alphas = (1.0 - onWhite + onBlack).rgbg;\n\
float mask = 1.0;\n\
\n\
gl_FragColor = vec4(onBlack, alphas.a) * uLayerOpacity * mask;\n\
}\n\
";

static const char sComponentPassMask2RGBFS[] = "/* sComponentPassMask2RGBFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
uniform float uLayerOpacity;\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
uniform sampler2D uBlackTexture;\n\
uniform sampler2D uWhiteTexture;\n\
void main()\n\
{\n\
vec3 onBlack = texture2D(uBlackTexture, vTexCoord).rgb;\n\
vec3 onWhite = texture2D(uWhiteTexture, vTexCoord).rgb;\n\
vec4 alphas = (1.0 - onWhite + onBlack).rgbg;\n\
float mask = texture2D(uMaskTexture, vMaskCoord).r;\n\
\n\
gl_FragColor = vec4(onBlack, alphas.a) * uLayerOpacity * mask;\n\
}\n\
";

static const char sCopyVS[] = "/* sCopyVS */\n\
/* Vertex Shader */\n\
attribute vec4 aVertexCoord;\n\
attribute vec2 aTexCoord;\n\
varying vec2 vTexCoord;\n\
void main()\n\
{\n\
gl_Position = aVertexCoord;\n\
vTexCoord = aTexCoord;\n\
}\n\
";

static const char sCopy2DFS[] = "/* sCopy2DFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
varying vec2 vTexCoord;\n\
uniform sampler2D uTexture;\n\
void main()\n\
{\n\
gl_FragColor = texture2D(uTexture, vTexCoord);\n\
}\n\
";

static const char sCopy2DRectFS[] = "/* sCopy2DRectFS */\n\
#extension GL_ARB_texture_rectangle : enable\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
#ifdef MEDIUMP_SHADER\n\
precision mediump float;\n\
#else\n\
precision lowp float;\n\
#endif\n\
#endif\n\
\n\
varying vec2 vTexCoord;\n\
uniform vec2 uTexCoordMultiplier;\n\
#ifndef GL_ES\n\
uniform sampler2DRect uTexture;\n\
void main()\n\
{\n\
gl_FragColor = texture2DRect(uTexture, vTexCoord * uTexCoordMultiplier);\n\
}\n\
#else\n\
void main()\n\
{\n\
gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n\
}\n\
#endif\n\
";

