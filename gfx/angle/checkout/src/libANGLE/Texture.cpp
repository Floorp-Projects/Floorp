//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Texture.cpp: Implements the gl::Texture class. [OpenGL ES 2.0.24] section 3.7 page 63.

#include "libANGLE/Texture.h"

#include "common/mathutil.h"
#include "common/utilities.h"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/Image.h"
#include "libANGLE/Surface.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/TextureImpl.h"

namespace gl
{

namespace
{
bool IsPointSampled(const SamplerState &samplerState)
{
    return (samplerState.magFilter == GL_NEAREST &&
            (samplerState.minFilter == GL_NEAREST ||
             samplerState.minFilter == GL_NEAREST_MIPMAP_NEAREST));
}

size_t GetImageDescIndex(TextureTarget target, size_t level)
{
    return TextureTargetToType(target) == TextureType::CubeMap
               ? (level * 6 + CubeMapTextureTargetToFaceIndex(target))
               : level;
}

InitState DetermineInitState(const Context *context, const uint8_t *pixels)
{
    // Can happen in tests.
    if (!context || !context->isRobustResourceInitEnabled())
        return InitState::Initialized;

    const auto &glState = context->getGLState();
    return (pixels == nullptr && glState.getTargetBuffer(gl::BufferBinding::PixelUnpack) == nullptr)
               ? InitState::MayNeedInit
               : InitState::Initialized;
}

}  // namespace

bool IsMipmapFiltered(const SamplerState &samplerState)
{
    switch (samplerState.minFilter)
    {
        case GL_NEAREST:
        case GL_LINEAR:
            return false;
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_LINEAR:
            return true;
        default:
            UNREACHABLE();
            return false;
    }
}

SwizzleState::SwizzleState()
    : swizzleRed(GL_RED), swizzleGreen(GL_GREEN), swizzleBlue(GL_BLUE), swizzleAlpha(GL_ALPHA)
{
}

SwizzleState::SwizzleState(GLenum red, GLenum green, GLenum blue, GLenum alpha)
    : swizzleRed(red), swizzleGreen(green), swizzleBlue(blue), swizzleAlpha(alpha)
{
}

bool SwizzleState::swizzleRequired() const
{
    return swizzleRed != GL_RED || swizzleGreen != GL_GREEN || swizzleBlue != GL_BLUE ||
           swizzleAlpha != GL_ALPHA;
}

bool SwizzleState::operator==(const SwizzleState &other) const
{
    return swizzleRed == other.swizzleRed && swizzleGreen == other.swizzleGreen &&
           swizzleBlue == other.swizzleBlue && swizzleAlpha == other.swizzleAlpha;
}

bool SwizzleState::operator!=(const SwizzleState &other) const
{
    return !(*this == other);
}

TextureState::TextureState(TextureType type)
    : mType(type),
      mSamplerState(SamplerState::CreateDefaultForTarget(type)),
      mBaseLevel(0),
      mMaxLevel(1000),
      mDepthStencilTextureMode(GL_DEPTH_COMPONENT),
      mImmutableFormat(false),
      mImmutableLevels(0),
      mUsage(GL_NONE),
      mImageDescs((IMPLEMENTATION_MAX_TEXTURE_LEVELS + 1) * (type == TextureType::CubeMap ? 6 : 1)),
      mCropRect(0, 0, 0, 0),
      mGenerateMipmapHint(GL_FALSE),
      mInitState(InitState::MayNeedInit)
{
}

TextureState::~TextureState()
{
}

bool TextureState::swizzleRequired() const
{
    return mSwizzleState.swizzleRequired();
}

GLuint TextureState::getEffectiveBaseLevel() const
{
    if (mImmutableFormat)
    {
        // GLES 3.0.4 section 3.8.10
        return std::min(mBaseLevel, mImmutableLevels - 1);
    }
    // Some classes use the effective base level to index arrays with level data. By clamping the
    // effective base level to max levels these arrays need just one extra item to store properties
    // that should be returned for all out-of-range base level values, instead of needing special
    // handling for out-of-range base levels.
    return std::min(mBaseLevel, static_cast<GLuint>(IMPLEMENTATION_MAX_TEXTURE_LEVELS));
}

GLuint TextureState::getEffectiveMaxLevel() const
{
    if (mImmutableFormat)
    {
        // GLES 3.0.4 section 3.8.10
        GLuint clampedMaxLevel = std::max(mMaxLevel, getEffectiveBaseLevel());
        clampedMaxLevel        = std::min(clampedMaxLevel, mImmutableLevels - 1);
        return clampedMaxLevel;
    }
    return mMaxLevel;
}

GLuint TextureState::getMipmapMaxLevel() const
{
    const ImageDesc &baseImageDesc = getImageDesc(getBaseImageTarget(), getEffectiveBaseLevel());
    GLuint expectedMipLevels       = 0;
    if (mType == TextureType::_3D)
    {
        const int maxDim  = std::max(std::max(baseImageDesc.size.width, baseImageDesc.size.height),
                                    baseImageDesc.size.depth);
        expectedMipLevels = static_cast<GLuint>(log2(maxDim));
    }
    else
    {
        expectedMipLevels = static_cast<GLuint>(
            log2(std::max(baseImageDesc.size.width, baseImageDesc.size.height)));
    }

    return std::min<GLuint>(getEffectiveBaseLevel() + expectedMipLevels, getEffectiveMaxLevel());
}

bool TextureState::setBaseLevel(GLuint baseLevel)
{
    if (mBaseLevel != baseLevel)
    {
        mBaseLevel = baseLevel;
        return true;
    }
    return false;
}

bool TextureState::setMaxLevel(GLuint maxLevel)
{
    if (mMaxLevel != maxLevel)
    {
        mMaxLevel = maxLevel;
        return true;
    }

    return false;
}

// Tests for cube texture completeness. [OpenGL ES 2.0.24] section 3.7.10 page 81.
// According to [OpenGL ES 3.0.5] section 3.8.13 Texture Completeness page 160 any
// per-level checks begin at the base-level.
// For OpenGL ES2 the base level is always zero.
bool TextureState::isCubeComplete() const
{
    ASSERT(mType == TextureType::CubeMap);

    angle::EnumIterator<TextureTarget> face = kCubeMapTextureTargetMin;
    const ImageDesc &baseImageDesc          = getImageDesc(*face, getEffectiveBaseLevel());
    if (baseImageDesc.size.width == 0 || baseImageDesc.size.width != baseImageDesc.size.height)
    {
        return false;
    }

    ++face;

    for (; face != kAfterCubeMapTextureTargetMax; ++face)
    {
        const ImageDesc &faceImageDesc = getImageDesc(*face, getEffectiveBaseLevel());
        if (faceImageDesc.size.width != baseImageDesc.size.width ||
            faceImageDesc.size.height != baseImageDesc.size.height ||
            !Format::SameSized(faceImageDesc.format, baseImageDesc.format))
        {
            return false;
        }
    }

    return true;
}

const ImageDesc &TextureState::getBaseLevelDesc() const
{
    ASSERT(mType != TextureType::CubeMap || isCubeComplete());
    return getImageDesc(getBaseImageTarget(), getEffectiveBaseLevel());
}

void TextureState::setCrop(const gl::Rectangle& rect)
{
    mCropRect = rect;
}

const gl::Rectangle& TextureState::getCrop() const
{
    return mCropRect;
}

void TextureState::setGenerateMipmapHint(GLenum hint)
{
    mGenerateMipmapHint = hint;
}

GLenum TextureState::getGenerateMipmapHint() const
{
    return mGenerateMipmapHint;
}

bool TextureState::computeSamplerCompleteness(const SamplerState &samplerState,
                                              const ContextState &data) const
{
    if (mBaseLevel > mMaxLevel)
    {
        return false;
    }
    const ImageDesc &baseImageDesc = getImageDesc(getBaseImageTarget(), getEffectiveBaseLevel());
    if (baseImageDesc.size.width == 0 || baseImageDesc.size.height == 0 ||
        baseImageDesc.size.depth == 0)
    {
        return false;
    }
    // The cases where the texture is incomplete because base level is out of range should be
    // handled by the above condition.
    ASSERT(mBaseLevel < IMPLEMENTATION_MAX_TEXTURE_LEVELS || mImmutableFormat);

    if (mType == TextureType::CubeMap && baseImageDesc.size.width != baseImageDesc.size.height)
    {
        return false;
    }

    // According to es 3.1 spec, texture is justified as incomplete if sized internalformat is
    // unfilterable(table 20.11) and filter is not GL_NEAREST(8.16). The default value of minFilter
    // is NEAREST_MIPMAP_LINEAR and magFilter is LINEAR(table 20.11,). For multismaple texture,
    // filter state of multisample texture is ignored(11.1.3.3). So it shouldn't be judged as
    // incomplete texture. So, we ignore filtering for multisample texture completeness here.
    if (mType != TextureType::_2DMultisample &&
        !baseImageDesc.format.info->filterSupport(data.getClientVersion(), data.getExtensions()) &&
        !IsPointSampled(samplerState))
    {
        return false;
    }
    bool npotSupport = data.getExtensions().textureNPOT || data.getClientMajorVersion() >= 3;
    if (!npotSupport)
    {
        if ((samplerState.wrapS != GL_CLAMP_TO_EDGE && !isPow2(baseImageDesc.size.width)) ||
            (samplerState.wrapT != GL_CLAMP_TO_EDGE && !isPow2(baseImageDesc.size.height)))
        {
            return false;
        }
    }

    if (mType != TextureType::_2DMultisample && IsMipmapFiltered(samplerState))
    {
        if (!npotSupport)
        {
            if (!isPow2(baseImageDesc.size.width) || !isPow2(baseImageDesc.size.height))
            {
                return false;
            }
        }

        if (!computeMipmapCompleteness())
        {
            return false;
        }
    }
    else
    {
        if (mType == TextureType::CubeMap && !isCubeComplete())
        {
            return false;
        }
    }

    // From GL_OES_EGL_image_external_essl3: If state is present in a sampler object bound to a
    // texture unit that would have been rejected by a call to TexParameter* for the texture bound
    // to that unit, the behavior of the implementation is as if the texture were incomplete. For
    // example, if TEXTURE_WRAP_S or TEXTURE_WRAP_T is set to anything but CLAMP_TO_EDGE on the
    // sampler object bound to a texture unit and the texture bound to that unit is an external
    // texture, the texture will be considered incomplete.
    // Sampler object state which does not affect sampling for the type of texture bound to a
    // texture unit, such as TEXTURE_WRAP_R for an external texture, does not affect completeness.
    if (mType == TextureType::External)
    {
        if (samplerState.wrapS != GL_CLAMP_TO_EDGE || samplerState.wrapT != GL_CLAMP_TO_EDGE)
        {
            return false;
        }

        if (samplerState.minFilter != GL_LINEAR && samplerState.minFilter != GL_NEAREST)
        {
            return false;
        }
    }

    // OpenGLES 3.0.2 spec section 3.8.13 states that a texture is not mipmap complete if:
    // The internalformat specified for the texture arrays is a sized internal depth or
    // depth and stencil format (see table 3.13), the value of TEXTURE_COMPARE_-
    // MODE is NONE, and either the magnification filter is not NEAREST or the mini-
    // fication filter is neither NEAREST nor NEAREST_MIPMAP_NEAREST.
    if (mType != TextureType::_2DMultisample && baseImageDesc.format.info->depthBits > 0 &&
        data.getClientMajorVersion() >= 3)
    {
        // Note: we restrict this validation to sized types. For the OES_depth_textures
        // extension, due to some underspecification problems, we must allow linear filtering
        // for legacy compatibility with WebGL 1.
        // See http://crbug.com/649200
        if (samplerState.compareMode == GL_NONE && baseImageDesc.format.info->sized)
        {
            if ((samplerState.minFilter != GL_NEAREST &&
                 samplerState.minFilter != GL_NEAREST_MIPMAP_NEAREST) ||
                samplerState.magFilter != GL_NEAREST)
            {
                return false;
            }
        }
    }

    // OpenGLES 3.1 spec section 8.16 states that a texture is not mipmap complete if:
    // The internalformat specified for the texture is DEPTH_STENCIL format, the value of
    // DEPTH_STENCIL_TEXTURE_MODE is STENCIL_INDEX, and either the magnification filter is
    // not NEAREST or the minification filter is neither NEAREST nor NEAREST_MIPMAP_NEAREST.
    // However, the ES 3.1 spec differs from the statement above, because it is incorrect.
    // See the issue at https://github.com/KhronosGroup/OpenGL-API/issues/33.
    // For multismaple texture, filter state of multisample texture is ignored(11.1.3.3).
    // So it shouldn't be judged as incomplete texture. So, we ignore filtering for multisample
    // texture completeness here.
    if (mType != TextureType::_2DMultisample && baseImageDesc.format.info->depthBits > 0 &&
        mDepthStencilTextureMode == GL_STENCIL_INDEX)
    {
        if ((samplerState.minFilter != GL_NEAREST &&
             samplerState.minFilter != GL_NEAREST_MIPMAP_NEAREST) ||
            samplerState.magFilter != GL_NEAREST)
        {
            return false;
        }
    }

    return true;
}

bool TextureState::computeMipmapCompleteness() const
{
    const GLuint maxLevel = getMipmapMaxLevel();

    for (GLuint level = getEffectiveBaseLevel(); level <= maxLevel; level++)
    {
        if (mType == TextureType::CubeMap)
        {
            for (TextureTarget face : AllCubeFaceTextureTargets())
            {
                if (!computeLevelCompleteness(face, level))
                {
                    return false;
                }
            }
        }
        else
        {
            if (!computeLevelCompleteness(NonCubeTextureTypeToTarget(mType), level))
            {
                return false;
            }
        }
    }

    return true;
}

bool TextureState::computeLevelCompleteness(TextureTarget target, size_t level) const
{
    ASSERT(level < IMPLEMENTATION_MAX_TEXTURE_LEVELS);

    if (mImmutableFormat)
    {
        return true;
    }

    const ImageDesc &baseImageDesc = getImageDesc(getBaseImageTarget(), getEffectiveBaseLevel());
    if (baseImageDesc.size.width == 0 || baseImageDesc.size.height == 0 ||
        baseImageDesc.size.depth == 0)
    {
        return false;
    }

    const ImageDesc &levelImageDesc = getImageDesc(target, level);
    if (levelImageDesc.size.width == 0 || levelImageDesc.size.height == 0 ||
        levelImageDesc.size.depth == 0)
    {
        return false;
    }

    if (!Format::SameSized(levelImageDesc.format, baseImageDesc.format))
    {
        return false;
    }

    ASSERT(level >= getEffectiveBaseLevel());
    const size_t relativeLevel = level - getEffectiveBaseLevel();
    if (levelImageDesc.size.width != std::max(1, baseImageDesc.size.width >> relativeLevel))
    {
        return false;
    }

    if (levelImageDesc.size.height != std::max(1, baseImageDesc.size.height >> relativeLevel))
    {
        return false;
    }

    if (mType == TextureType::_3D)
    {
        if (levelImageDesc.size.depth != std::max(1, baseImageDesc.size.depth >> relativeLevel))
        {
            return false;
        }
    }
    else if (mType == TextureType::_2DArray)
    {
        if (levelImageDesc.size.depth != baseImageDesc.size.depth)
        {
            return false;
        }
    }

    return true;
}

TextureTarget TextureState::getBaseImageTarget() const
{
    return mType == TextureType::CubeMap ? kCubeMapTextureTargetMin
                                         : NonCubeTextureTypeToTarget(mType);
}

ImageDesc::ImageDesc()
    : ImageDesc(Extents(0, 0, 0), Format::Invalid(), 0, GL_TRUE, InitState::Initialized)
{
}

ImageDesc::ImageDesc(const Extents &size, const Format &format, const InitState initState)
    : size(size), format(format), samples(0), fixedSampleLocations(GL_TRUE), initState(initState)
{
}

ImageDesc::ImageDesc(const Extents &size,
                     const Format &format,
                     const GLsizei samples,
                     const bool fixedSampleLocations,
                     const InitState initState)
    : size(size),
      format(format),
      samples(samples),
      fixedSampleLocations(fixedSampleLocations),
      initState(initState)
{
}

const ImageDesc &TextureState::getImageDesc(TextureTarget target, size_t level) const
{
    size_t descIndex = GetImageDescIndex(target, level);
    ASSERT(descIndex < mImageDescs.size());
    return mImageDescs[descIndex];
}

void TextureState::setImageDesc(TextureTarget target, size_t level, const ImageDesc &desc)
{
    size_t descIndex = GetImageDescIndex(target, level);
    ASSERT(descIndex < mImageDescs.size());
    mImageDescs[descIndex] = desc;
    if (desc.initState == InitState::MayNeedInit)
    {
        mInitState = InitState::MayNeedInit;
    }
}

const ImageDesc &TextureState::getImageDesc(const ImageIndex &imageIndex) const
{
    return getImageDesc(imageIndex.getTarget(), imageIndex.getLevelIndex());
}

void TextureState::setImageDescChain(GLuint baseLevel,
                                     GLuint maxLevel,
                                     Extents baseSize,
                                     const Format &format,
                                     InitState initState)
{
    for (GLuint level = baseLevel; level <= maxLevel; level++)
    {
        int relativeLevel = (level - baseLevel);
        Extents levelSize(std::max<int>(baseSize.width >> relativeLevel, 1),
                          std::max<int>(baseSize.height >> relativeLevel, 1),
                          (mType == TextureType::_2DArray)
                              ? baseSize.depth
                              : std::max<int>(baseSize.depth >> relativeLevel, 1));
        ImageDesc levelInfo(levelSize, format, initState);

        if (mType == TextureType::CubeMap)
        {
            for (TextureTarget face : AllCubeFaceTextureTargets())
            {
                setImageDesc(face, level, levelInfo);
            }
        }
        else
        {
            setImageDesc(NonCubeTextureTypeToTarget(mType), level, levelInfo);
        }
    }
}

void TextureState::setImageDescChainMultisample(Extents baseSize,
                                                const Format &format,
                                                GLsizei samples,
                                                bool fixedSampleLocations,
                                                InitState initState)
{
    ASSERT(mType == TextureType::_2DMultisample);
    ImageDesc levelInfo(baseSize, format, samples, fixedSampleLocations, initState);
    setImageDesc(TextureTarget::_2DMultisample, 0, levelInfo);
}

void TextureState::clearImageDesc(TextureTarget target, size_t level)
{
    setImageDesc(target, level, ImageDesc());
}

void TextureState::clearImageDescs()
{
    for (size_t descIndex = 0; descIndex < mImageDescs.size(); descIndex++)
    {
        mImageDescs[descIndex] = ImageDesc();
    }
}

Texture::Texture(rx::GLImplFactory *factory, GLuint id, TextureType type)
    : egl::ImageSibling(id),
      mState(type),
      mTexture(factory->createTexture(mState)),
      mLabel(),
      mBoundSurface(nullptr),
      mBoundStream(nullptr)
{
}

Error Texture::onDestroy(const Context *context)
{
    if (mBoundSurface)
    {
        ANGLE_TRY(mBoundSurface->releaseTexImage(context, EGL_BACK_BUFFER));
        mBoundSurface = nullptr;
    }
    if (mBoundStream)
    {
        mBoundStream->releaseTextures();
        mBoundStream = nullptr;
    }

    ANGLE_TRY(orphanImages(context));

    if (mTexture)
    {
        ANGLE_TRY(mTexture->onDestroy(context));
    }
    return NoError();
}

Texture::~Texture()
{
    SafeDelete(mTexture);
}

void Texture::setLabel(const std::string &label)
{
    mLabel = label;
    mDirtyBits.set(DIRTY_BIT_LABEL);
}

const std::string &Texture::getLabel() const
{
    return mLabel;
}

TextureType Texture::getType() const
{
    return mState.mType;
}

void Texture::setSwizzleRed(GLenum swizzleRed)
{
    mState.mSwizzleState.swizzleRed = swizzleRed;
    mDirtyBits.set(DIRTY_BIT_SWIZZLE_RED);
}

GLenum Texture::getSwizzleRed() const
{
    return mState.mSwizzleState.swizzleRed;
}

void Texture::setSwizzleGreen(GLenum swizzleGreen)
{
    mState.mSwizzleState.swizzleGreen = swizzleGreen;
    mDirtyBits.set(DIRTY_BIT_SWIZZLE_GREEN);
}

GLenum Texture::getSwizzleGreen() const
{
    return mState.mSwizzleState.swizzleGreen;
}

void Texture::setSwizzleBlue(GLenum swizzleBlue)
{
    mState.mSwizzleState.swizzleBlue = swizzleBlue;
    mDirtyBits.set(DIRTY_BIT_SWIZZLE_BLUE);
}

GLenum Texture::getSwizzleBlue() const
{
    return mState.mSwizzleState.swizzleBlue;
}

void Texture::setSwizzleAlpha(GLenum swizzleAlpha)
{
    mState.mSwizzleState.swizzleAlpha = swizzleAlpha;
    mDirtyBits.set(DIRTY_BIT_SWIZZLE_ALPHA);
}

GLenum Texture::getSwizzleAlpha() const
{
    return mState.mSwizzleState.swizzleAlpha;
}

void Texture::setMinFilter(GLenum minFilter)
{
    mState.mSamplerState.minFilter = minFilter;
    mDirtyBits.set(DIRTY_BIT_MIN_FILTER);
}

GLenum Texture::getMinFilter() const
{
    return mState.mSamplerState.minFilter;
}

void Texture::setMagFilter(GLenum magFilter)
{
    mState.mSamplerState.magFilter = magFilter;
    mDirtyBits.set(DIRTY_BIT_MAG_FILTER);
}

GLenum Texture::getMagFilter() const
{
    return mState.mSamplerState.magFilter;
}

void Texture::setWrapS(GLenum wrapS)
{
    mState.mSamplerState.wrapS = wrapS;
    mDirtyBits.set(DIRTY_BIT_WRAP_S);
}

GLenum Texture::getWrapS() const
{
    return mState.mSamplerState.wrapS;
}

void Texture::setWrapT(GLenum wrapT)
{
    mState.mSamplerState.wrapT = wrapT;
    mDirtyBits.set(DIRTY_BIT_WRAP_T);
}

GLenum Texture::getWrapT() const
{
    return mState.mSamplerState.wrapT;
}

void Texture::setWrapR(GLenum wrapR)
{
    mState.mSamplerState.wrapR = wrapR;
    mDirtyBits.set(DIRTY_BIT_WRAP_R);
}

GLenum Texture::getWrapR() const
{
    return mState.mSamplerState.wrapR;
}

void Texture::setMaxAnisotropy(float maxAnisotropy)
{
    mState.mSamplerState.maxAnisotropy = maxAnisotropy;
    mDirtyBits.set(DIRTY_BIT_MAX_ANISOTROPY);
}

float Texture::getMaxAnisotropy() const
{
    return mState.mSamplerState.maxAnisotropy;
}

void Texture::setMinLod(GLfloat minLod)
{
    mState.mSamplerState.minLod = minLod;
    mDirtyBits.set(DIRTY_BIT_MIN_LOD);
}

GLfloat Texture::getMinLod() const
{
    return mState.mSamplerState.minLod;
}

void Texture::setMaxLod(GLfloat maxLod)
{
    mState.mSamplerState.maxLod = maxLod;
    mDirtyBits.set(DIRTY_BIT_MAX_LOD);
}

GLfloat Texture::getMaxLod() const
{
    return mState.mSamplerState.maxLod;
}

void Texture::setCompareMode(GLenum compareMode)
{
    mState.mSamplerState.compareMode = compareMode;
    mDirtyBits.set(DIRTY_BIT_COMPARE_MODE);
}

GLenum Texture::getCompareMode() const
{
    return mState.mSamplerState.compareMode;
}

void Texture::setCompareFunc(GLenum compareFunc)
{
    mState.mSamplerState.compareFunc = compareFunc;
    mDirtyBits.set(DIRTY_BIT_COMPARE_FUNC);
}

GLenum Texture::getCompareFunc() const
{
    return mState.mSamplerState.compareFunc;
}

void Texture::setSRGBDecode(GLenum sRGBDecode)
{
    mState.mSamplerState.sRGBDecode = sRGBDecode;
    mDirtyBits.set(DIRTY_BIT_SRGB_DECODE);
}

GLenum Texture::getSRGBDecode() const
{
    return mState.mSamplerState.sRGBDecode;
}

const SamplerState &Texture::getSamplerState() const
{
    return mState.mSamplerState;
}

Error Texture::setBaseLevel(const Context *context, GLuint baseLevel)
{
    if (mState.setBaseLevel(baseLevel))
    {
        ANGLE_TRY(mTexture->setBaseLevel(context, mState.getEffectiveBaseLevel()));
        mDirtyBits.set(DIRTY_BIT_BASE_LEVEL);
        invalidateCompletenessCache();
    }

    return NoError();
}

GLuint Texture::getBaseLevel() const
{
    return mState.mBaseLevel;
}

void Texture::setMaxLevel(GLuint maxLevel)
{
    if (mState.setMaxLevel(maxLevel))
    {
        mDirtyBits.set(DIRTY_BIT_MAX_LEVEL);
        invalidateCompletenessCache();
    }
}

GLuint Texture::getMaxLevel() const
{
    return mState.mMaxLevel;
}

void Texture::setDepthStencilTextureMode(GLenum mode)
{
    if (mState.mDepthStencilTextureMode != mode)
    {
        mState.mDepthStencilTextureMode = mode;
        mDirtyBits.set(DIRTY_BIT_DEPTH_STENCIL_TEXTURE_MODE);
        invalidateCompletenessCache();
    }
}

GLenum Texture::getDepthStencilTextureMode() const
{
    return mState.mDepthStencilTextureMode;
}

bool Texture::getImmutableFormat() const
{
    return mState.mImmutableFormat;
}

GLuint Texture::getImmutableLevels() const
{
    return mState.mImmutableLevels;
}

void Texture::setUsage(GLenum usage)
{
    mState.mUsage = usage;
    mDirtyBits.set(DIRTY_BIT_USAGE);
}

GLenum Texture::getUsage() const
{
    return mState.mUsage;
}

const TextureState &Texture::getTextureState() const
{
    return mState;
}

size_t Texture::getWidth(TextureTarget target, size_t level) const
{
    ASSERT(TextureTargetToType(target) == mState.mType);
    return mState.getImageDesc(target, level).size.width;
}

size_t Texture::getHeight(TextureTarget target, size_t level) const
{
    ASSERT(TextureTargetToType(target) == mState.mType);
    return mState.getImageDesc(target, level).size.height;
}

size_t Texture::getDepth(TextureTarget target, size_t level) const
{
    ASSERT(TextureTargetToType(target) == mState.mType);
    return mState.getImageDesc(target, level).size.depth;
}

const Format &Texture::getFormat(TextureTarget target, size_t level) const
{
    ASSERT(TextureTargetToType(target) == mState.mType);
    return mState.getImageDesc(target, level).format;
}

GLsizei Texture::getSamples(TextureTarget target, size_t level) const
{
    ASSERT(TextureTargetToType(target) == mState.mType);
    return mState.getImageDesc(target, level).samples;
}

bool Texture::getFixedSampleLocations(TextureTarget target, size_t level) const
{
    ASSERT(TextureTargetToType(target) == mState.mType);
    return mState.getImageDesc(target, level).fixedSampleLocations;
}

GLuint Texture::getMipmapMaxLevel() const
{
    return mState.getMipmapMaxLevel();
}

bool Texture::isMipmapComplete() const
{
    return mState.computeMipmapCompleteness();
}

egl::Surface *Texture::getBoundSurface() const
{
    return mBoundSurface;
}

egl::Stream *Texture::getBoundStream() const
{
    return mBoundStream;
}

void Texture::signalDirty(const Context *context, InitState initState)
{
    mState.mInitState = initState;
    onStorageChange(context);
    invalidateCompletenessCache();
}

Error Texture::setImage(const Context *context,
                        const PixelUnpackState &unpackState,
                        TextureTarget target,
                        GLint level,
                        GLenum internalFormat,
                        const Extents &size,
                        GLenum format,
                        GLenum type,
                        const uint8_t *pixels)
{
    ASSERT(TextureTargetToType(target) == mState.mType);

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    ImageIndex index = ImageIndex::MakeFromTarget(target, level);

    ANGLE_TRY(mTexture->setImage(context, index, internalFormat, size, format, type, unpackState,
                                 pixels));

    InitState initState = DetermineInitState(context, pixels);
    mState.setImageDesc(target, level, ImageDesc(size, Format(internalFormat, type), initState));
    signalDirty(context, initState);

    return NoError();
}

Error Texture::setSubImage(const Context *context,
                           const PixelUnpackState &unpackState,
                           TextureTarget target,
                           GLint level,
                           const Box &area,
                           GLenum format,
                           GLenum type,
                           const uint8_t *pixels)
{
    ASSERT(TextureTargetToType(target) == mState.mType);

    ANGLE_TRY(ensureSubImageInitialized(context, target, level, area));

    ImageIndex index = ImageIndex::MakeFromTarget(target, level);

    return mTexture->setSubImage(context, index, area, format, type, unpackState, pixels);
}

Error Texture::setCompressedImage(const Context *context,
                                  const PixelUnpackState &unpackState,
                                  TextureTarget target,
                                  GLint level,
                                  GLenum internalFormat,
                                  const Extents &size,
                                  size_t imageSize,
                                  const uint8_t *pixels)
{
    ASSERT(TextureTargetToType(target) == mState.mType);

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    ImageIndex index = ImageIndex::MakeFromTarget(target, level);

    ANGLE_TRY(mTexture->setCompressedImage(context, index, internalFormat, size, unpackState,
                                           imageSize, pixels));

    InitState initState = DetermineInitState(context, pixels);
    mState.setImageDesc(target, level, ImageDesc(size, Format(internalFormat), initState));
    signalDirty(context, initState);

    return NoError();
}

Error Texture::setCompressedSubImage(const Context *context,
                                     const PixelUnpackState &unpackState,
                                     TextureTarget target,
                                     GLint level,
                                     const Box &area,
                                     GLenum format,
                                     size_t imageSize,
                                     const uint8_t *pixels)
{
    ASSERT(TextureTargetToType(target) == mState.mType);

    ANGLE_TRY(ensureSubImageInitialized(context, target, level, area));

    ImageIndex index = ImageIndex::MakeFromTarget(target, level);

    return mTexture->setCompressedSubImage(context, index, area, format, unpackState, imageSize,
                                           pixels);
}

Error Texture::copyImage(const Context *context,
                         TextureTarget target,
                         GLint level,
                         const Rectangle &sourceArea,
                         GLenum internalFormat,
                         Framebuffer *source)
{
    ASSERT(TextureTargetToType(target) == mState.mType);

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    // Ensure source FBO is initialized.
    ANGLE_TRY(source->ensureReadAttachmentInitialized(context, GL_COLOR_BUFFER_BIT));

    // Use the source FBO size as the init image area.
    Box destBox(0, 0, 0, sourceArea.width, sourceArea.height, 1);
    ANGLE_TRY(ensureSubImageInitialized(context, target, level, destBox));

    ImageIndex index = ImageIndex::MakeFromTarget(target, level);

    ANGLE_TRY(mTexture->copyImage(context, index, sourceArea, internalFormat, source));

    const InternalFormat &internalFormatInfo =
        GetInternalFormatInfo(internalFormat, GL_UNSIGNED_BYTE);

    mState.setImageDesc(target, level,
                        ImageDesc(Extents(sourceArea.width, sourceArea.height, 1),
                                  Format(internalFormatInfo), InitState::Initialized));

    // We need to initialize this texture only if the source attachment is not initialized.
    signalDirty(context, InitState::Initialized);

    return NoError();
}

Error Texture::copySubImage(const Context *context,
                            TextureTarget target,
                            GLint level,
                            const Offset &destOffset,
                            const Rectangle &sourceArea,
                            Framebuffer *source)
{
    ASSERT(TextureTargetToType(target) == mState.mType);

    // Ensure source FBO is initialized.
    ANGLE_TRY(source->ensureReadAttachmentInitialized(context, GL_COLOR_BUFFER_BIT));

    Box destBox(destOffset.x, destOffset.y, destOffset.y, sourceArea.width, sourceArea.height, 1);
    ANGLE_TRY(ensureSubImageInitialized(context, target, level, destBox));

    ImageIndex index = ImageIndex::MakeFromTarget(target, level);

    return mTexture->copySubImage(context, index, destOffset, sourceArea, source);
}

Error Texture::copyTexture(const Context *context,
                           TextureTarget target,
                           GLint level,
                           GLenum internalFormat,
                           GLenum type,
                           GLint sourceLevel,
                           bool unpackFlipY,
                           bool unpackPremultiplyAlpha,
                           bool unpackUnmultiplyAlpha,
                           Texture *source)
{
    ASSERT(TextureTargetToType(target) == mState.mType);
    ASSERT(source->getType() != TextureType::CubeMap);

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    // Initialize source texture.
    // Note: we don't have a way to notify which portions of the image changed currently.
    ANGLE_TRY(source->ensureInitialized(context));

    ImageIndex index = ImageIndex::MakeFromTarget(target, level);

    ANGLE_TRY(mTexture->copyTexture(context, index, internalFormat, type, sourceLevel, unpackFlipY,
                                    unpackPremultiplyAlpha, unpackUnmultiplyAlpha, source));

    const auto &sourceDesc =
        source->mState.getImageDesc(NonCubeTextureTypeToTarget(source->getType()), 0);
    const InternalFormat &internalFormatInfo = GetInternalFormatInfo(internalFormat, type);
    mState.setImageDesc(
        target, level,
        ImageDesc(sourceDesc.size, Format(internalFormatInfo), InitState::Initialized));

    signalDirty(context, InitState::Initialized);

    return NoError();
}

Error Texture::copySubTexture(const Context *context,
                              TextureTarget target,
                              GLint level,
                              const Offset &destOffset,
                              GLint sourceLevel,
                              const Rectangle &sourceArea,
                              bool unpackFlipY,
                              bool unpackPremultiplyAlpha,
                              bool unpackUnmultiplyAlpha,
                              Texture *source)
{
    ASSERT(TextureTargetToType(target) == mState.mType);

    // Ensure source is initialized.
    ANGLE_TRY(source->ensureInitialized(context));

    Box destBox(destOffset.x, destOffset.y, destOffset.y, sourceArea.width, sourceArea.height, 1);
    ANGLE_TRY(ensureSubImageInitialized(context, target, level, destBox));

    ImageIndex index = ImageIndex::MakeFromTarget(target, level);

    return mTexture->copySubTexture(context, index, destOffset, sourceLevel, sourceArea,
                                    unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha,
                                    source);
}

Error Texture::copyCompressedTexture(const Context *context, const Texture *source)
{
    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    ANGLE_TRY(mTexture->copyCompressedTexture(context, source));

    ASSERT(source->getType() != TextureType::CubeMap && getType() != TextureType::CubeMap);
    const auto &sourceDesc =
        source->mState.getImageDesc(NonCubeTextureTypeToTarget(source->getType()), 0);
    mState.setImageDesc(NonCubeTextureTypeToTarget(getType()), 0, sourceDesc);

    return NoError();
}

Error Texture::setStorage(const Context *context,
                          TextureType type,
                          GLsizei levels,
                          GLenum internalFormat,
                          const Extents &size)
{
    ASSERT(type == mState.mType);

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    ANGLE_TRY(mTexture->setStorage(context, type, levels, internalFormat, size));

    mState.mImmutableFormat = true;
    mState.mImmutableLevels = static_cast<GLuint>(levels);
    mState.clearImageDescs();
    mState.setImageDescChain(0, static_cast<GLuint>(levels - 1), size, Format(internalFormat),
                             InitState::MayNeedInit);

    // Changing the texture to immutable can trigger a change in the base and max levels:
    // GLES 3.0.4 section 3.8.10 pg 158:
    // "For immutable-format textures, levelbase is clamped to the range[0;levels],levelmax is then
    // clamped to the range[levelbase;levels].
    mDirtyBits.set(DIRTY_BIT_BASE_LEVEL);
    mDirtyBits.set(DIRTY_BIT_MAX_LEVEL);

    signalDirty(context, InitState::MayNeedInit);

    return NoError();
}

Error Texture::setStorageMultisample(const Context *context,
                                     TextureType type,
                                     GLsizei samples,
                                     GLint internalFormat,
                                     const Extents &size,
                                     bool fixedSampleLocations)
{
    ASSERT(type == mState.mType);

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    ANGLE_TRY(mTexture->setStorageMultisample(context, type, samples, internalFormat, size,
                                              fixedSampleLocations));

    mState.mImmutableFormat = true;
    mState.mImmutableLevels = static_cast<GLuint>(1);
    mState.clearImageDescs();
    mState.setImageDescChainMultisample(size, Format(internalFormat), samples, fixedSampleLocations,
                                        InitState::MayNeedInit);

    signalDirty(context, InitState::MayNeedInit);

    return NoError();
}

Error Texture::generateMipmap(const Context *context)
{
    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));

    // EGL_KHR_gl_image states that images are only orphaned when generating mipmaps if the texture
    // is not mip complete.
    if (!isMipmapComplete())
    {
        ANGLE_TRY(orphanImages(context));
    }

    const GLuint baseLevel = mState.getEffectiveBaseLevel();
    const GLuint maxLevel  = mState.getMipmapMaxLevel();

    if (maxLevel <= baseLevel)
    {
        return NoError();
    }
    ANGLE_TRY(syncState(context));

    // Clear the base image(s) immediately if needed
    if (context->isRobustResourceInitEnabled())
    {
        ImageIndexIterator it =
            ImageIndexIterator::MakeGeneric(mState.mType, baseLevel, baseLevel + 1,
                                            ImageIndex::kEntireLevel, ImageIndex::kEntireLevel);
        while (it.hasNext())
        {
            const ImageIndex index = it.next();
            const ImageDesc &desc  = mState.getImageDesc(index.getTarget(), index.getLevelIndex());

            if (desc.initState == InitState::MayNeedInit)
            {
                ANGLE_TRY(initializeContents(context, index));
            }
        }
    }

    ANGLE_TRY(mTexture->generateMipmap(context));

    // Propagate the format and size of the bsae mip to the smaller ones. Cube maps are guaranteed
    // to have faces of the same size and format so any faces can be picked.
    const ImageDesc &baseImageInfo = mState.getImageDesc(mState.getBaseImageTarget(), baseLevel);
    mState.setImageDescChain(baseLevel, maxLevel, baseImageInfo.size, baseImageInfo.format,
                             InitState::Initialized);

    signalDirty(context, InitState::Initialized);

    return NoError();
}

