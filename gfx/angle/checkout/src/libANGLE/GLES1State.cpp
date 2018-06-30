//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// GLES1State.cpp: Implements the GLES1State class, tracking state
// for GLES1 contexts.

#include "libANGLE/GLES1State.h"

#include "libANGLE/Context.h"

namespace gl
{

TextureCoordF::TextureCoordF() = default;

TextureCoordF::TextureCoordF(float _s, float _t, float _r, float _q) : s(_s), t(_t), r(_r), q(_q)
{
}

bool TextureCoordF::operator==(const TextureCoordF &other) const
{
    return s == other.s && t == other.t && r == other.r && q == other.q;
}

GLES1State::GLES1State()
    : mGLState(nullptr),
      mVertexArrayEnabled(false),
      mNormalArrayEnabled(false),
      mColorArrayEnabled(false),
      mPointSizeArrayEnabled(false),
      mLineSmoothEnabled(false),
      mPointSmoothEnabled(false),
      mPointSpriteEnabled(false),
      mAlphaTestEnabled(false),
      mLogicOpEnabled(false),
      mLightingEnabled(false),
      mFogEnabled(false),
      mRescaleNormalEnabled(false),
      mNormalizeEnabled(false),
      mColorMaterialEnabled(false),
      mReflectionMapEnabled(false),
      mCurrentColor({0.0f, 0.0f, 0.0f, 0.0f}),
      mCurrentNormal({0.0f, 0.0f, 0.0f}),
      mClientActiveTexture(0),
      mMatrixMode(MatrixType::Modelview),
      mShadeModel(ShadingModel::Smooth),
      mAlphaTestFunc(AlphaTestFunc::AlwaysPass),
      mAlphaTestRef(0.0f),
      mLogicOp(LogicalOperation::Copy),
      mLineSmoothHint(HintSetting::DontCare),
      mPointSmoothHint(HintSetting::DontCare),
      mPerspectiveCorrectionHint(HintSetting::DontCare),
      mFogHint(HintSetting::DontCare)
{
}

GLES1State::~GLES1State() = default;

// Taken from the GLES 1.x spec which specifies all initial state values.
void GLES1State::initialize(const Context *context, const State *state)
{
    mGLState = state;

    const Caps &caps = context->getCaps();

    mTexUnitEnables.resize(caps.maxMultitextureUnits);
    for (auto &enables : mTexUnitEnables)
    {
        enables.reset();
    }

    mVertexArrayEnabled    = false;
    mNormalArrayEnabled    = false;
    mColorArrayEnabled     = false;
    mPointSizeArrayEnabled = false;
    mTexCoordArrayEnabled.resize(caps.maxMultitextureUnits, false);

    mLineSmoothEnabled    = false;
    mPointSmoothEnabled   = false;
    mPointSpriteEnabled   = false;
    mLogicOpEnabled       = false;
    mAlphaTestEnabled     = false;
    mLightingEnabled      = false;
    mFogEnabled           = false;
    mRescaleNormalEnabled = false;
    mNormalizeEnabled     = false;
    mColorMaterialEnabled = false;
    mReflectionMapEnabled = false;

    mMatrixMode = MatrixType::Modelview;

    mCurrentColor  = {1.0f, 1.0f, 1.0f, 1.0f};
    mCurrentNormal = {0.0f, 0.0f, 1.0f};
    mCurrentTextureCoords.resize(caps.maxMultitextureUnits);
    mClientActiveTexture = 0;

    mTextureEnvironments.resize(caps.maxMultitextureUnits);

    mModelviewMatrices.push_back(angle::Mat4());
    mProjectionMatrices.push_back(angle::Mat4());
    mTextureMatrices.resize(caps.maxMultitextureUnits);
    for (auto &stack : mTextureMatrices)
    {
        stack.push_back(angle::Mat4());
    }

    mMaterial.ambient  = {0.2f, 0.2f, 0.2f, 1.0f};
    mMaterial.diffuse  = {0.8f, 0.8f, 0.8f, 1.0f};
    mMaterial.specular = {0.0f, 0.0f, 0.0f, 1.0f};
    mMaterial.emissive = {0.0f, 0.0f, 0.0f, 1.0f};

    mMaterial.specularExponent = 0.0f;

    mLightModel.color    = {0.2f, 0.2f, 0.2f, 1.0f};
    mLightModel.twoSided = false;

    mLights.resize(caps.maxLights);

    // GL_LIGHT0 is special and has default state that avoids all-black renderings.
    mLights[0].diffuse  = {1.0f, 1.0f, 1.0f, 1.0f};
    mLights[0].specular = {1.0f, 1.0f, 1.0f, 1.0f};

    mFog.mode    = FogMode::Exp;
    mFog.density = 1.0f;
    mFog.start   = 0.0f;
    mFog.end     = 1.0f;

    mFog.color = {0.0f, 0.0f, 0.0f, 0.0f};

    mShadeModel = ShadingModel::Smooth;

    mAlphaTestFunc = AlphaTestFunc::AlwaysPass;
    mAlphaTestRef  = 0.0f;

    mLogicOp = LogicalOperation::Copy;

    mClipPlaneEnabled.resize(caps.maxClipPlanes, false);

    mClipPlanes.resize(caps.maxClipPlanes, angle::Vector4(0.0f, 0.0f, 0.0f, 0.0f));

    mPointParameters.pointSizeMin                = 0.1f;
    mPointParameters.pointSizeMax                = 100.0f;
    mPointParameters.pointFadeThresholdSize      = 0.1f;
    mPointParameters.pointDistanceAttenuation[0] = 1.0f;
    mPointParameters.pointDistanceAttenuation[1] = 0.0f;
    mPointParameters.pointDistanceAttenuation[2] = 0.0f;

    mPointParameters.pointSize = 1.0f;

    mLineSmoothHint            = HintSetting::DontCare;
    mPointSmoothHint           = HintSetting::DontCare;
    mPerspectiveCorrectionHint = HintSetting::DontCare;
    mFogHint                   = HintSetting::DontCare;
}

void GLES1State::setAlphaFunc(AlphaTestFunc func, GLfloat ref)
{
    mAlphaTestFunc = func;
    mAlphaTestRef  = ref;
}

void GLES1State::setClientTextureUnit(unsigned int unit)
{
    mClientActiveTexture = unit;
}

unsigned int GLES1State::getClientTextureUnit() const
{
    return mClientActiveTexture;
}

void GLES1State::setCurrentColor(const ColorF &color)
{
    mCurrentColor = color;
}

const ColorF &GLES1State::getCurrentColor() const
{
    return mCurrentColor;
}

void GLES1State::setCurrentNormal(const angle::Vector3 &normal)
{
    mCurrentNormal = normal;
}

const angle::Vector3 &GLES1State::getCurrentNormal() const
{
    return mCurrentNormal;
}

void GLES1State::setCurrentTextureCoords(unsigned int unit, const TextureCoordF &coords)
{
    mCurrentTextureCoords[unit] = coords;
}

const TextureCoordF &GLES1State::getCurrentTextureCoords(unsigned int unit) const
{
    return mCurrentTextureCoords[unit];
}

void GLES1State::setMatrixMode(MatrixType mode)
{
    mMatrixMode = mode;
}

MatrixType GLES1State::getMatrixMode() const
{
    return mMatrixMode;
}

void GLES1State::pushMatrix()
{
    auto &stack = currentMatrixStack();
    stack.push_back(stack.back());
}

void GLES1State::popMatrix()
{
    auto &stack = currentMatrixStack();
    stack.pop_back();
}

GLES1State::MatrixStack &GLES1State::currentMatrixStack()
{
    switch (mMatrixMode)
    {
        case MatrixType::Modelview:
            return mModelviewMatrices;
        case MatrixType::Projection:
            return mProjectionMatrices;
        case MatrixType::Texture:
            return mTextureMatrices[mGLState->getActiveSampler()];
        default:
            UNREACHABLE();
            return mModelviewMatrices;
    }
}

const GLES1State::MatrixStack &GLES1State::currentMatrixStack() const
{
    switch (mMatrixMode)
    {
        case MatrixType::Modelview:
            return mModelviewMatrices;
        case MatrixType::Projection:
            return mProjectionMatrices;
        case MatrixType::Texture:
            return mTextureMatrices[mGLState->getActiveSampler()];
        default:
            UNREACHABLE();
            return mModelviewMatrices;
    }
}

void GLES1State::loadMatrix(const angle::Mat4 &m)
{
    currentMatrixStack().back() = m;
}

void GLES1State::multMatrix(const angle::Mat4 &m)
{
    angle::Mat4 currentMatrix   = currentMatrixStack().back();
    currentMatrixStack().back() = currentMatrix.product(m);
}

void GLES1State::setClientStateEnabled(ClientVertexArrayType clientState, bool enable)
{
    switch (clientState)
    {
        case ClientVertexArrayType::Vertex:
            mVertexArrayEnabled = enable;
            break;
        case ClientVertexArrayType::Normal:
            mNormalArrayEnabled = enable;
            break;
        case ClientVertexArrayType::Color:
            mColorArrayEnabled = enable;
            break;
        case ClientVertexArrayType::PointSize:
            mPointSizeArrayEnabled = enable;
            break;
        case ClientVertexArrayType::TextureCoord:
            mTexCoordArrayEnabled[mClientActiveTexture] = enable;
            break;
        default:
            UNREACHABLE();
            break;
    }
}

bool GLES1State::isClientStateEnabled(ClientVertexArrayType clientState) const
{
    switch (clientState)
    {
        case ClientVertexArrayType::Vertex:
            return mVertexArrayEnabled;
        case ClientVertexArrayType::Normal:
            return mNormalArrayEnabled;
        case ClientVertexArrayType::Color:
            return mColorArrayEnabled;
        case ClientVertexArrayType::PointSize:
            return mPointSizeArrayEnabled;
        case ClientVertexArrayType::TextureCoord:
            return mTexCoordArrayEnabled[mClientActiveTexture];
        default:
            UNREACHABLE();
            return false;
    }
}

bool GLES1State::isTexCoordArrayEnabled(unsigned int unit) const
{
    ASSERT(unit < mTexCoordArrayEnabled.size());
    return mTexCoordArrayEnabled[unit];
}

bool GLES1State::isTextureTargetEnabled(unsigned int unit, const TextureType type) const
{
    return mTexUnitEnables[unit].test(type);
}

}  // namespace gl
