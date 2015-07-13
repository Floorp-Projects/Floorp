/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OGLShaderProgram.h"
#include <stdint.h>                     // for uint32_t
#include <sstream>                      // for ostringstream
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/DebugOnly.h"          // for DebugOnly
#include "nsAString.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsString.h"                   // for nsAutoCString
#include "prenv.h"                      // for PR_GetEnv
#include "Layers.h"
#include "GLContext.h"

struct gfxRGBA;

namespace mozilla {
namespace layers {

using namespace std;

#define GAUSSIAN_KERNEL_HALF_WIDTH 11
#define GAUSSIAN_KERNEL_STEP 0.2

void
AddUniforms(ProgramProfileOGL& aProfile)
{
    // This needs to be kept in sync with the KnownUniformName enum
    static const char *sKnownUniformNames[] = {
        "uLayerTransform",
        "uLayerTransformInverse",
        "uMaskTransform",
        "uLayerRects",
        "uMatrixProj",
        "uTextureTransform",
        "uTextureRects",
        "uRenderTargetOffset",
        "uLayerOpacity",
        "uTexture",
        "uYTexture",
        "uCbTexture",
        "uCrTexture",
        "uBlackTexture",
        "uWhiteTexture",
        "uMaskTexture",
        "uRenderColor",
        "uTexCoordMultiplier",
        "uTexturePass2",
        "uColorMatrix",
        "uColorMatrixVector",
        "uBlurRadius",
        "uBlurOffset",
        "uBlurAlpha",
        "uBlurGaussianKernel",
        "uSSEdges",
        "uViewportSize",
        "uVisibleCenter",
        nullptr
    };

    for (int i = 0; sKnownUniformNames[i] != nullptr; ++i) {
        aProfile.mUniforms[i].mNameString = sKnownUniformNames[i];
        aProfile.mUniforms[i].mName = (KnownUniform::KnownUniformName) i;
    }
}

void
ShaderConfigOGL::SetRenderColor(bool aEnabled)
{
  SetFeature(ENABLE_RENDER_COLOR, aEnabled);
}

void
ShaderConfigOGL::SetTextureTarget(GLenum aTarget)
{
  SetFeature(ENABLE_TEXTURE_EXTERNAL | ENABLE_TEXTURE_RECT, false);
  switch (aTarget) {
  case LOCAL_GL_TEXTURE_EXTERNAL:
    SetFeature(ENABLE_TEXTURE_EXTERNAL, true);
    break;
  case LOCAL_GL_TEXTURE_RECTANGLE_ARB:
    SetFeature(ENABLE_TEXTURE_RECT, true);
    break;
  }
}

void
ShaderConfigOGL::SetRBSwap(bool aEnabled)
{
  SetFeature(ENABLE_TEXTURE_RB_SWAP, aEnabled);
}

void
ShaderConfigOGL::SetNoAlpha(bool aEnabled)
{
  SetFeature(ENABLE_TEXTURE_NO_ALPHA, aEnabled);
}

void
ShaderConfigOGL::SetOpacity(bool aEnabled)
{
  SetFeature(ENABLE_OPACITY, aEnabled);
}

void
ShaderConfigOGL::SetYCbCr(bool aEnabled)
{
  SetFeature(ENABLE_TEXTURE_YCBCR, aEnabled);
}

void
ShaderConfigOGL::SetComponentAlpha(bool aEnabled)
{
  SetFeature(ENABLE_TEXTURE_COMPONENT_ALPHA, aEnabled);
}

void
ShaderConfigOGL::SetColorMatrix(bool aEnabled)
{
  SetFeature(ENABLE_COLOR_MATRIX, aEnabled);
}

void
ShaderConfigOGL::SetBlur(bool aEnabled)
{
  SetFeature(ENABLE_BLUR, aEnabled);
}

void
ShaderConfigOGL::SetMask2D(bool aEnabled)
{
  SetFeature(ENABLE_MASK_2D, aEnabled);
}

void
ShaderConfigOGL::SetMask3D(bool aEnabled)
{
  SetFeature(ENABLE_MASK_3D, aEnabled);
}

void
ShaderConfigOGL::SetPremultiply(bool aEnabled)
{
  SetFeature(ENABLE_PREMULTIPLY, aEnabled);
}

void
ShaderConfigOGL::SetDEAA(bool aEnabled)
{
  SetFeature(ENABLE_DEAA, aEnabled);
}

/* static */ ProgramProfileOGL
ProgramProfileOGL::GetProfileFor(ShaderConfigOGL aConfig)
{
  ProgramProfileOGL result;
  ostringstream fs, vs;

  AddUniforms(result);

  vs << "uniform mat4 uMatrixProj;" << endl;
  vs << "uniform vec4 uLayerRects[4];" << endl;
  vs << "uniform mat4 uLayerTransform;" << endl;
  if (aConfig.mFeatures & ENABLE_DEAA) {
    vs << "uniform mat4 uLayerTransformInverse;" << endl;
    vs << "uniform vec3 uSSEdges[4];" << endl;
    vs << "uniform vec2 uVisibleCenter;" << endl;
    vs << "uniform vec2 uViewportSize;" << endl;
  }
  vs << "uniform vec2 uRenderTargetOffset;" << endl;
  vs << "attribute vec4 aCoord;" << endl;

  if (!(aConfig.mFeatures & ENABLE_RENDER_COLOR)) {
    vs << "uniform mat4 uTextureTransform;" << endl;
    vs << "uniform vec4 uTextureRects[4];" << endl;
    vs << "varying vec2 vTexCoord;" << endl;
  }

  if (aConfig.mFeatures & ENABLE_MASK_2D ||
      aConfig.mFeatures & ENABLE_MASK_3D) {
    vs << "uniform mat4 uMaskTransform;" << endl;
    vs << "varying vec3 vMaskCoord;" << endl;
  }

  vs << "void main() {" << endl;
  vs << "  int vertexID = int(aCoord.w);" << endl;
  vs << "  vec4 layerRect = uLayerRects[vertexID];" << endl;
  vs << "  vec4 finalPosition = vec4(aCoord.xy * layerRect.zw + layerRect.xy, 0.0, 1.0);" << endl;
  vs << "  finalPosition = uLayerTransform * finalPosition;" << endl;

  if (aConfig.mFeatures & ENABLE_DEAA) {
    // XXX kip - The DEAA shader could be made simpler if we switch to
    //           using dynamic vertex buffers instead of sending everything
    //           in through uniforms.  This would enable passing information
    //           about how to dilate each vertex explicitly and eliminate the
    //           need to extrapolate this with the sub-pixel coverage
    //           calculation in the vertex shader.

    // Calculate the screen space position of this vertex, in screen pixels
    vs << "  vec4 ssPos = finalPosition;" << endl;
    vs << "  ssPos.xy -= uRenderTargetOffset * finalPosition.w;" << endl;
    vs << "  ssPos = uMatrixProj * ssPos;" << endl;
    vs << "  ssPos.xy = ((ssPos.xy/ssPos.w)*0.5+0.5)*uViewportSize;" << endl;

    if (aConfig.mFeatures & ENABLE_MASK_2D ||
        aConfig.mFeatures & ENABLE_MASK_3D ||
        !(aConfig.mFeatures & ENABLE_RENDER_COLOR)) {
      vs << "  vec4 coordAdjusted;" << endl;
      vs << "  coordAdjusted.xy = aCoord.xy;" << endl;
    }

    // It is necessary to dilate edges away from uVisibleCenter to ensure that
    // fragments with less than 50% sub-pixel coverage will be shaded.
    // This offset is applied when the sub-pixel coverage of the vertex is
    // less than 100%.  Expanding by 0.5 pixels in screen space is sufficient
    // to include these pixels.
    vs << "  if (dot(uSSEdges[0], vec3(ssPos.xy, 1.0)) < 1.5 ||" << endl;
    vs << "      dot(uSSEdges[1], vec3(ssPos.xy, 1.0)) < 1.5 ||" << endl;
    vs << "      dot(uSSEdges[2], vec3(ssPos.xy, 1.0)) < 1.5 ||" << endl;
    vs << "      dot(uSSEdges[3], vec3(ssPos.xy, 1.0)) < 1.5) {" << endl;
    // If the shader reaches this branch, then this vertex is on the edge of
    // the layer's visible rect and should be dilated away from the center of
    // the visible rect.  We don't want to hit this for inner facing
    // edges between tiles, as the pixels may be covered twice without clipping
    // against uSSEdges.  If all edges were dilated, it would result in
    // artifacts visible within semi-transparent layers with multiple tiles.
    vs << "    vec4 visibleCenter = uLayerTransform * vec4(uVisibleCenter, 0.0, 1.0);" << endl;
    vs << "    vec2 dilateDir = finalPosition.xy / finalPosition.w - visibleCenter.xy / visibleCenter.w;" << endl;
    vs << "    vec2 offset = sign(dilateDir) * 0.5;" << endl;
    vs << "    finalPosition.xy += offset * finalPosition.w;" << endl;
    if (!(aConfig.mFeatures & ENABLE_RENDER_COLOR)) {
      // We must adjust the texture coordinates to compensate for the dilation
      vs << "    coordAdjusted = uLayerTransformInverse * finalPosition;" << endl;
      vs << "    coordAdjusted /= coordAdjusted.w;" << endl;
      vs << "    coordAdjusted.xy -= layerRect.xy;" << endl;
      vs << "    coordAdjusted.xy /= layerRect.zw;" << endl;
    }
    vs << "  }" << endl;

    if (!(aConfig.mFeatures & ENABLE_RENDER_COLOR)) {
      vs << "  vec4 textureRect = uTextureRects[vertexID];" << endl;
      vs << "  vec2 texCoord = coordAdjusted.xy * textureRect.zw + textureRect.xy;" << endl;
      vs << "  vTexCoord = (uTextureTransform * vec4(texCoord, 0.0, 1.0)).xy;" << endl;
    }

  } else if (!(aConfig.mFeatures & ENABLE_RENDER_COLOR)) {
    vs << "  vec4 textureRect = uTextureRects[vertexID];" << endl;
    vs << "  vec2 texCoord = aCoord.xy * textureRect.zw + textureRect.xy;" << endl;
    vs << "  vTexCoord = (uTextureTransform * vec4(texCoord, 0.0, 1.0)).xy;" << endl;
  }
  if (aConfig.mFeatures & ENABLE_MASK_2D ||
      aConfig.mFeatures & ENABLE_MASK_3D) {
    vs << "  vMaskCoord.xy = (uMaskTransform * (finalPosition / finalPosition.w)).xy;" << endl;
    if (aConfig.mFeatures & ENABLE_MASK_3D) {
      // correct for perspective correct interpolation, see comment in D3D10 shader
      vs << "  vMaskCoord.z = 1.0;" << endl;
      vs << "  vMaskCoord *= finalPosition.w;" << endl;
    }
  }
  vs << "  finalPosition.xy -= uRenderTargetOffset * finalPosition.w;" << endl;
  vs << "  finalPosition = uMatrixProj * finalPosition;" << endl;
  vs << "  gl_Position = finalPosition;" << endl;
  vs << "}" << endl;

  if (aConfig.mFeatures & ENABLE_TEXTURE_RECT) {
    fs << "#extension GL_ARB_texture_rectangle : require" << endl;
  }
  if (aConfig.mFeatures & ENABLE_TEXTURE_EXTERNAL) {
    fs << "#extension GL_OES_EGL_image_external : require" << endl;
  }
  fs << "#ifdef GL_ES" << endl;
  fs << "precision mediump float;" << endl;
  fs << "#define COLOR_PRECISION lowp" << endl;
  fs << "#else" << endl;
  fs << "#define COLOR_PRECISION" << endl;
  fs << "#endif" << endl;
  if (aConfig.mFeatures & ENABLE_RENDER_COLOR) {
    fs << "uniform COLOR_PRECISION vec4 uRenderColor;" << endl;
  } else {
    // for tiling, texcoord can be greater than the lowfp range
    fs << "varying vec2 vTexCoord;" << endl;
    if (aConfig.mFeatures & ENABLE_BLUR) {
      fs << "uniform bool uBlurAlpha;" << endl;
      fs << "uniform vec2 uBlurRadius;" << endl;
      fs << "uniform vec2 uBlurOffset;" << endl;
      fs << "uniform float uBlurGaussianKernel[" << GAUSSIAN_KERNEL_HALF_WIDTH << "];" << endl;
    }
    if (aConfig.mFeatures & ENABLE_COLOR_MATRIX) {
      fs << "uniform mat4 uColorMatrix;" << endl;
      fs << "uniform vec4 uColorMatrixVector;" << endl;
    }
    if (aConfig.mFeatures & ENABLE_OPACITY) {
      fs << "uniform COLOR_PRECISION float uLayerOpacity;" << endl;
    }
  }

  const char *sampler2D = "sampler2D";
  const char *texture2D = "texture2D";

  if (aConfig.mFeatures & ENABLE_TEXTURE_RECT) {
    fs << "uniform vec2 uTexCoordMultiplier;" << endl;
    sampler2D = "sampler2DRect";
    texture2D = "texture2DRect";
  }

  if (aConfig.mFeatures & ENABLE_TEXTURE_EXTERNAL) {
    sampler2D = "samplerExternalOES";
  }

  if (aConfig.mFeatures & ENABLE_TEXTURE_YCBCR) {
    fs << "uniform sampler2D uYTexture;" << endl;
    fs << "uniform sampler2D uCbTexture;" << endl;
    fs << "uniform sampler2D uCrTexture;" << endl;
  } else if (aConfig.mFeatures & ENABLE_TEXTURE_COMPONENT_ALPHA) {
    fs << "uniform sampler2D uBlackTexture;" << endl;
    fs << "uniform sampler2D uWhiteTexture;" << endl;
    fs << "uniform bool uTexturePass2;" << endl;
  } else {
    fs << "uniform " << sampler2D << " uTexture;" << endl;
  }

  if (aConfig.mFeatures & ENABLE_MASK_2D ||
      aConfig.mFeatures & ENABLE_MASK_3D) {
    fs << "varying vec3 vMaskCoord;" << endl;
    fs << "uniform sampler2D uMaskTexture;" << endl;
  }

  if (aConfig.mFeatures & ENABLE_DEAA) {
    fs << "uniform vec3 uSSEdges[4];" << endl;
  }

  if (!(aConfig.mFeatures & ENABLE_RENDER_COLOR)) {
    fs << "vec4 sample(vec2 coord) {" << endl;
    fs << "  vec4 color;" << endl;
    if (aConfig.mFeatures & ENABLE_TEXTURE_YCBCR) {
      fs << "  COLOR_PRECISION float y = texture2D(uYTexture, coord).r;" << endl;
      fs << "  COLOR_PRECISION float cb = texture2D(uCbTexture, coord).r;" << endl;
      fs << "  COLOR_PRECISION float cr = texture2D(uCrTexture, coord).r;" << endl;

      /* From Rec601:
[R]   [1.1643835616438356,  0.0,                 1.5960267857142858]      [ Y -  16]
[G] = [1.1643835616438358, -0.3917622900949137, -0.8129676472377708]    x [Cb - 128]
[B]   [1.1643835616438356,  2.017232142857143,   8.862867620416422e-17]   [Cr - 128]

For [0,1] instead of [0,255], and to 5 places:
[R]   [1.16438,  0.00000,  1.59603]   [ Y - 0.06275]
[G] = [1.16438, -0.39176, -0.81297] x [Cb - 0.50196]
[B]   [1.16438,  2.01723,  0.00000]   [Cr - 0.50196]
       */
      fs << "  y = (y - 0.06275) * 1.16438;" << endl;
      fs << "  cb = cb - 0.50196;" << endl;
      fs << "  cr = cr - 0.50196;" << endl;
      fs << "  color.r = y + 1.59603*cr;" << endl;
      fs << "  color.g = y - 0.39176*cb - 0.81297*cr;" << endl;
      fs << "  color.b = y + 2.01723*cb;" << endl;
      fs << "  color.a = 1.0;" << endl;
    } else if (aConfig.mFeatures & ENABLE_TEXTURE_COMPONENT_ALPHA) {
      fs << "  COLOR_PRECISION vec3 onBlack = texture2D(uBlackTexture, coord).rgb;" << endl;
      fs << "  COLOR_PRECISION vec3 onWhite = texture2D(uWhiteTexture, coord).rgb;" << endl;
      fs << "  COLOR_PRECISION vec4 alphas = (1.0 - onWhite + onBlack).rgbg;" << endl;
      fs << "  if (uTexturePass2)" << endl;
      fs << "    color = vec4(onBlack, alphas.a);" << endl;
      fs << "  else" << endl;
      fs << "    color = alphas;" << endl;
    } else {
      fs << "  color = " << texture2D << "(uTexture, coord);" << endl;
    }
    if (aConfig.mFeatures & ENABLE_TEXTURE_RB_SWAP) {
      fs << "  color = color.bgra;" << endl;
    }
    if (aConfig.mFeatures & ENABLE_TEXTURE_NO_ALPHA) {
      fs << "  color = vec4(color.rgb, 1.0);" << endl;
    }
    fs << "  return color;" << endl;
    fs << "}" << endl;
    if (aConfig.mFeatures & ENABLE_BLUR) {
      fs << "vec4 sampleAtRadius(vec2 coord, float radius) {" << endl;
      fs << "  coord += uBlurOffset;" << endl;
      fs << "  coord += radius * uBlurRadius;" << endl;
      fs << "  if (coord.x < 0. || coord.y < 0. || coord.x > 1. || coord.y > 1.)" << endl;
      fs << "    return vec4(0, 0, 0, 0);" << endl;
      fs << "  return sample(coord);" << endl;
      fs << "}" << endl;
      fs << "vec4 blur(vec4 color, vec2 coord) {" << endl;
      fs << "  vec4 total = color * uBlurGaussianKernel[0];" << endl;
      fs << "  for (int i = 1; i < " << GAUSSIAN_KERNEL_HALF_WIDTH << "; ++i) {" << endl;
      fs << "    float r = float(i) * " << GAUSSIAN_KERNEL_STEP << ";" << endl;
      fs << "    float k = uBlurGaussianKernel[i];" << endl;
      fs << "    total += sampleAtRadius(coord, r) * k;" << endl;
      fs << "    total += sampleAtRadius(coord, -r) * k;" << endl;
      fs << "  }" << endl;
      fs << "  if (uBlurAlpha) {" << endl;
      fs << "    color *= total.a;" << endl;
      fs << "  } else {" << endl;
      fs << "    color = total;" << endl;
      fs << "  }" << endl;
      fs << "  return color;" << endl;
      fs << "}" << endl;
    }
  }
  fs << "void main() {" << endl;
  if (aConfig.mFeatures & ENABLE_RENDER_COLOR) {
    fs << "  vec4 color = uRenderColor;" << endl;
  } else {
    if (aConfig.mFeatures & ENABLE_TEXTURE_RECT) {
      fs << "  vec4 color = sample(vTexCoord * uTexCoordMultiplier);" << endl;
    } else {
      fs << "  vec4 color = sample(vTexCoord);" << endl;
    }
    if (aConfig.mFeatures & ENABLE_BLUR) {
      fs << "  color = blur(color, vTexCoord);" << endl;
    }
    if (aConfig.mFeatures & ENABLE_COLOR_MATRIX) {
      fs << "  color = uColorMatrix * vec4(color.rgb / color.a, color.a) + uColorMatrixVector;" << endl;
      fs << "  color.rgb *= color.a;" << endl;
    }
    if (aConfig.mFeatures & ENABLE_OPACITY) {
      fs << "  color *= uLayerOpacity;" << endl;
    }
    if (aConfig.mFeatures & ENABLE_PREMULTIPLY) {
      fs << " color.rgb *= color.a;" << endl;
    }
  }
  if (aConfig.mFeatures & ENABLE_DEAA) {
    // Calculate the sub-pixel coverage of the pixel and modulate its opacity
    // by that amount to perform DEAA.
    fs << "  vec3 ssPos = vec3(gl_FragCoord.xy, 1.0);" << endl;
    fs << "  float deaaCoverage = clamp(dot(uSSEdges[0], ssPos), 0.0, 1.0);" << endl;
    fs << "  deaaCoverage *= clamp(dot(uSSEdges[1], ssPos), 0.0, 1.0);" << endl;
    fs << "  deaaCoverage *= clamp(dot(uSSEdges[2], ssPos), 0.0, 1.0);" << endl;
    fs << "  deaaCoverage *= clamp(dot(uSSEdges[3], ssPos), 0.0, 1.0);" << endl;
    fs << "  color *= deaaCoverage;" << endl;
  }
  if (aConfig.mFeatures & ENABLE_MASK_3D) {
    fs << "  vec2 maskCoords = vMaskCoord.xy / vMaskCoord.z;" << endl;
    fs << "  COLOR_PRECISION float mask = texture2D(uMaskTexture, maskCoords).r;" << endl;
    fs << "  color *= mask;" << endl;
  } else if (aConfig.mFeatures & ENABLE_MASK_2D) {
    fs << "  COLOR_PRECISION float mask = texture2D(uMaskTexture, vMaskCoord.xy).r;" << endl;
    fs << "  color *= mask;" << endl;
  } else {
    fs << "  COLOR_PRECISION float mask = 1.0;" << endl;
    fs << "  color *= mask;" << endl;
  }
  fs << "  gl_FragColor = color;" << endl;
  fs << "}" << endl;

  result.mVertexShaderString = vs.str();
  result.mFragmentShaderString = fs.str();

  if (aConfig.mFeatures & ENABLE_RENDER_COLOR) {
    result.mTextureCount = 0;
  } else {
    if (aConfig.mFeatures & ENABLE_TEXTURE_YCBCR) {
      result.mTextureCount = 3;
    } else if (aConfig.mFeatures & ENABLE_TEXTURE_COMPONENT_ALPHA) {
      result.mTextureCount = 2;
    } else {
      result.mTextureCount = 1;
    }
  }
  if (aConfig.mFeatures & ENABLE_MASK_2D ||
      aConfig.mFeatures & ENABLE_MASK_3D) {
    result.mTextureCount = 1;
  }

  return result;
}

ShaderProgramOGL::ShaderProgramOGL(GLContext* aGL, const ProgramProfileOGL& aProfile)
  : mGL(aGL)
  , mProgram(0)
  , mProfile(aProfile)
  , mProgramState(STATE_NEW)
{
}

ShaderProgramOGL::~ShaderProgramOGL()
{
  if (mProgram <= 0) {
    return;
  }

  nsRefPtr<GLContext> ctx = mGL->GetSharedContext();
  if (!ctx) {
    ctx = mGL;
  }
  ctx->MakeCurrent();
  ctx->fDeleteProgram(mProgram);
}

bool
ShaderProgramOGL::Initialize()
{
  NS_ASSERTION(mProgramState == STATE_NEW, "Shader program has already been initialised");

  ostringstream vs, fs;
  for (uint32_t i = 0; i < mProfile.mDefines.Length(); ++i) {
    vs << mProfile.mDefines[i] << endl;
    fs << mProfile.mDefines[i] << endl;
  }
  vs << mProfile.mVertexShaderString << endl;
  fs << mProfile.mFragmentShaderString << endl;

  if (!CreateProgram(vs.str().c_str(), fs.str().c_str())) {
    mProgramState = STATE_ERROR;
    return false;
  }

  mProgramState = STATE_OK;

  for (uint32_t i = 0; i < KnownUniform::KnownUniformCount; ++i) {
    mProfile.mUniforms[i].mLocation =
      mGL->fGetUniformLocation(mProgram, mProfile.mUniforms[i].mNameString);
  }

  return true;
}

GLint
ShaderProgramOGL::CreateShader(GLenum aShaderType, const char *aShaderSource)
{
  GLint success, len = 0;

  GLint sh = mGL->fCreateShader(aShaderType);
  mGL->fShaderSource(sh, 1, (const GLchar**)&aShaderSource, nullptr);
  mGL->fCompileShader(sh);
  mGL->fGetShaderiv(sh, LOCAL_GL_COMPILE_STATUS, &success);
  mGL->fGetShaderiv(sh, LOCAL_GL_INFO_LOG_LENGTH, (GLint*) &len);
  /* Even if compiling is successful, there may still be warnings.  Print them
   * in a debug build.  The > 10 is to catch silly compilers that might put
   * some whitespace in the log but otherwise leave it empty.
   */
  if (!success
#ifdef DEBUG
      || (len > 10 && PR_GetEnv("MOZ_DEBUG_SHADERS"))
#endif
      )
  {
    nsAutoCString log;
    log.SetCapacity(len);
    mGL->fGetShaderInfoLog(sh, len, (GLint*) &len, (char*) log.BeginWriting());
    log.SetLength(len);

    if (!success) {
      printf_stderr("=== SHADER COMPILATION FAILED ===\n");
    } else {
      printf_stderr("=== SHADER COMPILATION WARNINGS ===\n");
    }

      printf_stderr("=== Source:\n%s\n", aShaderSource);
      printf_stderr("=== Log:\n%s\n", log.get());
      printf_stderr("============\n");

    if (!success) {
      mGL->fDeleteShader(sh);
      return 0;
    }
  }

  return sh;
}

bool
ShaderProgramOGL::CreateProgram(const char *aVertexShaderString,
                                const char *aFragmentShaderString)
{
  GLuint vertexShader = CreateShader(LOCAL_GL_VERTEX_SHADER, aVertexShaderString);
  GLuint fragmentShader = CreateShader(LOCAL_GL_FRAGMENT_SHADER, aFragmentShaderString);

  if (!vertexShader || !fragmentShader)
    return false;

  GLint result = mGL->fCreateProgram();
  mGL->fAttachShader(result, vertexShader);
  mGL->fAttachShader(result, fragmentShader);

  mGL->fLinkProgram(result);

  GLint success, len;
  mGL->fGetProgramiv(result, LOCAL_GL_LINK_STATUS, &success);
  mGL->fGetProgramiv(result, LOCAL_GL_INFO_LOG_LENGTH, (GLint*) &len);
  /* Even if linking is successful, there may still be warnings.  Print them
   * in a debug build.  The > 10 is to catch silly compilers that might put
   * some whitespace in the log but otherwise leave it empty.
   */
  if (!success
#ifdef DEBUG
      || (len > 10 && PR_GetEnv("MOZ_DEBUG_SHADERS"))
#endif
      )
  {
    nsAutoCString log;
    log.SetCapacity(len);
    mGL->fGetProgramInfoLog(result, len, (GLint*) &len, (char*) log.BeginWriting());
    log.SetLength(len);

    if (!success) {
      printf_stderr("=== PROGRAM LINKING FAILED ===\n");
    } else {
      printf_stderr("=== PROGRAM LINKING WARNINGS ===\n");
    }
    printf_stderr("=== Log:\n%s\n", log.get());
    printf_stderr("============\n");
  }

  // We can mark the shaders for deletion; they're attached to the program
  // and will remain attached.
  mGL->fDeleteShader(vertexShader);
  mGL->fDeleteShader(fragmentShader);

  if (!success) {
    mGL->fDeleteProgram(result);
    return false;
  }

  mProgram = result;
  return true;
}

GLuint
ShaderProgramOGL::GetProgram()
{
  if (mProgramState == STATE_NEW) {
    if (!Initialize()) {
      NS_WARNING("Shader could not be initialised");
    }
  }
  MOZ_ASSERT(HasInitialized(), "Attempting to get a program that's not been initialized!");
  return mProgram;
}

void
ShaderProgramOGL::SetBlurRadius(float aRX, float aRY)
{
  float f[] = {aRX, aRY};
  SetUniform(KnownUniform::BlurRadius, 2, f);

  float gaussianKernel[GAUSSIAN_KERNEL_HALF_WIDTH];
  float sum = 0.0f;
  for (int i = 0; i < GAUSSIAN_KERNEL_HALF_WIDTH; i++) {
    float x = i * GAUSSIAN_KERNEL_STEP;
    float sigma = 1.0f;
    gaussianKernel[i] = exp(-x * x / (2 * sigma * sigma)) / sqrt(2 * M_PI * sigma * sigma);
    sum += gaussianKernel[i] * (i == 0 ? 1 : 2);
  }
  for (int i = 0; i < GAUSSIAN_KERNEL_HALF_WIDTH; i++) {
    gaussianKernel[i] /= sum;
  }
  SetArrayUniform(KnownUniform::BlurGaussianKernel, GAUSSIAN_KERNEL_HALF_WIDTH, gaussianKernel);
}

} // namespace layers
} // namespace mozilla
