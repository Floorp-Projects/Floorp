/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_OGLSHADERPROGRAM_H
#define GFX_OGLSHADERPROGRAM_H

#include "GLContext.h"  // for fast inlines of glUniform*
#include "OGLShaderConfig.h"

#include <string>
#include <utility>

namespace mozilla {
namespace layers {

#if defined(DEBUG)
#  define CHECK_CURRENT_PROGRAM 1
#  define ASSERT_THIS_PROGRAM                                       \
    do {                                                            \
      GLuint currentProgram;                                        \
      mGL->GetUIntegerv(LOCAL_GL_CURRENT_PROGRAM, &currentProgram); \
      MOZ_ASSERT(currentProgram == mProgram,                        \
                 "SetUniform with wrong program active!");          \
    } while (0)
#else
#  define ASSERT_THIS_PROGRAM \
    do {                      \
    } while (0)
#endif

/**
 * This struct represents the shaders that make up a program and the uniform
 * and attribute parmeters that those shaders take.
 * It is used by ShaderProgramOGL.
 * Use the factory method GetProfileFor to create instances.
 */
struct ProgramProfileOGL {
  /**
   * Factory method; creates an instance of this class for the given
   * ShaderConfigOGL
   */
  static ProgramProfileOGL GetProfileFor(ShaderConfigOGL aConfig);

  // the source code for the program's shaders
  std::string mVertexShaderString;
  std::string mFragmentShaderString;

  // the vertex attributes
  CopyableTArray<std::pair<nsCString, GLuint>> mAttributes;

  KnownUniform mUniforms[KnownUniform::KnownUniformCount];
  CopyableTArray<const char*> mDefines;
  size_t mTextureCount;

  ProgramProfileOGL() : mTextureCount(0) {}

 private:
  static void BuildMixBlender(const ShaderConfigOGL& aConfig,
                              std::ostringstream& fs);
};

/**
 * Represents an OGL shader program. The details of a program are represented
 * by a ProgramProfileOGL
 */
class ShaderProgramOGL {
 public:
  typedef mozilla::gl::GLContext GLContext;

  ShaderProgramOGL(GLContext* aGL, const ProgramProfileOGL& aProfile);

  ~ShaderProgramOGL();

  bool HasInitialized() {
    NS_ASSERTION(mProgramState != STATE_OK || mProgram > 0,
                 "Inconsistent program state");
    return mProgramState == STATE_OK;
  }

  GLuint GetProgram();

  bool Initialize();

  GLint CreateShader(GLenum aShaderType, const char* aShaderSource);

  /**
   * Creates a program and stores its id.
   */
  bool CreateProgram(const char* aVertexShaderString,
                     const char* aFragmentShaderString);

  /**
   * The following set of methods set a uniform argument to the shader program.
   * Not all uniforms may be set for all programs, and such uses will throw
   * an assertion.
   */
  void SetLayerTransform(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(KnownUniform::LayerTransform, aMatrix);
  }

  void SetLayerTransformInverse(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(KnownUniform::LayerTransformInverse, aMatrix);
  }

  void SetMaskLayerTransform(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(KnownUniform::MaskTransform, aMatrix);
  }

  void SetBackdropTransform(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(KnownUniform::BackdropTransform, aMatrix);
  }

  void SetDEAAEdges(const gfx::Point3D* aEdges) {
    SetArrayUniform(KnownUniform::SSEdges, 4, aEdges);
  }

  void SetViewportSize(const gfx::IntSize& aSize) {
    float vals[2] = {(float)aSize.width, (float)aSize.height};
    SetUniform(KnownUniform::ViewportSize, 2, vals);
  }

  void SetVisibleCenter(const gfx::Point& aVisibleCenter) {
    float vals[2] = {aVisibleCenter.x, aVisibleCenter.y};
    SetUniform(KnownUniform::VisibleCenter, 2, vals);
  }

