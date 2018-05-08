#include "ImageIndex.h"
//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ImageIndex.cpp: Implementation for ImageIndex methods.

#include "libANGLE/ImageIndex.h"
#include "libANGLE/Constants.h"
#include "common/utilities.h"

#include <tuple>

namespace gl
{

ImageIndex::ImageIndex(const ImageIndex &other) = default;
ImageIndex &ImageIndex::operator=(const ImageIndex &other) = default;

bool ImageIndex::is3D() const
{
    return type == TextureType::_3D || type == TextureType::_2DArray;
}

GLint ImageIndex::cubeMapFaceIndex() const
{
    ASSERT(type == TextureType::CubeMap);
    ASSERT(TextureTargetToType(target) == TextureType::CubeMap);
    return static_cast<GLint>(CubeMapTextureTargetToFaceIndex(target));
}

bool ImageIndex::valid() const
{
    return type != TextureType::InvalidEnum;
}

ImageIndex ImageIndex::Make2D(GLint mipIndex)
{
    return ImageIndex(TextureType::_2D, TextureTarget::_2D, mipIndex, ENTIRE_LEVEL, 1);
}

ImageIndex ImageIndex::MakeRectangle(GLint mipIndex)
{
    return ImageIndex(TextureType::Rectangle, TextureTarget::Rectangle, mipIndex, ENTIRE_LEVEL, 1);
}

ImageIndex ImageIndex::MakeCube(TextureTarget target, GLint mipIndex)
{
    ASSERT(TextureTargetToType(target) == TextureType::CubeMap);
    return ImageIndex(TextureType::CubeMap, target, mipIndex, ENTIRE_LEVEL, 1);
}

ImageIndex ImageIndex::Make2DArray(GLint mipIndex, GLint layerIndex)
{
    return ImageIndex(TextureType::_2DArray, TextureTarget::_2DArray, mipIndex, layerIndex, 1);
}

ImageIndex ImageIndex::Make2DArrayRange(GLint mipIndex, GLint layerIndex, GLint numLayers)
{
    return ImageIndex(TextureType::_2DArray, TextureTarget::_2DArray, mipIndex, layerIndex,
                      numLayers);
}

ImageIndex ImageIndex::Make3D(GLint mipIndex, GLint layerIndex)
{
    return ImageIndex(TextureType::_3D, TextureTarget::_3D, mipIndex, layerIndex, 1);
}

ImageIndex ImageIndex::MakeGeneric(TextureTarget target, GLint mipIndex)
{
    return ImageIndex(TextureTargetToType(target), target, mipIndex, ENTIRE_LEVEL, 1);
}

ImageIndex ImageIndex::Make2DMultisample()
{
    return ImageIndex(TextureType::_2DMultisample, TextureTarget::_2DMultisample, 0, ENTIRE_LEVEL,
                      1);
}

ImageIndex ImageIndex::MakeInvalid()
{
    return ImageIndex(TextureType::InvalidEnum, TextureTarget::InvalidEnum, -1, -1, -1);
}

bool operator<(const ImageIndex &a, const ImageIndex &b)
{
    return std::tie(a.type, a.target, a.mipIndex, a.layerIndex, a.numLayers) <
           std::tie(b.type, b.target, b.mipIndex, b.layerIndex, b.numLayers);
}

bool operator==(const ImageIndex &a, const ImageIndex &b)
{
    return std::tie(a.type, a.target, a.mipIndex, a.layerIndex, a.numLayers) ==
           std::tie(b.type, b.target, b.mipIndex, b.layerIndex, b.numLayers);
}

bool operator!=(const ImageIndex &a, const ImageIndex &b)
{
    return !(a == b);
}

ImageIndex::ImageIndex(TextureType typeIn,
                       TextureTarget targetIn,
                       GLint mipIndexIn,
                       GLint layerIndexIn,
                       GLint numLayersIn)
    : type(typeIn),
      target(targetIn),
      mipIndex(mipIndexIn),
      layerIndex(layerIndexIn),
      numLayers(numLayersIn)
{}

ImageIndexIterator::ImageIndexIterator(const ImageIndexIterator &other) = default;

ImageIndexIterator ImageIndexIterator::Make2D(GLint minMip, GLint maxMip)
{
    return ImageIndexIterator(
        TextureType::_2D, TextureTarget::_2D, TextureTarget::_2D, Range<GLint>(minMip, maxMip),
        Range<GLint>(ImageIndex::ENTIRE_LEVEL, ImageIndex::ENTIRE_LEVEL), nullptr);
}

ImageIndexIterator ImageIndexIterator::MakeRectangle(GLint minMip, GLint maxMip)
{
    return ImageIndexIterator(TextureType::Rectangle, TextureTarget::Rectangle,
                              TextureTarget::Rectangle, Range<GLint>(minMip, maxMip),
                              Range<GLint>(ImageIndex::ENTIRE_LEVEL, ImageIndex::ENTIRE_LEVEL),
                              nullptr);
}

ImageIndexIterator ImageIndexIterator::MakeCube(GLint minMip, GLint maxMip)
{
    return ImageIndexIterator(TextureType::CubeMap, kCubeMapTextureTargetMin,
                              kCubeMapTextureTargetMax, Range<GLint>(minMip, maxMip),
                              Range<GLint>(ImageIndex::ENTIRE_LEVEL, ImageIndex::ENTIRE_LEVEL),
                              nullptr);
}

ImageIndexIterator ImageIndexIterator::Make3D(GLint minMip, GLint maxMip,
                                              GLint minLayer, GLint maxLayer)
{
    return ImageIndexIterator(TextureType::_3D, TextureTarget::_3D, TextureTarget::_3D,
                              Range<GLint>(minMip, maxMip), Range<GLint>(minLayer, maxLayer),
                              nullptr);
}

ImageIndexIterator ImageIndexIterator::Make2DArray(GLint minMip, GLint maxMip,
                                                   const GLsizei *layerCounts)
{
    return ImageIndexIterator(TextureType::_2DArray, TextureTarget::_2DArray,
                              TextureTarget::_2DArray, Range<GLint>(minMip, maxMip),
                              Range<GLint>(0, IMPLEMENTATION_MAX_2D_ARRAY_TEXTURE_LAYERS),
                              layerCounts);
}

ImageIndexIterator ImageIndexIterator::Make2DMultisample()
{
    return ImageIndexIterator(TextureType::_2DMultisample, TextureTarget::_2DMultisample,
                              TextureTarget::_2DMultisample, Range<GLint>(0, 0),
                              Range<GLint>(ImageIndex::ENTIRE_LEVEL, ImageIndex::ENTIRE_LEVEL),
                              nullptr);
}

ImageIndexIterator ImageIndexIterator::MakeGeneric(TextureType type, GLint minMip, GLint maxMip)
{
    if (type == TextureType::CubeMap)
    {
        return MakeCube(minMip, maxMip);
    }

    TextureTarget target = NonCubeTextureTypeToTarget(type);
    return ImageIndexIterator(type, target, target, Range<GLint>(minMip, maxMip),
                              Range<GLint>(ImageIndex::ENTIRE_LEVEL, ImageIndex::ENTIRE_LEVEL),
                              nullptr);
}

ImageIndexIterator::ImageIndexIterator(TextureType type,
                                       angle::EnumIterator<TextureTarget> targetLow,
                                       angle::EnumIterator<TextureTarget> targetHigh,
                                       const Range<GLint> &mipRange,
                                       const Range<GLint> &layerRange,
                                       const GLsizei *layerCounts)
    : mTargetLow(targetLow),
      mTargetHigh(targetHigh),
      mMipRange(mipRange),
      mLayerRange(layerRange),
      mLayerCounts(layerCounts),
      mCurrentIndex(type, *targetLow, mipRange.low(), layerRange.low(), 1)
{}

GLint ImageIndexIterator::maxLayer() const
{
    if (mLayerCounts)
    {
        ASSERT(mCurrentIndex.hasLayer());
        return (mCurrentIndex.mipIndex < mMipRange.high()) ? mLayerCounts[mCurrentIndex.mipIndex]
                                                           : 0;
    }
    return mLayerRange.high();
}

ImageIndex ImageIndexIterator::next()
{
    ASSERT(hasNext());

    // Make a copy of the current index to return
    ImageIndex previousIndex = mCurrentIndex;

    // Iterate layers in the inner loop for now. We can add switchable
    // layer or mip iteration if we need it.

    angle::EnumIterator<TextureTarget> currentTarget = mCurrentIndex.target;
    if (currentTarget != mTargetHigh)
    {
        ++currentTarget;
        mCurrentIndex.target = *currentTarget;
    }
    else if (mCurrentIndex.hasLayer() && mCurrentIndex.layerIndex < maxLayer() - 1)
    {
        mCurrentIndex.target = *mTargetLow;
        mCurrentIndex.layerIndex++;
    }
    else if (mCurrentIndex.mipIndex < mMipRange.high() - 1)
    {
        mCurrentIndex.target     = *mTargetLow;
        mCurrentIndex.layerIndex = mLayerRange.low();
        mCurrentIndex.mipIndex++;
    }
    else
    {
        mCurrentIndex = ImageIndex::MakeInvalid();
    }

    return previousIndex;
}

ImageIndex ImageIndexIterator::current() const
{
    return mCurrentIndex;
}

bool ImageIndexIterator::hasNext() const
{
    return mCurrentIndex.valid();
}

}  // namespace gl