Error Texture::bindTexImageFromSurface(const Context *context, egl::Surface *surface)
{
    ASSERT(surface);

    if (mBoundSurface)
    {
        ANGLE_TRY(releaseTexImageFromSurface(context));
    }

    ANGLE_TRY(mTexture->bindTexImage(context, surface));
    mBoundSurface = surface;

    // Set the image info to the size and format of the surface
    ASSERT(mState.mType == TextureType::_2D || mState.mType == TextureType::Rectangle);
    Extents size(surface->getWidth(), surface->getHeight(), 1);
    ImageDesc desc(size, surface->getBindTexImageFormat(), InitState::Initialized);
    mState.setImageDesc(NonCubeTextureTypeToTarget(mState.mType), 0, desc);
    signalDirty(context, InitState::Initialized);
    return NoError();
}

Error Texture::releaseTexImageFromSurface(const Context *context)
{
    ASSERT(mBoundSurface);
    mBoundSurface = nullptr;
    ANGLE_TRY(mTexture->releaseTexImage(context));

    // Erase the image info for level 0
    ASSERT(mState.mType == TextureType::_2D || mState.mType == TextureType::Rectangle);
    mState.clearImageDesc(NonCubeTextureTypeToTarget(mState.mType), 0);
    signalDirty(context, InitState::Initialized);
    return NoError();
}