  void SetLayerRects(const gfx::Rect* aRects) {
    float vals[16] = {
        aRects[0].X(), aRects[0].Y(), aRects[0].Width(), aRects[0].Height(),
        aRects[1].X(), aRects[1].Y(), aRects[1].Width(), aRects[1].Height(),
        aRects[2].X(), aRects[2].Y(), aRects[2].Width(), aRects[2].Height(),
        aRects[3].X(), aRects[3].Y(), aRects[3].Width(), aRects[3].Height()};
    SetUniform(KnownUniform::LayerRects, 16, vals);
  }

  void SetProjectionMatrix(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(KnownUniform::MatrixProj, aMatrix);
  }

  // sets this program's texture transform, if it uses one
  void SetTextureTransform(const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(KnownUniform::TextureTransform, aMatrix);
  }

  void SetTextureRects(const gfx::Rect* aRects) {
    float vals[16] = {
        aRects[0].X(), aRects[0].Y(), aRects[0].Width(), aRects[0].Height(),
        aRects[1].X(), aRects[1].Y(), aRects[1].Width(), aRects[1].Height(),
        aRects[2].X(), aRects[2].Y(), aRects[2].Width(), aRects[2].Height(),
        aRects[3].X(), aRects[3].Y(), aRects[3].Width(), aRects[3].Height()};
    SetUniform(KnownUniform::TextureRects, 16, vals);
  }

  void SetRenderOffset(const nsIntPoint& aOffset) {
    float vals[4] = {float(aOffset.x), float(aOffset.y)};
    SetUniform(KnownUniform::RenderTargetOffset, 2, vals);
  }

  void SetRenderOffset(float aX, float aY) {
    float vals[2] = {aX, aY};
    SetUniform(KnownUniform::RenderTargetOffset, 2, vals);
  }

  void SetLayerOpacity(float aOpacity) {
    SetUniform(KnownUniform::LayerOpacity, aOpacity);
  }

  void SetTextureUnit(GLint aUnit) { SetUniform(KnownUniform::Texture, aUnit); }
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

  void SetNV12TextureUnits(GLint aYUnit, GLint aCbCrUnit) {
    SetUniform(KnownUniform::YTexture, aYUnit);
    SetUniform(KnownUniform::CbTexture, aCbCrUnit);
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

  void SetBackdropTextureUnit(GLint aUnit) {
    SetUniform(KnownUniform::BackdropTexture, aUnit);
  }

  void SetRenderColor(const gfx::DeviceColor& aColor) {
    SetUniform(KnownUniform::RenderColor, aColor);
  }

  void SetColorMatrix(const gfx::Matrix5x4& aColorMatrix) {
    SetMatrixUniform(KnownUniform::ColorMatrix, &aColorMatrix._11);
    SetUniform(KnownUniform::ColorMatrixVector, 4, &aColorMatrix._51);
  }

  void SetTexCoordMultiplier(float aWidth, float aHeight) {
    float f[] = {aWidth, aHeight};
    SetUniform(KnownUniform::TexCoordMultiplier, 2, f);
  }

  void SetCbCrTexCoordMultiplier(float aWidth, float aHeight) {
    float f[] = {aWidth, aHeight};
    SetUniform(KnownUniform::CbCrTexCoordMultiplier, 2, f);
  }

  void SetMaskCoordMultiplier(float aWidth, float aHeight) {
    float f[] = {aWidth, aHeight};
    SetUniform(KnownUniform::MaskCoordMultiplier, 2, f);
  }

  void SetYUVColorSpace(gfx::YUVColorSpace aYUVColorSpace);

  // Set whether we want the component alpha shader to return the color
  // vector (pass 1, false) or the alpha vector (pass2, true). With support
  // for multiple render targets we wouldn't need two passes here.
  void SetTexturePass2(bool aFlag) {
    SetUniform(KnownUniform::TexturePass2, aFlag ? 1 : 0);
  }

  void SetBlurRadius(float aRX, float aRY);

  void SetBlurAlpha(float aAlpha) {
    SetUniform(KnownUniform::BlurAlpha, aAlpha);
  }

  void SetBlurOffset(float aOffsetX, float aOffsetY) {
    float f[] = {aOffsetX, aOffsetY};
    SetUniform(KnownUniform::BlurOffset, 2, f);
  }

  size_t GetTextureCount() const { return mProfile.mTextureCount; }

 protected:
  RefPtr<GLContext> mGL;
  // the OpenGL id of the program
  GLuint mProgram;
  ProgramProfileOGL mProfile;
  enum { STATE_NEW, STATE_OK, STATE_ERROR } mProgramState;

#ifdef CHECK_CURRENT_PROGRAM
  static int sCurrentProgramKey;
#endif

  void SetUniform(KnownUniform::KnownUniformName aKnownUniform,
                  float aFloatValue) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(
        aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount,
        "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateUniform(aFloatValue)) {
      mGL->fUniform1f(ku.mLocation, aFloatValue);
    }
  }

