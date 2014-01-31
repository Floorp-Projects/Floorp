/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_OGLSHADERPROGRAM_H
#define GFX_OGLSHADERPROGRAM_H

#include "GLContext.h"                  // for fast inlines of glUniform*
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

class KnownUniform {
public:
  enum KnownUniformName {
    NotAKnownUniform = -1,

    LayerTransform = 0,
    MaskQuadTransform,
    LayerQuadTransform,
    MatrixProj,
    TextureTransform,
    RenderTargetOffset,
    LayerOpacity,
    Texture,
    YTexture,
    CbTexture,
    CrTexture,
    BlackTexture,
    WhiteTexture,
    MaskTexture,
    RenderColor,
    TexCoordMultiplier,

    KnownUniformCount
  };

  KnownUniform()
  {
    mName = NotAKnownUniform;
    mNameString = nullptr;
    mLocation = -1;
    memset(&mValue, 0, sizeof(mValue));
  }

  bool UpdateUniform(int32_t i1) {
    if (mLocation == -1) return false;
    if (mValue.i1 != i1) {
      mValue.i1 = i1;
      return true;
    }
    return false;
  }

  bool UpdateUniform(float f1) {
    if (mLocation == -1) return false;
    if (mValue.f1 != f1) {
      mValue.f1 = f1;
      return true;
    }
    return false;
  }

  bool UpdateUniform(float f1, float f2) {
    if (mLocation == -1) return false;
    if (mValue.f16v[0] != f1 ||
        mValue.f16v[1] != f2)
    {
      mValue.f16v[0] = f1;
      mValue.f16v[1] = f2;
      return true;
    }
    return false;
  }

  bool UpdateUniform(float f1, float f2, float f3, float f4) {
    if (mLocation == -1) return false;
    if (mValue.f16v[0] != f1 ||
        mValue.f16v[1] != f2 ||
        mValue.f16v[2] != f3 ||
        mValue.f16v[3] != f4)
    {
      mValue.f16v[0] = f1;
      mValue.f16v[1] = f2;
      mValue.f16v[2] = f3;
      mValue.f16v[3] = f4;
      return true;
    }
    return false;
  }

  bool UpdateUniform(int cnt, const float *fp) {
    if (mLocation == -1) return false;
    switch (cnt) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 16:
      if (memcmp(mValue.f16v, fp, sizeof(float) * cnt) != 0) {
        memcpy(mValue.f16v, fp, sizeof(float) * cnt);
        return true;
      }
      return false;
    }

    NS_NOTREACHED("cnt must be 1 2 3 4 or 16");
    return false;
  }

  KnownUniformName mName;
  const char *mNameString;
  int32_t mLocation;

  union {
    int i1;
    float f1;
    float f16v[16];
  } mValue;
};

