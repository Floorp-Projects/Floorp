//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// GLES1State.h: Defines the GLES1State class holding the state of
// a GLES1 context.

#ifndef LIBANGLE_GLES1STATE_H_
#define LIBANGLE_GLES1STATE_H_

#include <unordered_set>

#include "common/FixedVector.h"
#include "common/angleutils.h"
#include "common/matrix_utils.h"
#include "common/vector_utils.h"
#include "libANGLE/Caps.h"
#include "libANGLE/angletypes.h"

namespace gl
{

// State types specific to GLES1 contexts, from the OpenGL ES 1.1 spec "State Tables" section
struct TextureCoordF
{
    TextureCoordF();
    TextureCoordF(float _s, float _t, float _r, float _q);
    bool operator==(const TextureCoordF &other) const;

    GLfloat s = 0.0f;
    GLfloat t = 0.0f;
    GLfloat r = 0.0f;
    GLfloat q = 0.0f;
};

struct MaterialParameters
{
    ColorF ambient;
    ColorF diffuse;
    ColorF specular;
    ColorF emissive;
    GLfloat specularExponent;
};

struct LightModelParameters
{
    ColorF color;
    bool twoSided;
};

struct LightParameters
{
    bool enabled                 = false;
    ColorF ambient               = {0.0f, 0.0f, 0.0f, 1.0f};
    ColorF diffuse               = {0.0f, 0.0f, 0.0f, 1.0f};
    ColorF specular              = {0.0f, 0.0f, 0.0f, 1.0f};
    angle::Vector4 position      = {0.0f, 0.0f, 1.0f, 0.0f};
    angle::Vector3 direction     = {0.0f, 0.0f, -1.0f};
    GLfloat spotlightExponent    = 0.0f;
    GLfloat spotlightCutoffAngle = 180.0f;
    GLfloat attenuationConst     = 1.0f;
    GLfloat attenuationLinear    = 0.0f;
    GLfloat attenuationQuadratic = 0.0f;
};

struct FogParameters
{
    FogMode mode;
    GLfloat density;
    GLfloat start;
    GLfloat end;
    ColorF color;
};

struct TextureEnvironmentParameters
{
    TextureEnvMode envMode      = TextureEnvMode::Modulate;
    TextureCombine combineRgb   = TextureCombine::Modulate;
    TextureCombine combineAlpha = TextureCombine::Modulate;

    TextureSrc src0rgb   = TextureSrc::Texture;
    TextureSrc src0alpha = TextureSrc::Texture;

    TextureSrc src1rgb   = TextureSrc::Previous;
    TextureSrc src1alpha = TextureSrc::Previous;

    TextureSrc src2rgb   = TextureSrc::Constant;
    TextureSrc src2alpha = TextureSrc::Constant;

    TextureOp op0rgb   = TextureOp::SrcColor;
    TextureOp op0alpha = TextureOp::SrcAlpha;

    TextureOp op1rgb   = TextureOp::SrcColor;
    TextureOp op1alpha = TextureOp::SrcAlpha;

    TextureOp op2rgb   = TextureOp::SrcAlpha;
    TextureOp op2alpha = TextureOp::SrcAlpha;

    ColorF envColor    = {0.0f, 0.0f, 0.0f, 0.0f};
    GLfloat rgbScale   = 1.0f;
    GLfloat alphaScale = 1.0f;

    bool pointSpriteCoordReplace = false;
};

struct PointParameters
{
    GLfloat pointSizeMin;
    GLfloat pointSizeMax;
    GLfloat pointFadeThresholdSize;
    angle::Vector3 pointDistanceAttenuation;
    GLfloat pointSize;
};

class Context;
class GLES1Renderer;
class State;

class GLES1State final : angle::NonCopyable
{
  public:
    GLES1State();
    ~GLES1State();

    void initialize(const Context *context, const State *state);

