/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OGLShaderProgram.h"
#include <stdint.h>                     // for uint32_t
#include "gfxMatrix.h"                  // for gfxMatrix
#include "gfxPoint.h"                   // for gfxIntSize, gfxPoint, etc
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/DebugOnly.h"          // for DebugOnly
#include "nsAString.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsString.h"                   // for nsAutoCString
#include "prenv.h"                      // for PR_GetEnv
#include "OGLShaders.h"
#include "Layers.h"
#include "GLContext.h"

struct gfxRGBA;

namespace mozilla {
namespace layers {

typedef ProgramProfileOGL::Argument Argument;

// helper methods for GetProfileFor
void
AddCommonArgs(ProgramProfileOGL& aProfile)
{
  aProfile.mUniforms.AppendElement(Argument("uLayerTransform"));
  aProfile.mUniforms.AppendElement(Argument("uLayerQuadTransform"));
  aProfile.mUniforms.AppendElement(Argument("uMatrixProj"));
  aProfile.mHasMatrixProj = true;
  aProfile.mUniforms.AppendElement(Argument("uRenderTargetOffset"));
  aProfile.mAttributes.AppendElement(Argument("aVertexCoord"));
}
void
AddCommonTextureArgs(ProgramProfileOGL& aProfile)
{
  aProfile.mUniforms.AppendElement(Argument("uLayerOpacity"));
  aProfile.mUniforms.AppendElement(Argument("uTexture"));
  aProfile.mUniforms.AppendElement(Argument("uTextureTransform"));
  aProfile.mAttributes.AppendElement(Argument("aTexCoord"));
}

/* static */ ProgramProfileOGL
ProgramProfileOGL::GetProfileFor(ShaderProgramType aType,
                                 MaskType aMask)
{
  NS_ASSERTION(ProgramExists(aType, aMask), "Invalid program type.");
  ProgramProfileOGL result;

  switch (aType) {
  case RGBALayerProgramType:
    if (aMask == Mask3d) {
      result.mVertexShaderString = sLayerMask3DVS;
      result.mFragmentShaderString = sRGBATextureLayerMask3DFS;
    } else if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sRGBATextureLayerMaskFS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sRGBATextureLayerFS;
    }
    AddCommonArgs(result);
    AddCommonTextureArgs(result);
    result.mTextureCount = 1;
    break;
  case BGRALayerProgramType:
    if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sBGRATextureLayerMaskFS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sBGRATextureLayerFS;
    }
    AddCommonArgs(result);
    AddCommonTextureArgs(result);
    result.mTextureCount = 1;
    break;
  case RGBXLayerProgramType:
    if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sRGBXTextureLayerMaskFS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sRGBXTextureLayerFS;
    }
    AddCommonArgs(result);
    AddCommonTextureArgs(result);
    result.mTextureCount = 1;
    break;
  case BGRXLayerProgramType:
    if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sBGRXTextureLayerMaskFS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sBGRXTextureLayerFS;
    }
    AddCommonArgs(result);
    AddCommonTextureArgs(result);
    result.mTextureCount = 1;
    break;
  case RGBARectLayerProgramType:
    if (aMask == Mask3d) {
      result.mVertexShaderString = sLayerMask3DVS;
      result.mFragmentShaderString = sRGBARectTextureLayerMask3DFS;
    } else if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sRGBARectTextureLayerMaskFS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sRGBARectTextureLayerFS;
    }
    AddCommonArgs(result);
    AddCommonTextureArgs(result);
    result.mTextureCount = 1;
    break;
  case RGBXRectLayerProgramType:
    if (aMask == Mask3d) {
      result.mVertexShaderString = sLayerMask3DVS;
      result.mFragmentShaderString = sRGBXRectTextureLayerMask3DFS;
    } else if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sRGBXRectTextureLayerMaskFS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sRGBXRectTextureLayerFS;
    }
    AddCommonArgs(result);
    AddCommonTextureArgs(result);
    result.mTextureCount = 1;
    break;
  case BGRARectLayerProgramType:
    MOZ_ASSERT(aMask == MaskNone, "BGRARectLayerProgramType can't handle masks.");
    result.mVertexShaderString = sLayerVS;
    result.mFragmentShaderString = sBGRARectTextureLayerFS;
    AddCommonArgs(result);
    AddCommonTextureArgs(result);
    result.mTextureCount = 1;
    break;
  case RGBAExternalLayerProgramType:
    if (aMask == Mask3d) {
      result.mVertexShaderString = sLayerMask3DVS;
      result.mFragmentShaderString = sRGBAExternalTextureLayerMask3DFS;
    } else if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sRGBAExternalTextureLayerMaskFS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sRGBAExternalTextureLayerFS;
    }
    AddCommonArgs(result);
    AddCommonTextureArgs(result);
    result.mTextureCount = 1;
    break;
  case ColorLayerProgramType:
    if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sSolidColorLayerMaskFS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sSolidColorLayerFS;
    }
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uRenderColor"));
    break;
  case YCbCrLayerProgramType:
    if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sYCbCrTextureLayerMaskFS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sYCbCrTextureLayerFS;
    }
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uLayerOpacity"));
    result.mUniforms.AppendElement(Argument("uYTexture"));
    result.mUniforms.AppendElement(Argument("uCbTexture"));
    result.mUniforms.AppendElement(Argument("uCrTexture"));
    result.mUniforms.AppendElement(Argument("uTextureTransform"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    result.mTextureCount = 3;
    break;
  case ComponentAlphaPass1ProgramType:
    if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sComponentPassMask1FS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sComponentPass1FS;
    }
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uLayerOpacity"));
    result.mUniforms.AppendElement(Argument("uBlackTexture"));
    result.mUniforms.AppendElement(Argument("uWhiteTexture"));
    result.mUniforms.AppendElement(Argument("uTextureTransform"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    result.mTextureCount = 2;
    break;
  case ComponentAlphaPass1RGBProgramType:
    if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sComponentPassMask1RGBFS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sComponentPass1RGBFS;
    }
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uLayerOpacity"));
    result.mUniforms.AppendElement(Argument("uBlackTexture"));
    result.mUniforms.AppendElement(Argument("uWhiteTexture"));
    result.mUniforms.AppendElement(Argument("uTextureTransform"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    result.mTextureCount = 2;
    break;
  case ComponentAlphaPass2ProgramType:
    if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sComponentPassMask2FS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sComponentPass2FS;
    }
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uLayerOpacity"));
    result.mUniforms.AppendElement(Argument("uBlackTexture"));
    result.mUniforms.AppendElement(Argument("uWhiteTexture"));
    result.mUniforms.AppendElement(Argument("uTextureTransform"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    result.mTextureCount = 2;
    break;
  case ComponentAlphaPass2RGBProgramType:
    if (aMask == Mask2d) {
      result.mVertexShaderString = sLayerMaskVS;
      result.mFragmentShaderString = sComponentPassMask2RGBFS;
    } else {
      result.mVertexShaderString = sLayerVS;
      result.mFragmentShaderString = sComponentPass2RGBFS;
    }
    AddCommonArgs(result);
    result.mUniforms.AppendElement(Argument("uLayerOpacity"));
    result.mUniforms.AppendElement(Argument("uBlackTexture"));
    result.mUniforms.AppendElement(Argument("uWhiteTexture"));
    result.mUniforms.AppendElement(Argument("uTextureTransform"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    result.mTextureCount = 2;
    break;
  case Copy2DProgramType:
    NS_ASSERTION(!aMask, "Program does not have masked variant.");
    result.mVertexShaderString = sCopyVS;
    result.mFragmentShaderString = sCopy2DFS;
    result.mUniforms.AppendElement(Argument("uTexture"));
    result.mUniforms.AppendElement(Argument("uTextureTransform"));
    result.mAttributes.AppendElement(Argument("aVertexCoord"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    result.mTextureCount = 1;
    break;
  case Copy2DRectProgramType:
    NS_ASSERTION(!aMask, "Program does not have masked variant.");
    result.mVertexShaderString = sCopyVS;
    result.mFragmentShaderString = sCopy2DRectFS;
    result.mUniforms.AppendElement(Argument("uTexture"));
    result.mUniforms.AppendElement(Argument("uTextureTransform"));
    result.mAttributes.AppendElement(Argument("aVertexCoord"));
    result.mAttributes.AppendElement(Argument("aTexCoord"));
    result.mTextureCount = 1;
    break;
  default:
    NS_NOTREACHED("Unknown shader program type.");
  }

  if (aMask > MaskNone) {
    result.mUniforms.AppendElement(Argument("uMaskTexture"));
    result.mUniforms.AppendElement(Argument("uMaskQuadTransform"));
    result.mTextureCount += 1;
  }

  return result;
}

const char* const ShaderProgramOGL::VertexCoordAttrib = "aVertexCoord";
const char* const ShaderProgramOGL::TexCoordAttrib = "aTexCoord";

ShaderProgramOGL::ShaderProgramOGL(GLContext* aGL, const ProgramProfileOGL& aProfile)
  : mIsProjectionMatrixStale(false)
  , mGL(aGL)
  , mProgram(0)
  , mProfile(aProfile)
  , mProgramState(STATE_NEW)
{}

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

  if (!CreateProgram(mProfile.mVertexShaderString,
                     mProfile.mFragmentShaderString)) {
    mProgramState = STATE_ERROR;
    return false;
  }

  mProgramState = STATE_OK;

  for (uint32_t i = 0; i < mProfile.mUniforms.Length(); ++i) {
    mProfile.mUniforms[i].mLocation =
      mGL->fGetUniformLocation(mProgram, mProfile.mUniforms[i].mName);
    NS_ASSERTION(mProfile.mUniforms[i].mLocation >= 0, "Bad uniform location.");
  }

  for (uint32_t i = 0; i < mProfile.mAttributes.Length(); ++i) {
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

void
ShaderProgramOGL::Activate()
{
  if (mProgramState == STATE_NEW) {
    if (!Initialize()) {
      NS_WARNING("Shader could not be initialised");
      return;
    }
  }
  NS_ASSERTION(HasInitialized(), "Attempting to activate a program that's not in use!");
  mGL->fUseProgram(mProgram);
#if CHECK_CURRENT_PROGRAM
  mGL->SetUserData(&sCurrentProgramKey, this);
#endif
  // check and set the projection matrix
  if (mIsProjectionMatrixStale) {
    SetProjectionMatrix(mProjectionMatrix);
  }
}


void
ShaderProgramOGL::SetUniform(GLint aLocation, float aFloatValue)
{
  ASSERT_THIS_PROGRAM;
  NS_ASSERTION(aLocation >= 0, "Invalid location");

  mGL->fUniform1f(aLocation, aFloatValue);
}

void
ShaderProgramOGL::SetUniform(GLint aLocation, const gfxRGBA& aColor)
{
  ASSERT_THIS_PROGRAM;
  NS_ASSERTION(aLocation >= 0, "Invalid location");

  mGL->fUniform4f(aLocation, float(aColor.r), float(aColor.g), float(aColor.b), float(aColor.a));
}

void
ShaderProgramOGL::SetUniform(GLint aLocation, int aLength, float *aFloatValues)
{
  ASSERT_THIS_PROGRAM;
  NS_ASSERTION(aLocation >= 0, "Invalid location");

  if (aLength == 1) {
    mGL->fUniform1fv(aLocation, 1, aFloatValues);
  } else if (aLength == 2) {
    mGL->fUniform2fv(aLocation, 1, aFloatValues);
  } else if (aLength == 3) {
    mGL->fUniform3fv(aLocation, 1, aFloatValues);
  } else if (aLength == 4) {
    mGL->fUniform4fv(aLocation, 1, aFloatValues);
  } else {
    NS_NOTREACHED("Bogus aLength param");
  }
}

void
ShaderProgramOGL::SetUniform(GLint aLocation, GLint aIntValue)
{
  ASSERT_THIS_PROGRAM;
  NS_ASSERTION(aLocation >= 0, "Invalid location");

  mGL->fUniform1i(aLocation, aIntValue);
}

void
ShaderProgramOGL::SetMatrixUniform(GLint aLocation, const gfx3DMatrix& aMatrix)
{
  SetMatrixUniform(aLocation, &aMatrix._11);
}

void
ShaderProgramOGL::SetMatrixUniform(GLint aLocation, const float *aFloatValues)
{
  ASSERT_THIS_PROGRAM;
  NS_ASSERTION(aLocation >= 0, "Invalid location");

  mGL->fUniformMatrix4fv(aLocation, 1, false, aFloatValues);
}

void
ShaderProgramOGL::SetUniform(GLint aLocation, const gfx::Color& aColor) {
  ASSERT_THIS_PROGRAM;
  NS_ASSERTION(aLocation >= 0, "Invalid location");

  mGL->fUniform4f(aLocation, float(aColor.r), float(aColor.g), float(aColor.b), float(aColor.a));
}


} /* layers */
} /* mozilla */
