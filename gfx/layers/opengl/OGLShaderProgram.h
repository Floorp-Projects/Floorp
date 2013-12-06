/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_OGLSHADERPROGRAM_H
#define GFX_OGLSHADERPROGRAM_H

#include "GLDefs.h"                     // for GLint, GLenum, GLuint, etc
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxTypes.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsPoint.h"                    // for nsIntPoint
#include "nsTArray.h"                   // for nsTArray
#include "mozilla/layers/CompositorTypes.h"

struct gfxRGBA;
struct nsIntRect;

namespace mozilla {
namespace gl {
class GLContext;
}
namespace layers {

class Layer;

enum ShaderProgramType {
  RGBALayerProgramType,
  BGRALayerProgramType,
  RGBXLayerProgramType,
  BGRXLayerProgramType,
  RGBARectLayerProgramType,
  RGBXRectLayerProgramType,
  BGRARectLayerProgramType,
  RGBAExternalLayerProgramType,
  ColorLayerProgramType,
  YCbCrLayerProgramType,
  ComponentAlphaPass1ProgramType,
  ComponentAlphaPass1RGBProgramType,
  ComponentAlphaPass2ProgramType,
  ComponentAlphaPass2RGBProgramType,
  Copy2DProgramType,
  Copy2DRectProgramType,
  NumProgramTypes
};

static inline ShaderProgramType
ShaderProgramFromSurfaceFormat(gfx::SurfaceFormat aFormat)
{
  switch (aFormat) {
    case gfx::FORMAT_B8G8R8A8:
      return BGRALayerProgramType;
    case gfx::FORMAT_B8G8R8X8:
      return BGRXLayerProgramType;
    case gfx::FORMAT_R8G8B8A8:
      return RGBALayerProgramType;
    case gfx::FORMAT_R8G8B8X8:
    case gfx::FORMAT_R5G6B5:
      return RGBXLayerProgramType;
    case gfx::FORMAT_A8:
      // We don't have a specific luminance shader
      break;
    default:
      NS_ASSERTION(false, "Unhandled surface format!");
  }
  return ShaderProgramType(0);
}

static inline ShaderProgramType
ShaderProgramFromTargetAndFormat(GLenum aTarget,
                                 gfx::SurfaceFormat aFormat)
{
  switch(aTarget) {
    case LOCAL_GL_TEXTURE_EXTERNAL:
      MOZ_ASSERT(aFormat == gfx::FORMAT_R8G8B8A8);
      return RGBAExternalLayerProgramType;
    case LOCAL_GL_TEXTURE_RECTANGLE_ARB:
      MOZ_ASSERT(aFormat == gfx::FORMAT_R8G8B8A8 ||
                 aFormat == gfx::FORMAT_R8G8B8X8);
      if (aFormat == gfx::FORMAT_R8G8B8A8)
        return RGBARectLayerProgramType;
      else
        return RGBXRectLayerProgramType;
    default:
      return ShaderProgramFromSurfaceFormat(aFormat);
  }
}

static inline ShaderProgramType
ShaderProgramFromContentType(gfxContentType aContentType)
{
  if (aContentType == GFX_CONTENT_COLOR_ALPHA)
    return RGBALayerProgramType;
  return RGBXLayerProgramType;
}

/**
 * This struct represents the shaders that make up a program and the uniform
 * and attribute parmeters that those shaders take.
 * It is used by ShaderProgramOGL.
 * Use the factory method GetProfileFor to create instances.
 */
struct ProgramProfileOGL
{
  /**
   * Factory method; creates an instance of this class for the given
   * ShaderProgramType
   */
  static ProgramProfileOGL GetProfileFor(ShaderProgramType aType,
                                         MaskType aMask);

  /**
   * returns true if such a shader program exists
   */
  static bool ProgramExists(ShaderProgramType aType, MaskType aMask)
  {
    if (aType < 0 ||
        aType >= NumProgramTypes)
      return false;

    if (aMask < MaskNone ||
        aMask >= NumMaskTypes)
      return false;

    if (aMask == Mask2d &&
        (aType == Copy2DProgramType ||
         aType == Copy2DRectProgramType))
      return false;

    if (aMask != MaskNone &&
        aType == BGRARectLayerProgramType)
      return false;

    return aMask != Mask3d ||
           aType == RGBARectLayerProgramType ||
           aType == RGBXRectLayerProgramType ||
           aType == RGBALayerProgramType;
  }