void Texture::bindStream(egl::Stream *stream)
{
    ASSERT(stream);

    // It should not be possible to bind a texture already bound to another stream
    ASSERT(mBoundStream == nullptr);

    mBoundStream = stream;

    ASSERT(mState.mType == TextureType::External);
}

void Texture::releaseStream()
{
    ASSERT(mBoundStream);
    mBoundStream = nullptr;
}

Error Texture::acquireImageFromStream(const Context *context,
                                      const egl::Stream::GLTextureDescription &desc)
{
    ASSERT(mBoundStream != nullptr);
    ANGLE_TRY(mTexture->setImageExternal(context, mState.mType, mBoundStream, desc));

    Extents size(desc.width, desc.height, 1);
    mState.setImageDesc(NonCubeTextureTypeToTarget(mState.mType), 0,
                        ImageDesc(size, Format(desc.internalFormat), InitState::Initialized));
    signalDirty(context, InitState::Initialized);
    return NoError();
}

Error Texture::releaseImageFromStream(const Context *context)
{
    ASSERT(mBoundStream != nullptr);
    ANGLE_TRY(mTexture->setImageExternal(context, mState.mType, nullptr,
                                         egl::Stream::GLTextureDescription()));

    // Set to incomplete
    mState.clearImageDesc(NonCubeTextureTypeToTarget(mState.mType), 0);
    signalDirty(context, InitState::Initialized);
    return NoError();
}

