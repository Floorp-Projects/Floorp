/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERMANAGEROGLPROGRAM_H
#define GFX_LAYERMANAGEROGLPROGRAM_H

#include <string.h>

#include "prenv.h"

#include "nsString.h"
#include "GLContext.h"
#include "gfx3DMatrix.h"

namespace mozilla {
namespace layers {

class Layer;

// The kinds of mask layer a shader can support
// We rely on the items in this enum being sequential
enum MaskType {
  MaskNone = 0,   // no mask layer
  Mask2d,         // mask layer for layers with 2D transforms
  Mask3d,         // mask layer for layers with 3D transforms
  NumMaskTypes
};

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
  static ProgramProfileOGL GetProfileFor(gl::ShaderProgramType aType,
                                         MaskType aMask);

  /**
   * returns true if such a shader program exists
   */
  static bool ProgramExists(gl::ShaderProgramType aType, MaskType aMask)
  {
    if (aType < 0 ||
        aType >= gl::NumProgramTypes)
      return false;

    if (aMask < MaskNone ||
        aMask >= NumMaskTypes)
      return false;

    if (aMask == Mask2d &&
        (aType == gl::Copy2DProgramType ||
         aType == gl::Copy2DRectProgramType))
      return false;

    return aMask != Mask3d ||
           aType == gl::RGBARectLayerProgramType ||
           aType == gl::RGBALayerProgramType;
  }


  /**
   * These two methods lookup the location of a uniform and attribute,
   * respectively. Returns -1 if the named uniform/attribute does not
   * have a location for the shaders represented by this profile.
   */
  GLint LookupUniformLocation(const char* aName)
  {
    for (PRUint32 i = 0; i < mUniforms.Length(); ++i) {
      if (strcmp(mUniforms[i].mName, aName) == 0) {
        return mUniforms[i].mLocation;
      }
    }

    return -1;
  }

  GLint LookupAttributeLocation(const char* aName)
  {
    for (PRUint32 i = 0; i < mAttributes.Length(); ++i) {
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
  PRUint32 mTextureCount;
  bool mHasMatrixProj;
  bool mHasTextureTransform;
private:
  ProgramProfileOGL() :
    mTextureCount(0),
    mHasMatrixProj(false),
    mHasTextureTransform(false) {}
};


#if defined(DEBUG)
#define CHECK_CURRENT_PROGRAM 1
#define ASSERT_THIS_PROGRAM                                             \
  do {                                                                  \
    NS_ASSERTION(mGL->GetUserData(&sCurrentProgramKey) == this, \
                 "SetUniform with wrong program active!");              \
  } while (0)
#else
#define ASSERT_THIS_PROGRAM
#endif

/**
 * Represents an OGL shader program. The details of a program are represented
 * by a ProgramProfileOGL
 */
class ShaderProgramOGL
{
public:
  typedef mozilla::gl::GLContext GLContext;

  ShaderProgramOGL(GLContext* aGL, const ProgramProfileOGL& aProfile) :
    mIsProjectionMatrixStale(false), mGL(aGL), mProgram(0),
    mProfile(aProfile), mProgramState(STATE_NEW) { }

  ~ShaderProgramOGL() {
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

  bool HasInitialized() {
    NS_ASSERTION(mProgramState != STATE_OK || mProgram > 0, "Inconsistent program state");
    return mProgramState == STATE_OK;
  }

  void Activate() {
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
   * aLayer is the mask layer to use for rendering, or null, if there is no
   * mask layer.
   * If aLayer is non-null, then the result of rendering aLayer is stored as
   * as a texture to be used by the shader. It is stored in the next available
   * texture unit, as determined by the texture unit requirements for the
   * shader.
   * Any other features of the mask layer required by the shader are also
   * loaded to graphics memory. In particular the transform used to move from
   * the layer's coordinates to the mask's coordinates is loaded; this must be
   * a 2D transform.
   */
  bool LoadMask(Layer* aLayer);

  /**
   * The following set of methods set a uniform argument to the shader program.
   * Not all uniforms may be set for all programs, and such uses will throw
   * an assertion.
   */
  void SetLayerTransform(const gfx3DMatrix& aMatrix) {
    SetMatrixUniform(mProfile.LookupUniformLocation("uLayerTransform"), aMatrix);
  }

  void SetLayerQuadRect(const nsIntRect& aRect) {
    gfx3DMatrix m;
    m._11 = float(aRect.width);
    m._22 = float(aRect.height);
    m._41 = float(aRect.x);
    m._42 = float(aRect.y);
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
    if (mProfile.mHasTextureTransform)
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

  void SetRenderColor(const gfxRGBA& aColor) {
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

  nsRefPtr<GLContext> mGL;
  // the OpenGL id of the program 
  GLuint mProgram;
  ProgramProfileOGL mProfile;
  enum {
    STATE_NEW,
    STATE_OK,
    STATE_ERROR
  } mProgramState;

  GLint mTexCoordMultiplierUniformLocation;
#ifdef CHECK_CURRENT_PROGRAM
  static int sCurrentProgramKey;
#endif

  void SetUniform(GLint aLocation, float aFloatValue) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(aLocation >= 0, "Invalid location");

    mGL->fUniform1f(aLocation, aFloatValue);
  }

  void SetUniform(GLint aLocation, const gfxRGBA& aColor) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(aLocation >= 0, "Invalid location");

    mGL->fUniform4f(aLocation, float(aColor.r), float(aColor.g), float(aColor.b), float(aColor.a));
  }

  void SetUniform(GLint aLocation, int aLength, float *aFloatValues) {
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

  void SetUniform(GLint aLocation, GLint aIntValue) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(aLocation >= 0, "Invalid location");

    mGL->fUniform1i(aLocation, aIntValue);
  }

  void SetMatrixUniform(GLint aLocation, const gfx3DMatrix& aMatrix) {
    SetMatrixUniform(aLocation, &aMatrix._11);
  }

  void SetMatrixUniform(GLint aLocation, const float *aFloatValues) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(aLocation >= 0, "Invalid location");

    mGL->fUniformMatrix4fv(aLocation, 1, false, aFloatValues);
  }
};


} /* layers */
} /* mozilla */

#endif /* GFX_LAYERMANAGEROGLPROGRAM_H */