  void SetUniform(KnownUniform::KnownUniformName aKnownUniform,
                  const gfx::DeviceColor& aColor) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(
        aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount,
        "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateUniform(aColor.r, aColor.g, aColor.b, aColor.a)) {
      mGL->fUniform4fv(ku.mLocation, 1, ku.mValue.f16v);
    }
  }

  void SetUniform(KnownUniform::KnownUniformName aKnownUniform, int aLength,
                  const float* aFloatValues) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(
        aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount,
        "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateUniform(aLength, aFloatValues)) {
      switch (aLength) {
        case 1:
          mGL->fUniform1fv(ku.mLocation, 1, ku.mValue.f16v);
          break;
        case 2:
          mGL->fUniform2fv(ku.mLocation, 1, ku.mValue.f16v);
          break;
        case 3:
          mGL->fUniform3fv(ku.mLocation, 1, ku.mValue.f16v);
          break;
        case 4:
          mGL->fUniform4fv(ku.mLocation, 1, ku.mValue.f16v);
          break;
        case 16:
          mGL->fUniform4fv(ku.mLocation, 4, ku.mValue.f16v);
          break;
        default:
          MOZ_ASSERT_UNREACHABLE("Bogus aLength param");
      }
    }
  }

  void SetArrayUniform(KnownUniform::KnownUniformName aKnownUniform,
                       int aLength, float* aFloatValues) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(
        aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount,
        "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateArrayUniform(aLength, aFloatValues)) {
      mGL->fUniform1fv(ku.mLocation, aLength, ku.mValue.f16v);
    }
  }

  void SetArrayUniform(KnownUniform::KnownUniformName aKnownUniform,
                       int aLength, const gfx::Point3D* aPointValues) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(
        aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount,
        "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateArrayUniform(aLength, aPointValues)) {
      mGL->fUniform3fv(ku.mLocation, aLength, ku.mValue.f16v);
    }
  }

  void SetUniform(KnownUniform::KnownUniformName aKnownUniform,
                  GLint aIntValue) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(
        aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount,
        "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateUniform(aIntValue)) {
      mGL->fUniform1i(ku.mLocation, aIntValue);
    }
  }

  void SetMatrixUniform(KnownUniform::KnownUniformName aKnownUniform,
                        const float* aFloatValues) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(
        aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount,
        "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateUniform(16, aFloatValues)) {
      mGL->fUniformMatrix4fv(ku.mLocation, 1, false, ku.mValue.f16v);
    }
  }

  void SetMatrix3fvUniform(KnownUniform::KnownUniformName aKnownUniform,
                           const float* aFloatValues) {
    ASSERT_THIS_PROGRAM;
    NS_ASSERTION(
        aKnownUniform >= 0 && aKnownUniform < KnownUniform::KnownUniformCount,
        "Invalid known uniform");

    KnownUniform& ku(mProfile.mUniforms[aKnownUniform]);
    if (ku.UpdateUniform(9, aFloatValues)) {
      mGL->fUniformMatrix3fv(ku.mLocation, 1, false, ku.mValue.f16v);
    }
  }

  void SetMatrixUniform(KnownUniform::KnownUniformName aKnownUniform,
                        const gfx::Matrix4x4& aMatrix) {
    SetMatrixUniform(aKnownUniform, &aMatrix._11);
  }
};

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_OGLSHADERPROGRAM_H