Error Texture::releaseTexImageInternal(const Context *context)
{
    if (mBoundSurface)
    {
        // Notify the surface
        mBoundSurface->releaseTexImageFromTexture(context);

        // Then, call the same method as from the surface
        ANGLE_TRY(releaseTexImageFromSurface(context));
    }
    return NoError();
}

Error Texture::setEGLImageTarget(const Context *context, TextureType type, egl::Image *imageTarget)
{
    ASSERT(type == mState.mType);
    ASSERT(type == TextureType::_2D || type == TextureType::External);

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    ANGLE_TRY(mTexture->setEGLImageTarget(context, type, imageTarget));

    setTargetImage(context, imageTarget);

    Extents size(static_cast<int>(imageTarget->getWidth()),
                 static_cast<int>(imageTarget->getHeight()), 1);

    auto initState = imageTarget->sourceInitState();

    mState.clearImageDescs();
    mState.setImageDesc(NonCubeTextureTypeToTarget(type), 0,
                        ImageDesc(size, imageTarget->getFormat(), initState));
    signalDirty(context, initState);

    return NoError();
}

Extents Texture::getAttachmentSize(const ImageIndex &imageIndex) const
{
    return mState.getImageDesc(imageIndex).size;
}

const Format &Texture::getAttachmentFormat(GLenum /*binding*/, const ImageIndex &imageIndex) const
{
    return mState.getImageDesc(imageIndex).format;
}