  /**
   * These two methods lookup the location of a uniform and attribute,
   * respectively. Returns -1 if the named uniform/attribute does not
   * have a location for the shaders represented by this profile.
   */
  GLint LookupUniformLocation(const char* aName)
  {
    for (uint32_t i = 0; i < mUniforms.Length(); ++i) {
      if (strcmp(mUniforms[i].mName, aName) == 0) {
        return mUniforms[i].mLocation;
      }
    }

    return -1;
  }

  GLint LookupAttributeLocation(const char* aName)
  {
    for (uint32_t i = 0; i < mAttributes.Length(); ++i) {
      if (strcmp(mAttributes[i].mName, aName) == 0) {
        return mAttributes[i].mLocation;
      }
    }

    return -1;
  }

  // represents the name and location of a uniform or attribute
  struct Argument
  {
    Argument(const char* aName) :
      mName(aName) {}
    const char* mName;
    GLint mLocation;
  };

  // the source code for the program's shaders
  const char *mVertexShaderString;
  const char *mFragmentShaderString;

  nsTArray<Argument> mUniforms;
  nsTArray<Argument> mAttributes;
  uint32_t mTextureCount;
  bool mHasMatrixProj;
private:
  ProgramProfileOGL() :
    mTextureCount(0),
    mHasMatrixProj(false) {}
};


#if defined(DEBUG)
#define CHECK_CURRENT_PROGRAM 1
#define ASSERT_THIS_PROGRAM                                             \
  do {                                                                  \
    GLuint currentProgram;                                              \
    mGL->GetUIntegerv(LOCAL_GL_CURRENT_PROGRAM, &currentProgram);       \
    NS_ASSERTION(currentProgram == mProgram,                            \
                 "SetUniform with wrong program active!");              \
  } while (0)
#else
#define ASSERT_THIS_PROGRAM                                             \
  do { } while (0)
#endif

/**
 * Represents an OGL shader program. The details of a program are represented
 * by a ProgramProfileOGL
 */
class ShaderProgramOGL
{
public:
  typedef mozilla::gl::GLContext GLContext;

  ShaderProgramOGL(GLContext* aGL, const ProgramProfileOGL& aProfile);

  ~ShaderProgramOGL();

  bool HasInitialized() {
    NS_ASSERTION(mProgramState != STATE_OK || mProgram > 0, "Inconsistent program state");
    return mProgramState == STATE_OK;
  }

  void Activate();

  bool Initialize();

  GLint CreateShader(GLenum aShaderType, const char *aShaderSource);

  /**
   * Creates a program and stores its id.
   */
  bool CreateProgram(const char *aVertexShaderString,
                     const char *aFragmentShaderString);

  /**
   * Lookup the location of an attribute
   */
  GLint AttribLocation(const char* aName) {
    return mProfile.LookupAttributeLocation(aName);
  }

  GLint GetTexCoordMultiplierUniformLocation() {
    return mTexCoordMultiplierUniformLocation;
  }

  /**
   * The following set of methods set a uniform argument to the shader program.
   * Not all uniforms may be set for all programs, and such uses will throw
   * an assertion.
   */
  void SetLayerTransform(const gfx3DMatrix& aMatrix) {
    SetMatrixUniform(mProfile.LookupUniformLocation("uLayerTransform"), aMatrix);
  }

  void SetLayerTransform(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(mProfile.LookupUniformLocation("uLayerTransform"), aMatrix);
  }

  void SetMaskLayerTransform(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(mProfile.LookupUniformLocation("uMaskQuadTransform"), aMatrix);
  }

  void SetLayerQuadRect(const nsIntRect& aRect) {
    gfx3DMatrix m;
    m._11 = float(aRect.width);
    m._22 = float(aRect.height);
    m._41 = float(aRect.x);
    m._42 = float(aRect.y);
    SetMatrixUniform(mProfile.LookupUniformLocation("uLayerQuadTransform"), m);
  }

  void SetLayerQuadRect(const gfx::Rect& aRect) {
    gfx3DMatrix m;
    m._11 = aRect.width;
    m._22 = aRect.height;
    m._41 = aRect.x;
    m._42 = aRect.y;
    SetMatrixUniform(mProfile.LookupUniformLocation("uLayerQuadTransform"), m);
  }

  // activates this program and sets its projection matrix, if the program uses one
  void CheckAndSetProjectionMatrix(const gfx3DMatrix& aMatrix)
  {
    if (mProfile.mHasMatrixProj) {
      mIsProjectionMatrixStale = true;
      mProjectionMatrix = aMatrix;
    }
  }

