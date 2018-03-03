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
    return type == GL_TEXTURE_3D || type == GL_TEXTURE_2D_ARRAY;
}

GLint ImageIndex::cubeMapFaceIndex() const
{
    ASSERT(type == GL_TEXTURE_CUBE_MAP);
    ASSERT(target >= gl::FirstCubeMapTextureTarget && target <= gl::LastCubeMapTextureTarget);
    return target - gl::FirstCubeMapTextureTarget;
}

bool ImageIndex::valid() const
{
    return type != GL_NONE;
}

ImageIndex ImageIndex::Make2D(GLint mipIndex)
{
    return ImageIndex(GL_TEXTURE_2D, GL_TEXTURE_2D, mipIndex, ENTIRE_LEVEL, 1);
}

ImageIndex ImageIndex::MakeRectangle(GLint mipIndex)
{
    return ImageIndex(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_RECTANGLE_ANGLE, mipIndex,
                      ENTIRE_LEVEL, 1);
}

ImageIndex ImageIndex::MakeCube(GLenum target, GLint mipIndex)
{
    ASSERT(gl::IsCubeMapTextureTarget(target));
    return ImageIndex(GL_TEXTURE_CUBE_MAP, target, mipIndex, ENTIRE_LEVEL, 1);
}

ImageIndex ImageIndex::Make2DArray(GLint mipIndex, GLint layerIndex)
{
    return ImageIndex(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_2D_ARRAY, mipIndex, layerIndex, 1);
}

ImageIndex ImageIndex::Make2DArrayRange(GLint mipIndex, GLint layerIndex, GLint numLayers)
{
    return ImageIndex(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_2D_ARRAY, mipIndex, layerIndex, numLayers);
}

ImageIndex ImageIndex::Make3D(GLint mipIndex, GLint layerIndex)
{
    return ImageIndex(GL_TEXTURE_3D, GL_TEXTURE_3D, mipIndex, layerIndex, 1);
}

ImageIndex ImageIndex::MakeGeneric(GLenum target, GLint mipIndex)
{
    GLenum textureType = IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target;
    return ImageIndex(textureType, target, mipIndex, ENTIRE_LEVEL, 1);
}

ImageIndex ImageIndex::Make2DMultisample()
{
    return ImageIndex(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_2D_MULTISAMPLE, 0, ENTIRE_LEVEL, 1);
}

ImageIndex ImageIndex::MakeInvalid()
{
    return ImageIndex(GL_NONE, GL_NONE, -1, -1, -1);
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

ImageIndex::ImageIndex(GLenum typeIn,
                       GLenum targetIn,
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
        GL_TEXTURE_2D, Range<GLenum>(GL_TEXTURE_2D, GL_TEXTURE_2D), Range<GLint>(minMip, maxMip),
        Range<GLint>(ImageIndex::ENTIRE_LEVEL, ImageIndex::ENTIRE_LEVEL), nullptr);
}

ImageIndexIterator ImageIndexIterator::MakeRectangle(GLint minMip, GLint maxMip)
{
    return ImageIndexIterator(GL_TEXTURE_RECTANGLE_ANGLE,
                              Range<GLenum>(GL_TEXTURE_RECTANGLE_ANGLE, GL_TEXTURE_RECTANGLE_ANGLE),
                              Range<GLint>(minMip, maxMip),
                              Range<GLint>(ImageIndex::ENTIRE_LEVEL, ImageIndex::ENTIRE_LEVEL),
                              nullptr);
}

ImageIndexIterator ImageIndexIterator::MakeCube(GLint minMip, GLint maxMip)
{
    return ImageIndexIterator(
        GL_TEXTURE_CUBE_MAP,
        Range<GLenum>(gl::FirstCubeMapTextureTarget, gl::LastCubeMapTextureTarget),
        Range<GLint>(minMip, maxMip),
        Range<GLint>(ImageIndex::ENTIRE_LEVEL, ImageIndex::ENTIRE_LEVEL), nullptr);
}

ImageIndexIterator ImageIndexIterator::Make3D(GLint minMip, GLint maxMip,
                                              GLint minLayer, GLint maxLayer)
{
    return ImageIndexIterator(GL_TEXTURE_3D, Range<GLenum>(GL_TEXTURE_3D, GL_TEXTURE_3D),
                              Range<GLint>(minMip, maxMip), Range<GLint>(minLayer, maxLayer),
                              nullptr);
}

ImageIndexIterator ImageIndexIterator::Make2DArray(GLint minMip, GLint maxMip,
                                                   const GLsizei *layerCounts)
{
    return ImageIndexIterator(
        GL_TEXTURE_2D_ARRAY, Range<GLenum>(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_2D_ARRAY),
        Range<GLint>(minMip, maxMip), Range<GLint>(0, IMPLEMENTATION_MAX_2D_ARRAY_TEXTURE_LAYERS),
        layerCounts);
}

ImageIndexIterator ImageIndexIterator::Make2DMultisample()
{
    return ImageIndexIterator(
        GL_TEXTURE_2D_MULTISAMPLE,
        Range<GLenum>(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_2D_MULTISAMPLE), Range<GLint>(0, 0),
        Range<GLint>(ImageIndex::ENTIRE_LEVEL, ImageIndex::ENTIRE_LEVEL), nullptr);
}

ImageIndexIterator::ImageIndexIterator(GLenum type,
                                       const Range<GLenum> &targetRange,
                                       const Range<GLint> &mipRange,
                                       const Range<GLint> &layerRange,
                                       const GLsizei *layerCounts)
    : mTargetRange(targetRange),
      mMipRange(mipRange),
      mLayerRange(layerRange),
      mLayerCounts(layerCounts),
      mCurrentIndex(type, targetRange.low(), mipRange.low(), layerRange.low(), 1)
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

    if (mCurrentIndex.target < mTargetRange.high())
    {
        mCurrentIndex.target++;
    }
    else if (mCurrentIndex.hasLayer() && mCurrentIndex.layerIndex < maxLayer() - 1)
    {
        mCurrentIndex.target = mTargetRange.low();
        mCurrentIndex.layerIndex++;
    }
    else if (mCurrentIndex.mipIndex < mMipRange.high() - 1)
    {
        mCurrentIndex.target     = mTargetRange.low();
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