GLsizei Texture::getAttachmentSamples(const ImageIndex &imageIndex) const
{
    return getSamples(imageIndex.getTarget(), 0);
}

void Texture::setCrop(const gl::Rectangle& rect)
{
    mState.setCrop(rect);
}

const gl::Rectangle& Texture::getCrop() const
{
    return mState.getCrop();
}

void Texture::setGenerateMipmapHint(GLenum hint)
{
    mState.setGenerateMipmapHint(hint);
}

GLenum Texture::getGenerateMipmapHint() const
{
    return mState.getGenerateMipmapHint();
}

void Texture::onAttach(const Context *context)
{
    addRef();
}

void Texture::onDetach(const Context *context)
{
    release(context);
}

GLuint Texture::getId() const
{
    return id();
}

Error Texture::syncState(const Context *context)
{
    ANGLE_TRY(mTexture->syncState(context, mDirtyBits));
    mDirtyBits.reset();
    return NoError();
}

rx::FramebufferAttachmentObjectImpl *Texture::getAttachmentImpl() const
{
    return mTexture;
}

bool Texture::isSamplerComplete(const Context *context, const Sampler *optionalSampler)
{
    const auto &samplerState =
        optionalSampler ? optionalSampler->getSamplerState() : mState.mSamplerState;
    const auto &contextState = context->getContextState();

    if (contextState.getContextID() != mCompletenessCache.context ||
        mCompletenessCache.samplerState != samplerState)
    {
        mCompletenessCache.context      = context->getContextState().getContextID();
        mCompletenessCache.samplerState = samplerState;
        mCompletenessCache.samplerComplete =
            mState.computeSamplerCompleteness(samplerState, contextState);
    }

    return mCompletenessCache.samplerComplete;
}

