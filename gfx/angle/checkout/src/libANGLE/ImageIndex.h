//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ImageIndex.h: A helper struct for indexing into an Image array

#ifndef LIBANGLE_IMAGE_INDEX_H_
#define LIBANGLE_IMAGE_INDEX_H_

#include "common/mathutil.h"
#include "libANGLE/PackedGLEnums.h"

#include "angle_gl.h"

namespace gl
{

class ImageIndexIterator;

struct ImageIndex
{
    TextureType type;
    TextureTarget target;

    GLint mipIndex;

    GLint layerIndex;
    GLint numLayers;

    ImageIndex(const ImageIndex &other);
    ImageIndex &operator=(const ImageIndex &other);

    bool hasLayer() const { return layerIndex != ENTIRE_LEVEL; }
    bool is3D() const;
    GLint cubeMapFaceIndex() const;
    bool valid() const;

    static ImageIndex Make2D(GLint mipIndex);
    static ImageIndex MakeRectangle(GLint mipIndex);
    static ImageIndex MakeCube(TextureTarget target, GLint mipIndex);
    static ImageIndex Make2DArray(GLint mipIndex, GLint layerIndex);
    static ImageIndex Make2DArrayRange(GLint mipIndex, GLint layerIndex, GLint numLayers);
    static ImageIndex Make3D(GLint mipIndex, GLint layerIndex = ENTIRE_LEVEL);
    static ImageIndex MakeGeneric(TextureTarget target, GLint mipIndex);
    static ImageIndex Make2DMultisample();

    static ImageIndex MakeInvalid();

    static const GLint ENTIRE_LEVEL = static_cast<GLint>(-1);

  private:
    friend class ImageIndexIterator;

    ImageIndex(TextureType typeIn,
               TextureTarget targetIn,
               GLint mipIndexIn,
               GLint layerIndexIn,
               GLint numLayersIn);
};

bool operator<(const ImageIndex &a, const ImageIndex &b);
bool operator==(const ImageIndex &a, const ImageIndex &b);
bool operator!=(const ImageIndex &a, const ImageIndex &b);

// To be used like this:
//
// ImageIndexIterator it = ...;
// while (it.hasNext())
// {
//     ImageIndex current = it.next();
// }
class ImageIndexIterator
{
  public:
    ImageIndexIterator(const ImageIndexIterator &other);

    static ImageIndexIterator Make2D(GLint minMip, GLint maxMip);
    static ImageIndexIterator MakeRectangle(GLint minMip, GLint maxMip);
    static ImageIndexIterator MakeCube(GLint minMip, GLint maxMip);
    static ImageIndexIterator Make3D(GLint minMip, GLint maxMip, GLint minLayer, GLint maxLayer);
    static ImageIndexIterator Make2DArray(GLint minMip, GLint maxMip, const GLsizei *layerCounts);
    static ImageIndexIterator Make2DMultisample();
    static ImageIndexIterator MakeGeneric(TextureType type, GLint minMip, GLint maxMip);

    ImageIndex next();
    ImageIndex current() const;
    bool hasNext() const;

  private:
    ImageIndexIterator(TextureType type,
                       angle::EnumIterator<TextureTarget> targetLow,
                       angle::EnumIterator<TextureTarget> targetHigh,
                       const Range<GLint> &mipRange,
                       const Range<GLint> &layerRange,
                       const GLsizei *layerCounts);

    GLint maxLayer() const;

    const angle::EnumIterator<TextureTarget> mTargetLow;
    const angle::EnumIterator<TextureTarget> mTargetHigh;
    const Range<GLint> mMipRange;
    const Range<GLint> mLayerRange;
    const GLsizei *const mLayerCounts;

    ImageIndex mCurrentIndex;
};

}

#endif // LIBANGLE_IMAGE_INDEX_H_
