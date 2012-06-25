/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* AUTOMATICALLY GENERATED from LayerManagerOGLShaders.txt */
/* DO NOT EDIT! */

static const char sLayerVS[] = "/* sLayerVS */\n\
/* Vertex Shader */\n\
uniform mat4 uMatrixProj;\n\
uniform mat4 uLayerQuadTransform;\n\
uniform mat4 uLayerTransform;\n\
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
vTexCoord = aTexCoord;\n\
gl_Position = finalPosition;\n\
}\n\
";

static const char sLayerMaskVS[] = "/* sLayerMaskVS */\n\
/* Vertex Shader */\n\
uniform mat4 uMatrixProj;\n\
uniform mat4 uLayerQuadTransform;\n\
uniform mat4 uLayerTransform;\n\
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
vTexCoord = aTexCoord;\n\
gl_Position = finalPosition;\n\
}\n\
";

static const char sLayerMask3DVS[] = "/* sLayerMask3DVS */\n\
/* Vertex Shader */\n\
uniform mat4 uMatrixProj;\n\
uniform mat4 uLayerQuadTransform;\n\
uniform mat4 uLayerTransform;\n\
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
vTexCoord = aTexCoord;\n\
gl_Position = finalPosition;\n\
}\n\
";

static const char sSolidColorLayerFS[] = "/* sSolidColorLayerFS */\n\
#define NO_LAYER_OPACITY 1\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
float mask = texture2D(uMaskTexture, vMaskCoord).a;\n\
\n\
gl_FragColor = mask * uRenderColor;\n\
}\n\
";

static const char sRGBATextureLayerFS[] = "/* sRGBATextureLayerFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
float mask = texture2D(uMaskTexture, vMaskCoord).a;\n\
\n\
gl_FragColor = texture2D(uTexture, vTexCoord) * uLayerOpacity * mask;\n\
}\n\
";

static const char sRGBATextureLayerMask3DFS[] = "/* sRGBATextureLayerMask3DFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
float mask = texture2D(uMaskTexture, maskCoords).a;\n\
\n\
gl_FragColor = texture2D(uTexture, vTexCoord) * uLayerOpacity * mask;\n\
}\n\
";

static const char sRGBARectTextureLayerFS[] = "/* sRGBARectTextureLayerFS */\n\
#extension GL_ARB_texture_rectangle : enable\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
float mask = texture2D(uMaskTexture, vMaskCoord).a;\n\
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
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
float mask = texture2D(uMaskTexture, maskCoords).a;\n\
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

static const char sBGRATextureLayerFS[] = "/* sBGRATextureLayerFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
float mask = texture2D(uMaskTexture, vMaskCoord).a;\n\
\n\
gl_FragColor = texture2D(uTexture, vTexCoord).bgra * uLayerOpacity * mask;\n\
}\n\
";

static const char sRGBXTextureLayerFS[] = "/* sRGBXTextureLayerFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
float mask = texture2D(uMaskTexture, vMaskCoord).a;\n\
\n\
gl_FragColor = vec4(texture2D(uTexture, vTexCoord).rgb, 1.0) * uLayerOpacity * mask;\n\
}\n\
";

static const char sBGRXTextureLayerFS[] = "/* sBGRXTextureLayerFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
float mask = texture2D(uMaskTexture, vMaskCoord).a;\n\
\n\
gl_FragColor = vec4(texture2D(uTexture, vTexCoord).bgr, 1.0) * uLayerOpacity * mask;\n\
}\n\
";

static const char sYCbCrTextureLayerFS[] = "/* sYCbCrTextureLayerFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
#ifdef GL_ES\n\
precision mediump float;\n\
#endif\n\
uniform sampler2D uYTexture;\n\
uniform sampler2D uCbTexture;\n\
uniform sampler2D uCrTexture;\n\
void main()\n\
{\n\
vec4 yuv;\n\
vec4 color;\n\
yuv.r = texture2D(uCrTexture, vTexCoord).r - 0.5;\n\
yuv.g = texture2D(uYTexture, vTexCoord).r - 0.0625;\n\
yuv.b = texture2D(uCbTexture, vTexCoord).r - 0.5;\n\
color.r = yuv.g * 1.164 + yuv.r * 1.596;\n\
color.g = yuv.g * 1.164 - 0.813 * yuv.r - 0.391 * yuv.b;\n\
color.b = yuv.g * 1.164 + yuv.b * 2.018;\n\
color.a = 1.0;\n\
float mask = 1.0;\n\
\n\
gl_FragColor = color * uLayerOpacity * mask;\n\
}\n\
";

static const char sYCbCrTextureLayerMaskFS[] = "/* sYCbCrTextureLayerMaskFS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
#ifdef GL_ES // for tiling, texcoord can be greater than the lowfp range\n\
varying mediump vec2 vTexCoord;\n\
#else\n\
varying vec2 vTexCoord;\n\
#endif\n\
\n\
varying vec2 vMaskCoord;\n\
uniform sampler2D uMaskTexture;\n\
\n\
#ifdef GL_ES\n\
precision mediump float;\n\
#endif\n\
uniform sampler2D uYTexture;\n\
uniform sampler2D uCbTexture;\n\
uniform sampler2D uCrTexture;\n\
void main()\n\
{\n\
vec4 yuv;\n\
vec4 color;\n\
yuv.r = texture2D(uCrTexture, vTexCoord).r - 0.5;\n\
yuv.g = texture2D(uYTexture, vTexCoord).r - 0.0625;\n\
yuv.b = texture2D(uCbTexture, vTexCoord).r - 0.5;\n\
color.r = yuv.g * 1.164 + yuv.r * 1.596;\n\
color.g = yuv.g * 1.164 - 0.813 * yuv.r - 0.391 * yuv.b;\n\
color.b = yuv.g * 1.164 + yuv.b * 2.018;\n\
color.a = 1.0;\n\
float mask = texture2D(uMaskTexture, vMaskCoord).a;\n\
\n\
gl_FragColor = color * uLayerOpacity * mask;\n\
}\n\
";

static const char sComponentPass1FS[] = "/* sComponentPass1FS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
float mask = texture2D(uMaskTexture, vMaskCoord).a;\n\
\n\
gl_FragColor = alphas * uLayerOpacity * mask;\n\
}\n\
";

static const char sComponentPass2FS[] = "/* sComponentPass2FS */\n\
/* Fragment Shader */\n\
#ifdef GL_ES\n\
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
precision lowp float;\n\
#endif\n\
\n\
#ifndef NO_LAYER_OPACITY\n\
uniform float uLayerOpacity;\n\
#endif\n\
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
float mask = texture2D(uMaskTexture, vMaskCoord).a;\n\
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
precision lowp float;\n\
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
precision lowp float;\n\
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