Texture::SamplerCompletenessCache::SamplerCompletenessCache()
    : context(0), samplerState(), samplerComplete(false)
{
}

void Texture::invalidateCompletenessCache() const
{
    mCompletenessCache.context = 0;
}

Error Texture::ensureInitialized(const Context *context)
{
    if (!context->isRobustResourceInitEnabled() || mState.mInitState == InitState::Initialized)
    {
        return NoError();
    }

    bool anyDirty = false;

    ImageIndexIterator it =
        ImageIndexIterator::MakeGeneric(mState.mType, 0, IMPLEMENTATION_MAX_TEXTURE_LEVELS + 1,
                                        ImageIndex::kEntireLevel, ImageIndex::kEntireLevel);
    while (it.hasNext())
    {
        const ImageIndex index = it.next();
        ImageDesc &desc =
            mState.mImageDescs[GetImageDescIndex(index.getTarget(), index.getLevelIndex())];
        if (desc.initState == InitState::MayNeedInit)
        {
            ASSERT(mState.mInitState == InitState::MayNeedInit);
            ANGLE_TRY(initializeContents(context, index));
            desc.initState = InitState::Initialized;
            anyDirty       = true;
        }
    }
    if (anyDirty)
    {
        signalDirty(context, InitState::Initialized);
    }
    mState.mInitState = InitState::Initialized;

    return NoError();
}