static inline ShaderProgramType
ShaderProgramFromSurfaceFormat(gfx::SurfaceFormat aFormat)
{
  switch (aFormat) {
    case gfx::SurfaceFormat::B8G8R8A8:
      return BGRALayerProgramType;
    case gfx::SurfaceFormat::B8G8R8X8:
      return BGRXLayerProgramType;
    case gfx::SurfaceFormat::R8G8B8A8:
      return RGBALayerProgramType;
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::R5G6B5:
      return RGBXLayerProgramType;
    case gfx::SurfaceFormat::A8:
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
      MOZ_ASSERT(aFormat == gfx::SurfaceFormat::R8G8B8A8);
      return RGBAExternalLayerProgramType;
    case LOCAL_GL_TEXTURE_RECTANGLE_ARB:
      MOZ_ASSERT(aFormat == gfx::SurfaceFormat::R8G8B8A8 ||
                 aFormat == gfx::SurfaceFormat::R8G8B8X8);
      if (aFormat == gfx::SurfaceFormat::R8G8B8A8)
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
  if (aContentType == gfxContentType::COLOR_ALPHA)
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

  KnownUniform mUniforms[KnownUniform::KnownUniformCount];
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
    return mProfile.mUniforms[KnownUniform::TexCoordMultiplier].mLocation;
  }

  /**
   * The following set of methods set a uniform argument to the shader program.
   * Not all uniforms may be set for all programs, and such uses will throw
   * an assertion.
   */
  void SetLayerTransform(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(KnownUniform::LayerTransform, aMatrix);
  }

  void SetMaskLayerTransform(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(KnownUniform::MaskQuadTransform, aMatrix);
  }

  void SetLayerQuadRect(const nsIntRect& aRect) {
    gfx3DMatrix m;
    m._11 = float(aRect.width);
    m._22 = float(aRect.height);
    m._41 = float(aRect.x);
    m._42 = float(aRect.y);
    SetMatrixUniform(KnownUniform::LayerQuadTransform, m);
  }

  void SetLayerQuadRect(const gfx::Rect& aRect) {
    gfx3DMatrix m;
    m._11 = aRect.width;
    m._22 = aRect.height;
    m._41 = aRect.x;
    m._42 = aRect.y;
    SetMatrixUniform(KnownUniform::LayerQuadTransform, m);
  }

  // Set a projection matrix on the program to be set the next time
  // the program is activated.
  void DelayedSetProjectionMatrix(const gfx::Matrix4x4& aMatrix)
  {
    mIsProjectionMatrixStale = true;
    mProjectionMatrix = aMatrix;
  }

  void SetProjectionMatrix(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(KnownUniform::MatrixProj, aMatrix);
    mIsProjectionMatrixStale = false;
  }

  // sets this program's texture transform, if it uses one
  void SetTextureTransform(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(KnownUniform::TextureTransform, aMatrix);
  }

  void SetRenderOffset(const nsIntPoint& aOffset) {
    float vals[4] = { float(aOffset.x), float(aOffset.y), 0.0f, 0.0f };
    SetUniform(KnownUniform::RenderTargetOffset, 4, vals);
  }

  void SetRenderOffset(float aX, float aY) {
    float vals[4] = { aX, aY, 0.0f, 0.0f };
    SetUniform(KnownUniform::RenderTargetOffset, 4, vals);
  }

  void SetLayerOpacity(float aOpacity) {
    SetUniform(KnownUniform::LayerOpacity, aOpacity);
  }

  void SetTextureUnit(GLint aUnit) {
    SetUniform(KnownUniform::Texture, aUnit);
  }
  void SetYTextureUnit(GLint aUnit) {
    SetUniform(KnownUniform::YTexture, aUnit);
  }

  void SetCbTextureUnit(GLint aUnit) {
    SetUniform(KnownUniform::CbTexture, aUnit);
  }

  void SetCrTextureUnit(GLint aUnit) {
    SetUniform(KnownUniform::CrTexture, aUnit);
  }

  void SetYCbCrTextureUnits(GLint aYUnit, GLint aCbUnit, GLint aCrUnit) {
    SetUniform(KnownUniform::YTexture, aYUnit);
    SetUniform(KnownUniform::CbTexture, aCbUnit);
    SetUniform(KnownUniform::CrTexture, aCrUnit);
  }

  void SetBlackTextureUnit(GLint aUnit) {
    SetUniform(KnownUniform::BlackTexture, aUnit);
  }

  void SetWhiteTextureUnit(GLint aUnit) {
    SetUniform(KnownUniform::WhiteTexture, aUnit);
  }

  void SetMaskTextureUnit(GLint aUnit) {
    SetUniform(KnownUniform::MaskTexture, aUnit);
  }

  void SetRenderColor(const gfxRGBA& aColor) {
    SetUniform(KnownUniform::RenderColor, aColor);
  }

  void SetRenderColor(const gfx::Color& aColor) {
    SetUniform(KnownUniform::RenderColor, aColor);
  }

  void SetTexCoordMultiplier(float aWidth, float aHeight) {
    float f[] = {aWidth, aHeight};
    SetUniform(KnownUniform::TexCoordMultiplier, 2, f);
  }

  // the names of attributes
  static const char* const VertexCoordAttrib;
  static const char* const TexCoordAttrib;

protected:
  gfx::Matrix4x4 mProjectionMatrix;
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

  void SetUniform(KnownUniform::KnownUniformName aKnownUniform, float aFloatValue)
  {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount, "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateUniform(aFloatValue)) {
      mGL->fUniform1f(ku.mLocation, aFloatValue);
    }
  }

  void SetUniform(KnownUniform::KnownUniformName aKnownUniform, const gfxRGBA& aColor)
  {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount, "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateUniform(aColor.r, aColor.g, aColor.b, aColor.a)) {
      mGL->fUniform4fv(ku.mLocation, 1, ku.mValue.f16v);
    }
  }

  void SetUniform(KnownUniform::KnownUniformName aKnownUniform, const gfx::Color& aColor) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount, "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateUniform(aColor.r, aColor.g, aColor.b, aColor.a)) {
      mGL->fUniform4fv(ku.mLocation, 1, ku.mValue.f16v);
    }
  }

  void SetUniform(KnownUniform::KnownUniformName aKnownUniform, int aLength, float *aFloatValues)
  {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount, "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateUniform(aLength, aFloatValues)) {
      switch (aLength) {
      case 1: mGL->fUniform1fv(ku.mLocation, 1, ku.mValue.f16v); break;
      case 2: mGL->fUniform2fv(ku.mLocation, 1, ku.mValue.f16v); break;
      case 3: mGL->fUniform3fv(ku.mLocation, 1, ku.mValue.f16v); break;
      case 4: mGL->fUniform4fv(ku.mLocation, 1, ku.mValue.f16v); break;
      default:
        NS_NOTREACHED("Bogus aLength param");
      }
    }
  }

  void SetUniform(KnownUniform::KnownUniformName aKnownUniform, GLint aIntValue) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount, "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateUniform(aIntValue)) {
      mGL->fUniform1i(ku.mLocation, aIntValue);
    }
  }

  void SetMatrixUniform(KnownUniform::KnownUniformName aKnownUniform, const float *aFloatValues) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount, "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateUniform(16, aFloatValues)) {
      mGL->fUniformMatrix4fv(ku.mLocation, 1, false, ku.mValue.f16v);
    }
  }

  void SetMatrixUniform(KnownUniform::KnownUniformName aKnownUniform, const gfx3DMatrix& aMatrix) {
    SetMatrixUniform(aKnownUniform, &aMatrix._11);
  }

  void SetMatrixUniform(KnownUniform::KnownUniformName aKnownUniform, const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(aKnownUniform, &aMatrix._11);
  }
};


} /* layers */
} /* mozilla */

#endif /* GFX_OGLSHADERPROGRAM_H */