  void SetProjectionMatrix(const gfx3DMatrix& aMatrix) {
    SetMatrixUniform(mProfile.LookupUniformLocation("uMatrixProj"), aMatrix);
    mIsProjectionMatrixStale = false;
  }

  // sets this program's texture transform, if it uses one
  void SetTextureTransform(const gfx3DMatrix& aMatrix) {
    SetMatrixUniform(mProfile.LookupUniformLocation("uTextureTransform"), aMatrix);
  }

  void SetTextureTransform(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(mProfile.LookupUniformLocation("uTextureTransform"), aMatrix);
  }

  void SetRenderOffset(const nsIntPoint& aOffset) {
    float vals[4] = { float(aOffset.x), float(aOffset.y), 0.0f, 0.0f };
    SetUniform(mProfile.LookupUniformLocation("uRenderTargetOffset"), 4, vals);
  }

  void SetRenderOffset(float aX, float aY) {
    float vals[4] = { aX, aY, 0.0f, 0.0f };
    SetUniform(mProfile.LookupUniformLocation("uRenderTargetOffset"), 4, vals);
  }

  void SetLayerOpacity(float aOpacity) {
    SetUniform(mProfile.LookupUniformLocation("uLayerOpacity"), aOpacity);
  }

  void SetTextureUnit(GLint aUnit) {
    SetUniform(mProfile.LookupUniformLocation("uTexture"), aUnit);
  }
  void SetYTextureUnit(GLint aUnit) {
    SetUniform(mProfile.LookupUniformLocation("uYTexture"), aUnit);
  }

  void SetCbTextureUnit(GLint aUnit) {
    SetUniform(mProfile.LookupUniformLocation("uCbTexture"), aUnit);
  }

  void SetCrTextureUnit(GLint aUnit) {
    SetUniform(mProfile.LookupUniformLocation("uCrTexture"), aUnit);
  }

  void SetYCbCrTextureUnits(GLint aYUnit, GLint aCbUnit, GLint aCrUnit) {
    SetUniform(mProfile.LookupUniformLocation("uYTexture"), aYUnit);
    SetUniform(mProfile.LookupUniformLocation("uCbTexture"), aCbUnit);
    SetUniform(mProfile.LookupUniformLocation("uCrTexture"), aCrUnit);
  }

  void SetBlackTextureUnit(GLint aUnit) {
    SetUniform(mProfile.LookupUniformLocation("uBlackTexture"), aUnit);
  }

  void SetWhiteTextureUnit(GLint aUnit) {
    SetUniform(mProfile.LookupUniformLocation("uWhiteTexture"), aUnit);
  }

  void SetMaskTextureUnit(GLint aUnit) {
    SetUniform(mProfile.LookupUniformLocation("uMaskTexture"), aUnit);
  }

  void SetRenderColor(const gfxRGBA& aColor) {
    SetUniform(mProfile.LookupUniformLocation("uRenderColor"), aColor);
  }

  void SetRenderColor(const gfx::Color& aColor) {
    SetUniform(mProfile.LookupUniformLocation("uRenderColor"), aColor);
  }

  void SetTexCoordMultiplier(float aWidth, float aHeight) {
    float f[] = {aWidth, aHeight};
    SetUniform(mTexCoordMultiplierUniformLocation, 2, f);
  }

  // the names of attributes
  static const char* const VertexCoordAttrib;
  static const char* const TexCoordAttrib;

protected:
  gfx3DMatrix mProjectionMatrix;
  // true if the projection matrix needs setting
  bool mIsProjectionMatrixStale;

  RefPtr<GLContext> mGL;
  // the OpenGL id of the program
  GLuint mProgram;
  ProgramProfileOGL mProfile;
  enum {
    STATE_NEW,
    STATE_OK,
    STATE_ERROR
  } mProgramState;

  GLint mTexCoordMultiplierUniformLocation;

  void SetUniform(GLint aLocation, float aFloatValue);
  void SetUniform(GLint aLocation, const gfxRGBA& aColor);
  void SetUniform(GLint aLocation, int aLength, float *aFloatValues);
  void SetUniform(GLint aLocation, GLint aIntValue);
  void SetMatrixUniform(GLint aLocation, const gfx3DMatrix& aMatrix);
  void SetMatrixUniform(GLint aLocation, const float *aFloatValues);

  void SetUniform(GLint aLocation, const gfx::Color& aColor);
  void SetMatrixUniform(GLint aLocation, const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(aLocation, &aMatrix._11);
  }
};


} /* layers */
} /* mozilla */

#endif /* GFX_OGLSHADERPROGRAM_H */
