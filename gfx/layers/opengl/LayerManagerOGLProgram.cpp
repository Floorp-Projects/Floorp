/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerManagerOGLProgram.h"
#include "LayerManagerOGLShaders.h"

namespace mozilla {
namespace layers {

typedef ProgramProfileOGL::Argument Argument;

// helper method for GetProfileFor
void
AddCommonArgs(ProgramProfileOGL& aProfile)
{
  aProfile.mUniforms.AppendElement(Argument("uLayerTransform"));
  aProfile.mUniforms.AppendElement(Argument("uLayerQuadTransform"));
  aProfile.mUniforms.AppendElement(Argument("uMatrixProj"));
  aProfile.mUniforms.AppendElement(Argument("uRenderTargetOffset"));
  aProfile.mUniforms.AppendElement(Argument("uLayerOpacity"));
  aProfile.mAttributes.AppendElement(Argument("aVertexCoord"));
}


/* static */ ProgramProfileOGL
ProgramProfileOGL::GetProfileFor(gl::ShaderProgramType aType)
{
  ProgramProfileOGL result;

  switch (aType) {
  case gl::RGBALayerProgramType:
    result.mVertexShaderString = sLayerVS;
    result.mFragmentShaderString = sRGBATextureLayerFS;
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uTexture"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    break;
  case gl::BGRALayerProgramType:
    result.mVertexShaderString = sLayerVS;
    result.mFragmentShaderString = sBGRATextureLayerFS;
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uTexture"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    break;
  case gl::RGBXLayerProgramType:
    result.mVertexShaderString = sLayerVS;
    result.mFragmentShaderString = sRGBXTextureLayerFS;
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uTexture"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    break;
  case gl::BGRXLayerProgramType:
    result.mVertexShaderString = sLayerVS;
    result.mFragmentShaderString = sBGRXTextureLayerFS;
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uTexture"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    break;
  case gl::RGBARectLayerProgramType:
    result.mVertexShaderString = sLayerVS;
    result.mFragmentShaderString = sRGBARectTextureLayerFS;
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uTexture"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    break;
  case gl::ColorLayerProgramType:
    result.mVertexShaderString = sLayerVS;
    result.mFragmentShaderString = sSolidColorLayerFS;
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uRenderColor"));
    break;
  case gl::YCbCrLayerProgramType:
    result.mVertexShaderString = sLayerVS;
    result.mFragmentShaderString = sYCbCrTextureLayerFS;
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uYTexture"));
    result.mUniforms.AppendElement(Argument("uCbTexture"));
    result.mUniforms.AppendElement(Argument("uCrTexture"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    break;
  case gl::ComponentAlphaPass1ProgramType:
    result.mVertexShaderString = sLayerVS;
    result.mFragmentShaderString = sComponentPass1FS;
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uBlackTexture"));
    result.mUniforms.AppendElement(Argument("uWhiteTexture"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    break;
  case gl::ComponentAlphaPass2ProgramType:
    result.mVertexShaderString = sLayerVS;
    result.mFragmentShaderString = sComponentPass2FS;
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uBlackTexture"));
    result.mUniforms.AppendElement(Argument("uWhiteTexture"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    break;
  case gl::Copy2DProgramType:
    result.mVertexShaderString = sCopyVS;
    result.mFragmentShaderString = sCopy2DFS;
    result.mUniforms.AppendElement(Argument("uTexture"));
    result.mAttributes.AppendElement(Argument("aVertexCoord"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    break;
  case gl::Copy2DRectProgramType:
    result.mVertexShaderString = sCopyVS;
    result.mFragmentShaderString = sCopy2DRectFS;
    result.mUniforms.AppendElement(Argument("uTexture"));
    result.mAttributes.AppendElement(Argument("aVertexCoord"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    break;
  default:
    NS_NOTREACHED("Unknown shader program type.");
  }

  return result;
}

const char* const ShaderProgramOGL::VertexCoordAttrib = "aVertexCoord";
const char* const ShaderProgramOGL::TexCoordAttrib = "aTexCoord";

bool
ShaderProgramOGL::Initialize()
{
  if (!CreateProgram(mProfile.mVertexShaderString,
                     mProfile.mFragmentShaderString)) {
    return false;
  }

  for (int i = 0; i < mProfile.mUniforms.Length(); ++i) {
    mProfile.mUniforms[i].mLocation =
      mGL->fGetUniformLocation(mProgram, mProfile.mUniforms[i].mName);
    NS_ASSERTION(mProfile.mUniforms[i].mLocation >= 0, "Bad uniform location.");
  }

  for (int i = 0; i < mProfile.mAttributes.Length(); ++i) {
    mProfile.mAttributes[i].mLocation =
      mGL->fGetAttribLocation(mProgram, mProfile.mAttributes[i].mName);
    NS_ASSERTION(mProfile.mAttributes[i].mLocation >= 0, "Bad attribute location.");
  }

  // this is a one-off that's present in the 2DRect versions of some shaders.
  mTexCoordMultiplierUniformLocation =
    mGL->fGetUniformLocation(mProgram, "uTexCoordMultiplier");

  return true;
}

GLint
ShaderProgramOGL::CreateShader(GLenum aShaderType, const char *aShaderSource)
{
  GLint success, len = 0;

  GLint sh = mGL->fCreateShader(aShaderType);
  mGL->fShaderSource(sh, 1, (const GLchar**)&aShaderSource, NULL);
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
    nsCAutoString log;
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
    nsCAutoString log;
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

} /* layers */
} /* mozilla */