    void setAlphaFunc(AlphaTestFunc func, GLfloat ref);
    void setClientTextureUnit(unsigned int unit);
    unsigned int getClientTextureUnit() const;

    void setCurrentColor(const ColorF &color);
    const ColorF &getCurrentColor() const;

    void setCurrentNormal(const angle::Vector3 &normal);
    const angle::Vector3 &getCurrentNormal() const;

    void setCurrentTextureCoords(unsigned int unit, const TextureCoordF &coords);
    const TextureCoordF &getCurrentTextureCoords(unsigned int unit) const;

    void setMatrixMode(MatrixType mode);
    MatrixType getMatrixMode() const;

    void pushMatrix();
    void popMatrix();

    using MatrixStack = angle::FixedVector<angle::Mat4, Caps::GlobalMatrixStackDepth>;
    MatrixStack &currentMatrixStack();
    const MatrixStack &currentMatrixStack() const;

    void loadMatrix(const angle::Mat4 &m);
    void multMatrix(const angle::Mat4 &m);

    void setClientStateEnabled(ClientVertexArrayType clientState, bool enable);
    bool isClientStateEnabled(ClientVertexArrayType clientState) const;
    bool isTexCoordArrayEnabled(unsigned int unit) const;
    bool isTextureTargetEnabled(unsigned int unit, const TextureType type) const;

  private:
    friend class State;
    friend class GLES1Renderer;

    // Back pointer for reading from State.
    const State *mGLState;

    // All initial state values come from the
    // OpenGL ES 1.1 spec.
    std::vector<angle::PackedEnumBitSet<TextureType>> mTexUnitEnables;

    // Table 6.4, 6.5 (IsEnabled)
    bool mVertexArrayEnabled;
    bool mNormalArrayEnabled;
    bool mColorArrayEnabled;
    bool mPointSizeArrayEnabled;
    std::vector<bool> mTexCoordArrayEnabled;

    // Table 6.7-6.16 (IsEnabled)
    std::vector<bool> mClipPlaneEnabled;
    bool mLineSmoothEnabled;
    bool mPointSmoothEnabled;
    bool mPointSpriteEnabled;
    bool mAlphaTestEnabled;
    bool mLogicOpEnabled;
    bool mLightingEnabled;
    bool mFogEnabled;
    bool mRescaleNormalEnabled;
    bool mNormalizeEnabled;
    bool mColorMaterialEnabled;
    bool mReflectionMapEnabled;

    // Table 6.3
    ColorF mCurrentColor;
    angle::Vector3 mCurrentNormal;
    // Invariant: mCurrentTextureCoords size is == GL_MAX_TEXTURE_UNITS.
    std::vector<TextureCoordF> mCurrentTextureCoords;

    // Table 6.4
    unsigned int mClientActiveTexture;

    // Table 6.7
    MatrixType mMatrixMode;
    MatrixStack mProjectionMatrices;
    MatrixStack mModelviewMatrices;
    std::vector<MatrixStack> mTextureMatrices;

    // Table 6.15
    using TextureEnvironments = std::vector<TextureEnvironmentParameters>;
    TextureEnvironments mTextureEnvironments;

    // Table 6.9, 2.8
    MaterialParameters mMaterial;
    LightModelParameters mLightModel;

    // Table 6.10
    std::vector<LightParameters> mLights;

    // Table 6.8
    FogParameters mFog;
    ShadingModel mShadeModel;

    // Table 6.11
    PointParameters mPointParameters;

    // Table 6.16
    AlphaTestFunc mAlphaTestFunc;
    GLfloat mAlphaTestRef;
    LogicalOperation mLogicOp;

    // Table 6.7
    std::vector<angle::Vector4> mClipPlanes;

    // Table 6.19
    HintSetting mLineSmoothHint;
    HintSetting mPointSmoothHint;
    HintSetting mPerspectiveCorrectionHint;
    HintSetting mFogHint;
};

}  // namespace gl

#endif  // LIBANGLE_GLES1STATE_H_