InitState Texture::initState(const ImageIndex &imageIndex) const
{
    return mState.getImageDesc(imageIndex).initState;
}

InitState Texture::initState() const
{
    return mState.mInitState;
}

void Texture::setInitState(const ImageIndex &imageIndex, InitState initState)
{
    ImageDesc newDesc = mState.getImageDesc(imageIndex);
    newDesc.initState = initState;
    mState.setImageDesc(imageIndex.getTarget(), imageIndex.getLevelIndex(), newDesc);
}

Error Texture::ensureSubImageInitialized(const Context *context,
                                         TextureTarget target,
                                         size_t level,
                                         const gl::Box &area)
{
    if (!context->isRobustResourceInitEnabled() || mState.mInitState == InitState::Initialized)
    {
        return NoError();
    }

    // Pre-initialize the texture contents if necessary.
    // TODO(jmadill): Check if area overlaps the entire texture.
    ImageIndex imageIndex  = ImageIndex::MakeFromTarget(target, static_cast<GLint>(level));
    const auto &desc       = mState.getImageDesc(imageIndex);
    if (desc.initState == InitState::MayNeedInit)
    {
        ASSERT(mState.mInitState == InitState::MayNeedInit);
        bool coversWholeImage = area.x == 0 && area.y == 0 && area.z == 0 &&
                                area.width == desc.size.width && area.height == desc.size.height &&
                                area.depth == desc.size.depth;
        if (!coversWholeImage)
        {
            ANGLE_TRY(initializeContents(context, imageIndex));
        }
        setInitState(imageIndex, InitState::Initialized);
    }

    return NoError();
}

}  // namespace gl
