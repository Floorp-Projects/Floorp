//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureD3D.cpp: Implementations of the Texture interfaces shared betweeen the D3D backends.

#include "libGLESv2/renderer/d3d/TextureD3D.h"
#include "libGLESv2/renderer/d3d/TextureStorage.h"
#include "libGLESv2/renderer/d3d/ImageD3D.h"
#include "libGLESv2/Buffer.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/main.h"
#include "libGLESv2/formatutils.h"
#include "libGLESv2/renderer/BufferImpl.h"
#include "libGLESv2/renderer/RenderTarget.h"
#include "libGLESv2/renderer/Renderer.h"

#include "libEGL/Surface.h"

#include "common/mathutil.h"
#include "common/utilities.h"

namespace rx
{

bool IsRenderTargetUsage(GLenum usage)
{
    return (usage == GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
}

TextureD3D::TextureD3D(Renderer *renderer)
    : mRenderer(renderer),
      mUsage(GL_NONE),
      mDirtyImages(true),
      mImmutable(false),
      mTexStorage(NULL)
{
}

TextureD3D::~TextureD3D()
{
}

TextureD3D *TextureD3D::makeTextureD3D(TextureImpl *texture)
{
    ASSERT(HAS_DYNAMIC_TYPE(TextureD3D*, texture));
    return static_cast<TextureD3D*>(texture);
}

TextureStorage *TextureD3D::getNativeTexture()
{
    // ensure the underlying texture is created
    initializeStorage(false);

    if (mTexStorage)
    {
        updateStorage();
    }

    return mTexStorage;
}

GLint TextureD3D::getBaseLevelWidth() const
{
    const Image *baseImage = getBaseLevelImage();
    return (baseImage ? baseImage->getWidth() : 0);
}

GLint TextureD3D::getBaseLevelHeight() const
{
    const Image *baseImage = getBaseLevelImage();
    return (baseImage ? baseImage->getHeight() : 0);
}

GLint TextureD3D::getBaseLevelDepth() const
{
    const Image *baseImage = getBaseLevelImage();
    return (baseImage ? baseImage->getDepth() : 0);
}

// Note: "base level image" is loosely defined to be any image from the base level,
// where in the base of 2D array textures and cube maps there are several. Don't use
// the base level image for anything except querying texture format and size.
GLenum TextureD3D::getBaseLevelInternalFormat() const
{
    const Image *baseImage = getBaseLevelImage();
    return (baseImage ? baseImage->getInternalFormat() : GL_NONE);
}

gl::Error TextureD3D::setImage(const gl::PixelUnpackState &unpack, GLenum type, const void *pixels, const gl::ImageIndex &index)
{
    Image *image = getImage(index);

    // No-op
    if (image->getWidth() == 0 || image->getHeight() == 0 || image->getDepth() == 0)
    {
        return gl::Error(GL_NO_ERROR);
    }

    // We no longer need the "GLenum format" parameter to TexImage to determine what data format "pixels" contains.
    // From our image internal format we know how many channels to expect, and "type" gives the format of pixel's components.
    const uint8_t *pixelData = NULL;

    if (unpack.pixelBuffer.id() != 0)
    {
        // Do a CPU readback here, if we have an unpack buffer bound and the fast GPU path is not supported
        gl::Buffer *pixelBuffer = unpack.pixelBuffer.get();
        ptrdiff_t offset = reinterpret_cast<ptrdiff_t>(pixels);
        // TODO: setImage/subImage is the only place outside of renderer that asks for a buffers raw data.
        // This functionality should be moved into renderer and the getData method of BufferImpl removed.
        const void *bufferData = pixelBuffer->getImplementation()->getData();
        pixelData = static_cast<const uint8_t *>(bufferData) + offset;
    }
    else
    {
        pixelData = static_cast<const uint8_t *>(pixels);
    }

    if (pixelData != NULL)
    {
        gl::Error error(GL_NO_ERROR);

        gl::InternalFormat internalFormat = gl::GetInternalFormatInfo(image->getInternalFormat());

        // TODO(jmadill): Handle compressed internal formats
        if (mTexStorage && mRenderer->getWorkarounds().setDataFasterThanImageUpload && !internalFormat.compressed)
        {
            gl::Box sourceBox(0, 0, 0, image->getWidth(), image->getHeight(), image->getDepth());
            error = mTexStorage->setData(index, sourceBox, image->getInternalFormat(), type, unpack, pixelData);
        }
        else
        {
            error = image->loadData(0, 0, 0, image->getWidth(), image->getHeight(), image->getDepth(), unpack.alignment, type, pixelData);
        }

        if (error.isError())
        {
            return error;
        }

        mDirtyImages = true;
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureD3D::subImage(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                               GLenum format, GLenum type, const gl::PixelUnpackState &unpack, const void *pixels, const gl::ImageIndex &index)
{
    const uint8_t *pixelData = static_cast<const uint8_t *>(pixels);

    // CPU readback & copy where direct GPU copy is not supported
    if (unpack.pixelBuffer.id() != 0)
    {
        gl::Buffer *pixelBuffer = unpack.pixelBuffer.get();
        uintptr_t offset = reinterpret_cast<uintptr_t>(pixels);
        // TODO: setImage/subImage is the only place outside of renderer that asks for a buffers raw data.
        // This functionality should be moved into renderer and the getData method of BufferImpl removed.
        const void *bufferData = pixelBuffer->getImplementation()->getData();
        pixelData = static_cast<const uint8_t *>(bufferData)+offset;
    }

    if (pixelData != NULL)
    {
        Image *image = getImage(index);
        ASSERT(image);

        gl::InternalFormat internalFormat = gl::GetInternalFormatInfo(image->getInternalFormat());
        gl::Box region(xoffset, yoffset, zoffset, width, height, depth);

        // TODO(jmadill): Handle compressed internal formats
        if (mTexStorage && mRenderer->getWorkarounds().setDataFasterThanImageUpload && !internalFormat.compressed)
        {
            return mTexStorage->setData(index, region, image->getInternalFormat(),
                                        type, unpack, pixelData);
        }

        gl::Error error = image->loadData(xoffset, yoffset, zoffset, width, height, depth, unpack.alignment,
                                          type, pixelData);
        if (error.isError())
        {
            return error;
        }

        error = commitRegion(index, region);
        if (error.isError())
        {
            return error;
        }

        mDirtyImages = true;
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureD3D::setCompressedImage(GLsizei imageSize, const void *pixels, Image *image)
{
    if (pixels != NULL)
    {
        gl::Error error = image->loadCompressedData(0, 0, 0, image->getWidth(), image->getHeight(), image->getDepth(), pixels);
        if (error.isError())
        {
            return error;
        }

        mDirtyImages = true;
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureD3D::subImageCompressed(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                 GLenum format, GLsizei imageSize, const void *pixels, Image *image)
{
    if (pixels != NULL)
    {
        gl::Error error = image->loadCompressedData(xoffset, yoffset, zoffset, width, height, depth, pixels);
        if (error.isError())
        {
            return error;
        }

        mDirtyImages = true;
    }

    return gl::Error(GL_NO_ERROR);
}

bool TextureD3D::isFastUnpackable(const gl::PixelUnpackState &unpack, GLenum sizedInternalFormat)
{
    return unpack.pixelBuffer.id() != 0 && mRenderer->supportsFastCopyBufferToTexture(sizedInternalFormat);
}

gl::Error TextureD3D::fastUnpackPixels(const gl::PixelUnpackState &unpack, const void *pixels, const gl::Box &destArea,
                                       GLenum sizedInternalFormat, GLenum type, RenderTarget *destRenderTarget)
{
    // No-op
    if (destArea.width <= 0 && destArea.height <= 0 && destArea.depth <= 0)
    {
        return gl::Error(GL_NO_ERROR);
    }

    // In order to perform the fast copy through the shader, we must have the right format, and be able
    // to create a render target.
    ASSERT(mRenderer->supportsFastCopyBufferToTexture(sizedInternalFormat));

    uintptr_t offset = reinterpret_cast<uintptr_t>(pixels);

    gl::Error error = mRenderer->fastCopyBufferToTexture(unpack, offset, destRenderTarget, sizedInternalFormat, type, destArea);
    if (error.isError())
    {
        return error;
    }

    return gl::Error(GL_NO_ERROR);
}

GLint TextureD3D::creationLevels(GLsizei width, GLsizei height, GLsizei depth) const
{
    if ((gl::isPow2(width) && gl::isPow2(height) && gl::isPow2(depth)) || mRenderer->getRendererExtensions().textureNPOT)
    {
        // Maximum number of levels
        return gl::log2(std::max(std::max(width, height), depth)) + 1;
    }
    else
    {
        // OpenGL ES 2.0 without GL_OES_texture_npot does not permit NPOT mipmaps.
        return 1;
    }
}

int TextureD3D::mipLevels() const
{
    return gl::log2(std::max(std::max(getBaseLevelWidth(), getBaseLevelHeight()), getBaseLevelDepth())) + 1;
}

TextureStorage *TextureD3D::getStorage()
{
    return mTexStorage;
}

Image *TextureD3D::getBaseLevelImage() const
{
    return getImage(getImageIndex(0, 0));
}

void TextureD3D::generateMipmaps()
{
    GLint mipCount = mipLevels();

    if (mipCount == 1)
    {
        return; // no-op
    }

    // Set up proper mipmap chain in our Image array.
    initMipmapsImages();

    // We know that all layers have the same dimension, for the texture to be complete
    GLint layerCount = static_cast<GLint>(getLayerCount(0));

    // When making mipmaps with the setData workaround enabled, the texture storage has
    // the image data already. For non-render-target storage, we have to pull it out into
    // an image layer.
    if (mRenderer->getWorkarounds().setDataFasterThanImageUpload && mTexStorage)
    {
        if (!mTexStorage->isRenderTarget())
        {
            // Copy from the storage mip 0 to Image mip 0
            for (GLint layer = 0; layer < layerCount; ++layer)
            {
                gl::ImageIndex srcIndex = getImageIndex(0, layer);

                Image *image = getImage(srcIndex);
                gl::Rectangle area(0, 0, image->getWidth(), image->getHeight());
                image->copy(0, 0, 0, area, srcIndex, mTexStorage);
            }
        }
        else
        {
            updateStorage();
        }
    }

    bool renderableStorage = (mTexStorage && mTexStorage->isRenderTarget());

    for (GLint layer = 0; layer < layerCount; ++layer)
    {
        for (GLint mip = 1; mip < mipCount; ++mip)
        {
            ASSERT(getLayerCount(mip) == layerCount);

            gl::ImageIndex sourceIndex = getImageIndex(mip - 1, layer);
            gl::ImageIndex destIndex = getImageIndex(mip, layer);

            if (renderableStorage)
            {
                // GPU-side mipmapping
                mTexStorage->generateMipmap(sourceIndex, destIndex);
            }
            else
            {
                // CPU-side mipmapping
                mRenderer->generateMipmap(getImage(destIndex), getImage(sourceIndex));
            }
        }
    }
}

bool TextureD3D::isBaseImageZeroSize() const
{
    Image *baseImage = getBaseLevelImage();

    if (!baseImage || baseImage->getWidth() <= 0)
    {
        return true;
    }

    if (!gl::IsCubemapTextureTarget(baseImage->getTarget()) && baseImage->getHeight() <= 0)
    {
        return true;
    }

    if (baseImage->getTarget() == GL_TEXTURE_3D && baseImage->getDepth() <= 0)
    {
        return true;
    }

    if (baseImage->getTarget() == GL_TEXTURE_2D_ARRAY && getLayerCount(0) <= 0)
    {
        return true;
    }

    return false;
}

bool TextureD3D::ensureRenderTarget()
{
    initializeStorage(true);

    if (!isBaseImageZeroSize())
    {
        ASSERT(mTexStorage);
        if (!mTexStorage->isRenderTarget())
        {
            TextureStorage *newRenderTargetStorage = createCompleteStorage(true);

            if (mTexStorage->copyToStorage(newRenderTargetStorage).isError())
            {
                delete newRenderTargetStorage;
                return gl::error(GL_OUT_OF_MEMORY, false);
            }

            setCompleteTexStorage(newRenderTargetStorage);
        }
    }

    return (mTexStorage && mTexStorage->isRenderTarget());
}

TextureD3D_2D::TextureD3D_2D(Renderer *renderer)
    : TextureD3D(renderer)
{
    for (int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; ++i)
    {
        mImageArray[i] = ImageD3D::makeImageD3D(renderer->createImage());
    }
}

TextureD3D_2D::~TextureD3D_2D()
{
    // Delete the Images before the TextureStorage.
    // Images might be relying on the TextureStorage for some of their data.
    // If TextureStorage is deleted before the Images, then their data will be wastefully copied back from the GPU before we delete the Images.
    for (int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; ++i)
    {
        delete mImageArray[i];
    }

    SafeDelete(mTexStorage);
}

Image *TextureD3D_2D::getImage(int level, int layer) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(layer == 0);
    return mImageArray[level];
}

Image *TextureD3D_2D::getImage(const gl::ImageIndex &index) const
{
    ASSERT(index.mipIndex < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(!index.hasLayer());
    ASSERT(index.type == GL_TEXTURE_2D);
    return mImageArray[index.mipIndex];
}

GLsizei TextureD3D_2D::getLayerCount(int level) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    return 1;
}

GLsizei TextureD3D_2D::getWidth(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getWidth();
    else
        return 0;
}

GLsizei TextureD3D_2D::getHeight(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getHeight();
    else
        return 0;
}

GLenum TextureD3D_2D::getInternalFormat(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getInternalFormat();
    else
        return GL_NONE;
}

GLenum TextureD3D_2D::getActualFormat(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getActualFormat();
    else
        return GL_NONE;
}

bool TextureD3D_2D::isDepth(GLint level) const
{
    return gl::GetInternalFormatInfo(getInternalFormat(level)).depthBits > 0;
}

gl::Error TextureD3D_2D::setImage(GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth,
                                  GLenum internalFormat, GLenum format, GLenum type, const gl::PixelUnpackState &unpack,
                                  const void *pixels)
{
    ASSERT(target == GL_TEXTURE_2D && depth == 1);

    GLenum sizedInternalFormat = gl::GetSizedInternalFormat(internalFormat, type);

    bool fastUnpacked = false;

    redefineImage(level, sizedInternalFormat, width, height);

    gl::ImageIndex index = gl::ImageIndex::Make2D(level);

    // Attempt a fast gpu copy of the pixel data to the surface
    if (isFastUnpackable(unpack, sizedInternalFormat) && isLevelComplete(level))
    {
        // Will try to create RT storage if it does not exist
        RenderTarget *destRenderTarget = getRenderTarget(index);
        gl::Box destArea(0, 0, 0, getWidth(level), getHeight(level), 1);

        if (destRenderTarget)
        {
            gl::Error error = fastUnpackPixels(unpack, pixels, destArea, sizedInternalFormat, type, destRenderTarget);
            if (error.isError())
            {
                return error;
            }

            // Ensure we don't overwrite our newly initialized data
            mImageArray[level]->markClean();

            fastUnpacked = true;
        }
    }

    if (!fastUnpacked)
    {
        gl::Error error = TextureD3D::setImage(unpack, type, pixels, index);
        if (error.isError())
        {
            return error;
        }
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureD3D_2D::setCompressedImage(GLenum target, GLint level, GLenum format,
                                            GLsizei width, GLsizei height, GLsizei depth,
                                            GLsizei imageSize, const void *pixels)
{
    ASSERT(target == GL_TEXTURE_2D && depth == 1);

    // compressed formats don't have separate sized internal formats-- we can just use the compressed format directly
    redefineImage(level, format, width, height);

    return TextureD3D::setCompressedImage(imageSize, pixels, mImageArray[level]);
}

gl::Error TextureD3D_2D::subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                  GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                  const gl::PixelUnpackState &unpack, const void *pixels)
{
    ASSERT(target == GL_TEXTURE_2D && depth == 1 && zoffset == 0);

    bool fastUnpacked = false;

    gl::ImageIndex index = gl::ImageIndex::Make2D(level);
    gl::Box destArea(xoffset, yoffset, 0, width, height, 1);
    if (isFastUnpackable(unpack, getInternalFormat(level)) && isLevelComplete(level))
    {
        RenderTarget *renderTarget = getRenderTarget(index);

        if (renderTarget)
        {
            gl::Error error = fastUnpackPixels(unpack, pixels, destArea, getInternalFormat(level), type, renderTarget);
            if (error.isError())
            {
                return error;
            }

            // Ensure we don't overwrite our newly initialized data
            mImageArray[level]->markClean();

            fastUnpacked = true;
        }
    }

    if (!fastUnpacked)
    {
        return TextureD3D::subImage(xoffset, yoffset, 0, width, height, 1, format, type,
                                    unpack, pixels, index);
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureD3D_2D::subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                            GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                                            GLsizei imageSize, const void *pixels)
{
    ASSERT(target == GL_TEXTURE_2D && depth == 1 && zoffset == 0);

    gl::Error error = TextureD3D::subImageCompressed(xoffset, yoffset, 0, width, height, 1, format, imageSize, pixels, mImageArray[level]);
    if (error.isError())
    {
        return error;
    }

    gl::ImageIndex index = gl::ImageIndex::Make2D(level);
    gl::Box region(xoffset, yoffset, 0, width, height, 1);
    return commitRegion(index, region);
}

void TextureD3D_2D::copyImage(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source)
{
    ASSERT(target == GL_TEXTURE_2D);

    GLenum sizedInternalFormat = gl::GetSizedInternalFormat(format, GL_UNSIGNED_BYTE);
    redefineImage(level, sizedInternalFormat, width, height);

    gl::Rectangle sourceRect(x, y, width, height);

    if (!mImageArray[level]->isRenderableFormat())
    {
        mImageArray[level]->copy(0, 0, 0, sourceRect, source);
        mDirtyImages = true;
    }
    else
    {
        ensureRenderTarget();
        mImageArray[level]->markClean();

        if (width != 0 && height != 0 && isValidLevel(level))
        {
            mRenderer->copyImage2D(source, sourceRect, format, 0, 0, mTexStorage, level);
        }
    }
}

void TextureD3D_2D::copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source)
{
    ASSERT(target == GL_TEXTURE_2D && zoffset == 0);

    // can only make our texture storage to a render target if level 0 is defined (with a width & height) and
    // the current level we're copying to is defined (with appropriate format, width & height)
    bool canCreateRenderTarget = isLevelComplete(level) && isLevelComplete(0);

    gl::Rectangle sourceRect(x, y, width, height);

    if (!mImageArray[level]->isRenderableFormat() || (!mTexStorage && !canCreateRenderTarget))
    {
        mImageArray[level]->copy(xoffset, yoffset, 0, sourceRect, source);
        mDirtyImages = true;
    }
    else
    {
        ensureRenderTarget();

        if (isValidLevel(level))
        {
            updateStorageLevel(level);

            mRenderer->copyImage2D(source, sourceRect,
                                   gl::GetInternalFormatInfo(getBaseLevelInternalFormat()).format,
                                   xoffset, yoffset, mTexStorage, level);
        }
    }
}

void TextureD3D_2D::storage(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    ASSERT(target == GL_TEXTURE_2D && depth == 1);

    for (int level = 0; level < levels; level++)
    {
        GLsizei levelWidth = std::max(1, width >> level);
        GLsizei levelHeight = std::max(1, height >> level);
        mImageArray[level]->redefine(mRenderer, GL_TEXTURE_2D, internalformat, levelWidth, levelHeight, 1, true);
    }

    for (int level = levels; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        mImageArray[level]->redefine(mRenderer, GL_TEXTURE_2D, GL_NONE, 0, 0, 0, true);
    }

    mImmutable = true;

    bool renderTarget = IsRenderTargetUsage(mUsage);
    TextureStorage *storage = mRenderer->createTextureStorage2D(internalformat, renderTarget, width, height, levels);
    setCompleteTexStorage(storage);
}

void TextureD3D_2D::bindTexImage(egl::Surface *surface)
{
    GLenum internalformat = surface->getFormat();

    mImageArray[0]->redefine(mRenderer, GL_TEXTURE_2D, internalformat, surface->getWidth(), surface->getHeight(), 1, true);

    if (mTexStorage)
    {
        SafeDelete(mTexStorage);
    }

    mTexStorage = mRenderer->createTextureStorage2D(surface->getSwapChain());

    mDirtyImages = true;
}

void TextureD3D_2D::releaseTexImage()
{
    if (mTexStorage)
    {
        SafeDelete(mTexStorage);
    }

    for (int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        mImageArray[i]->redefine(mRenderer, GL_TEXTURE_2D, GL_NONE, 0, 0, 0, true);
    }
}

void TextureD3D_2D::initMipmapsImages()
{
    // Purge array levels 1 through q and reset them to represent the generated mipmap levels.
    int levelCount = mipLevels();
    for (int level = 1; level < levelCount; level++)
    {
        redefineImage(level, getBaseLevelInternalFormat(),
                      std::max(getBaseLevelWidth() >> level, 1),
                      std::max(getBaseLevelHeight() >> level, 1));
    }
}

unsigned int TextureD3D_2D::getRenderTargetSerial(const gl::ImageIndex &index)
{
    ASSERT(!index.hasLayer());
    return (ensureRenderTarget() ? mTexStorage->getRenderTargetSerial(index) : 0);
}

RenderTarget *TextureD3D_2D::getRenderTarget(const gl::ImageIndex &index)
{
    ASSERT(!index.hasLayer());

    // ensure the underlying texture is created
    if (!ensureRenderTarget())
    {
        return NULL;
    }

    updateStorageLevel(index.mipIndex);
    return mTexStorage->getRenderTarget(index);
}

bool TextureD3D_2D::isValidLevel(int level) const
{
    return (mTexStorage ? (level >= 0 && level < mTexStorage->getLevelCount()) : false);
}

bool TextureD3D_2D::isLevelComplete(int level) const
{
    if (isImmutable())
    {
        return true;
    }

    const Image *baseImage = getBaseLevelImage();

    GLsizei width = baseImage->getWidth();
    GLsizei height = baseImage->getHeight();

    if (width <= 0 || height <= 0)
    {
        return false;
    }

    // The base image level is complete if the width and height are positive
    if (level == 0)
    {
        return true;
    }

    ASSERT(level >= 1 && level <= (int)ArraySize(mImageArray) && mImageArray[level] != NULL);
    ImageD3D *image = mImageArray[level];

    if (image->getInternalFormat() != baseImage->getInternalFormat())
    {
        return false;
    }

    if (image->getWidth() != std::max(1, width >> level))
    {
        return false;
    }

    if (image->getHeight() != std::max(1, height >> level))
    {
        return false;
    }

    return true;
}

// Constructs a native texture resource from the texture images
void TextureD3D_2D::initializeStorage(bool renderTarget)
{
    // Only initialize the first time this texture is used as a render target or shader resource
    if (mTexStorage)
    {
        return;
    }

    // do not attempt to create storage for nonexistant data
    if (!isLevelComplete(0))
    {
        return;
    }

    bool createRenderTarget = (renderTarget || IsRenderTargetUsage(mUsage));

    setCompleteTexStorage(createCompleteStorage(createRenderTarget));
    ASSERT(mTexStorage);

    // flush image data to the storage
    updateStorage();
}

TextureStorage *TextureD3D_2D::createCompleteStorage(bool renderTarget) const
{
    GLsizei width = getBaseLevelWidth();
    GLsizei height = getBaseLevelHeight();
    GLenum internalFormat = getBaseLevelInternalFormat();

    ASSERT(width > 0 && height > 0);

    // use existing storage level count, when previously specified by TexStorage*D
    GLint levels = (mTexStorage ? mTexStorage->getLevelCount() : creationLevels(width, height, 1));

    return mRenderer->createTextureStorage2D(internalFormat, renderTarget, width, height, levels);
}

void TextureD3D_2D::setCompleteTexStorage(TextureStorage *newCompleteTexStorage)
{
    SafeDelete(mTexStorage);
    mTexStorage = newCompleteTexStorage;

    if (mTexStorage && mTexStorage->isManaged())
    {
        for (int level = 0; level < mTexStorage->getLevelCount(); level++)
        {
            mImageArray[level]->setManagedSurface2D(mTexStorage, level);
        }
    }

    mDirtyImages = true;
}

void TextureD3D_2D::updateStorage()
{
    ASSERT(mTexStorage != NULL);
    GLint storageLevels = mTexStorage->getLevelCount();
    for (int level = 0; level < storageLevels; level++)
    {
        if (mImageArray[level]->isDirty() && isLevelComplete(level))
        {
            updateStorageLevel(level);
        }
    }
}

void TextureD3D_2D::updateStorageLevel(int level)
{
    ASSERT(level <= (int)ArraySize(mImageArray) && mImageArray[level] != NULL);
    ASSERT(isLevelComplete(level));

    if (mImageArray[level]->isDirty())
    {
        gl::ImageIndex index = gl::ImageIndex::Make2D(level);
        gl::Box region(0, 0, 0, getWidth(level), getHeight(level), 1);
        commitRegion(index, region);
    }
}

void TextureD3D_2D::redefineImage(GLint level, GLenum internalformat, GLsizei width, GLsizei height)
{
    // If there currently is a corresponding storage texture image, it has these parameters
    const int storageWidth = std::max(1, getBaseLevelWidth() >> level);
    const int storageHeight = std::max(1, getBaseLevelHeight() >> level);
    const GLenum storageFormat = getBaseLevelInternalFormat();

    mImageArray[level]->redefine(mRenderer, GL_TEXTURE_2D, internalformat, width, height, 1, false);

    if (mTexStorage)
    {
        const int storageLevels = mTexStorage->getLevelCount();

        if ((level >= storageLevels && storageLevels != 0) ||
            width != storageWidth ||
            height != storageHeight ||
            internalformat != storageFormat)   // Discard mismatched storage
        {
            for (int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
            {
                mImageArray[i]->markDirty();
            }

            SafeDelete(mTexStorage);
            mDirtyImages = true;
        }
    }
}

gl::Error TextureD3D_2D::commitRegion(const gl::ImageIndex &index, const gl::Box &region)
{
    ASSERT(!index.hasLayer());
    GLint level = index.mipIndex;

    if (isValidLevel(level))
    {
        ImageD3D *image = mImageArray[level];
        gl::Error error = image->copyToStorage2D(mTexStorage, index, region);
        if (error.isError())
        {
            return error;
        }

        image->markClean();
    }

    return gl::Error(GL_NO_ERROR);
}

gl::ImageIndexIterator TextureD3D_2D::imageIterator() const
{
    return gl::ImageIndexIterator::Make2D(0, mTexStorage->getLevelCount());
}

gl::ImageIndex TextureD3D_2D::getImageIndex(GLint mip, GLint /*layer*/) const
{
    // "layer" does not apply to 2D Textures.
    return gl::ImageIndex::Make2D(mip);
}

TextureD3D_Cube::TextureD3D_Cube(Renderer *renderer)
    : TextureD3D(renderer)
{
    for (int i = 0; i < 6; i++)
    {
        for (int j = 0; j < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; ++j)
        {
            mImageArray[i][j] = ImageD3D::makeImageD3D(renderer->createImage());
        }
    }
}

TextureD3D_Cube::~TextureD3D_Cube()
{
    // Delete the Images before the TextureStorage.
    // Images might be relying on the TextureStorage for some of their data.
    // If TextureStorage is deleted before the Images, then their data will be wastefully copied back from the GPU before we delete the Images.
    for (int i = 0; i < 6; i++)
    {
        for (int j = 0; j < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; ++j)
        {
            SafeDelete(mImageArray[i][j]);
        }
    }

    SafeDelete(mTexStorage);
}

Image *TextureD3D_Cube::getImage(int level, int layer) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(layer < 6);
    return mImageArray[layer][level];
}

Image *TextureD3D_Cube::getImage(const gl::ImageIndex &index) const
{
    ASSERT(index.mipIndex < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(index.layerIndex < 6);
    return mImageArray[index.layerIndex][index.mipIndex];
}

GLsizei TextureD3D_Cube::getLayerCount(int level) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    return 6;
}

GLenum TextureD3D_Cube::getInternalFormat(GLint level, GLint layer) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[layer][level]->getInternalFormat();
    else
        return GL_NONE;
}

bool TextureD3D_Cube::isDepth(GLint level, GLint layer) const
{
    return gl::GetInternalFormatInfo(getInternalFormat(level, layer)).depthBits > 0;
}

gl::Error TextureD3D_Cube::setImage(GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth,
                                    GLenum internalFormat, GLenum format, GLenum type, const gl::PixelUnpackState &unpack,
                                    const void *pixels)
{
    ASSERT(depth == 1);

    GLenum sizedInternalFormat = gl::GetSizedInternalFormat(internalFormat, type);
    gl::ImageIndex index = gl::ImageIndex::MakeCube(target, level);

    redefineImage(index.layerIndex, level, sizedInternalFormat, width, height);

    return TextureD3D::setImage(unpack, type, pixels, index);
}

gl::Error TextureD3D_Cube::setCompressedImage(GLenum target, GLint level, GLenum format,
                                              GLsizei width, GLsizei height, GLsizei depth,
                                              GLsizei imageSize, const void *pixels)
{
    ASSERT(depth == 1);

    // compressed formats don't have separate sized internal formats-- we can just use the compressed format directly
    int faceIndex = gl::TextureCubeMap::targetToLayerIndex(target);

    redefineImage(faceIndex, level, format, width, height);

    return TextureD3D::setCompressedImage(imageSize, pixels, mImageArray[faceIndex][level]);
}

gl::Error TextureD3D_Cube::subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                    GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                    const gl::PixelUnpackState &unpack, const void *pixels)
{
    ASSERT(depth == 1 && zoffset == 0);
    gl::ImageIndex index = gl::ImageIndex::MakeCube(target, level);
    return TextureD3D::subImage(xoffset, yoffset, 0, width, height, 1, format, type, unpack, pixels, index);
}

gl::Error TextureD3D_Cube::subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                              GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                                              GLsizei imageSize, const void *pixels)
{
    ASSERT(depth == 1 && zoffset == 0);

    gl::ImageIndex index = gl::ImageIndex::MakeCube(target, level);

    gl::Error error = TextureD3D::subImageCompressed(xoffset, yoffset, 0, width, height, 1, format, imageSize, pixels, mImageArray[index.layerIndex][level]);
    if (error.isError())
    {
        return error;
    }

    gl::Box region(xoffset, yoffset, 0, width, height, 1);
    return commitRegion(index, region);
}

void TextureD3D_Cube::copyImage(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source)
{
    int faceIndex = gl::TextureCubeMap::targetToLayerIndex(target);
    GLenum sizedInternalFormat = gl::GetSizedInternalFormat(format, GL_UNSIGNED_BYTE);

    redefineImage(faceIndex, level, sizedInternalFormat, width, height);

    gl::Rectangle sourceRect(x, y, width, height);

    if (!mImageArray[faceIndex][level]->isRenderableFormat())
    {
        mImageArray[faceIndex][level]->copy(0, 0, 0, sourceRect, source);
        mDirtyImages = true;
    }
    else
    {
        ensureRenderTarget();
        mImageArray[faceIndex][level]->markClean();

        ASSERT(width == height);

        if (width > 0 && isValidFaceLevel(faceIndex, level))
        {
            mRenderer->copyImageCube(source, sourceRect, format, 0, 0, mTexStorage, target, level);
        }
    }
}

void TextureD3D_Cube::copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source)
{
    int faceIndex = gl::TextureCubeMap::targetToLayerIndex(target);

    // We can only make our texture storage to a render target if the level we're copying *to* is complete
    // and the base level is cube-complete. The base level must be cube complete (common case) because we cannot
    // rely on the "getBaseLevel*" methods reliably otherwise.
    bool canCreateRenderTarget = isFaceLevelComplete(faceIndex, level) && isCubeComplete();

    gl::Rectangle sourceRect(x, y, width, height);

    if (!mImageArray[faceIndex][level]->isRenderableFormat() || (!mTexStorage && !canCreateRenderTarget))
    {
        mImageArray[faceIndex][level]->copy(0, 0, 0, sourceRect, source);
        mDirtyImages = true;
    }
    else
    {
        ensureRenderTarget();

        if (isValidFaceLevel(faceIndex, level))
        {
            updateStorageFaceLevel(faceIndex, level);

            mRenderer->copyImageCube(source, sourceRect, gl::GetInternalFormatInfo(getBaseLevelInternalFormat()).format,
                                     xoffset, yoffset, mTexStorage, target, level);
        }
    }
}

void TextureD3D_Cube::storage(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    ASSERT(width == height);
    ASSERT(depth == 1);

    for (int level = 0; level < levels; level++)
    {
        GLsizei mipSize = std::max(1, width >> level);
        for (int faceIndex = 0; faceIndex < 6; faceIndex++)
        {
            mImageArray[faceIndex][level]->redefine(mRenderer, GL_TEXTURE_CUBE_MAP, internalformat, mipSize, mipSize, 1, true);
        }
    }

    for (int level = levels; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        for (int faceIndex = 0; faceIndex < 6; faceIndex++)
        {
            mImageArray[faceIndex][level]->redefine(mRenderer, GL_TEXTURE_CUBE_MAP, GL_NONE, 0, 0, 0, true);
        }
    }

    mImmutable = true;

    bool renderTarget = IsRenderTargetUsage(mUsage);
    TextureStorage *storage = mRenderer->createTextureStorageCube(internalformat, renderTarget, width, levels);
    setCompleteTexStorage(storage);
}

// Tests for cube texture completeness. [OpenGL ES 2.0.24] section 3.7.10 page 81.
bool TextureD3D_Cube::isCubeComplete() const
{
    int    baseWidth  = getBaseLevelWidth();
    int    baseHeight = getBaseLevelHeight();
    GLenum baseFormat = getBaseLevelInternalFormat();

    if (baseWidth <= 0 || baseWidth != baseHeight)
    {
        return false;
    }

    for (int faceIndex = 1; faceIndex < 6; faceIndex++)
    {
        const ImageD3D &faceBaseImage = *mImageArray[faceIndex][0];

        if (faceBaseImage.getWidth()          != baseWidth  ||
            faceBaseImage.getHeight()         != baseHeight ||
            faceBaseImage.getInternalFormat() != baseFormat )
        {
            return false;
        }
    }

    return true;
}

void TextureD3D_Cube::bindTexImage(egl::Surface *surface)
{
    UNREACHABLE();
}

void TextureD3D_Cube::releaseTexImage()
{
    UNREACHABLE();
}


void TextureD3D_Cube::initMipmapsImages()
{
    // Purge array levels 1 through q and reset them to represent the generated mipmap levels.
    int levelCount = mipLevels();
    for (int faceIndex = 0; faceIndex < 6; faceIndex++)
    {
        for (int level = 1; level < levelCount; level++)
        {
            int faceLevelSize = (std::max(mImageArray[faceIndex][0]->getWidth() >> level, 1));
            redefineImage(faceIndex, level, mImageArray[faceIndex][0]->getInternalFormat(), faceLevelSize, faceLevelSize);
        }
    }
}

unsigned int TextureD3D_Cube::getRenderTargetSerial(const gl::ImageIndex &index)
{
    return (ensureRenderTarget() ? mTexStorage->getRenderTargetSerial(index) : 0);
}

RenderTarget *TextureD3D_Cube::getRenderTarget(const gl::ImageIndex &index)
{
    ASSERT(gl::IsCubemapTextureTarget(index.type));

    // ensure the underlying texture is created
    if (!ensureRenderTarget())
    {
        return NULL;
    }

    updateStorageFaceLevel(index.layerIndex, index.mipIndex);
    return mTexStorage->getRenderTarget(index);
}

void TextureD3D_Cube::initializeStorage(bool renderTarget)
{
    // Only initialize the first time this texture is used as a render target or shader resource
    if (mTexStorage)
    {
        return;
    }

    // do not attempt to create storage for nonexistant data
    if (!isFaceLevelComplete(0, 0))
    {
        return;
    }

    bool createRenderTarget = (renderTarget || IsRenderTargetUsage(mUsage));

    setCompleteTexStorage(createCompleteStorage(createRenderTarget));
    ASSERT(mTexStorage);

    // flush image data to the storage
    updateStorage();
}

TextureStorage *TextureD3D_Cube::createCompleteStorage(bool renderTarget) const
{
    GLsizei size = getBaseLevelWidth();

    ASSERT(size > 0);

    // use existing storage level count, when previously specified by TexStorage*D
    GLint levels = (mTexStorage ? mTexStorage->getLevelCount() : creationLevels(size, size, 1));

    return mRenderer->createTextureStorageCube(getBaseLevelInternalFormat(), renderTarget, size, levels);
}

void TextureD3D_Cube::setCompleteTexStorage(TextureStorage *newCompleteTexStorage)
{
    SafeDelete(mTexStorage);
    mTexStorage = newCompleteTexStorage;

    if (mTexStorage && mTexStorage->isManaged())
    {
        for (int faceIndex = 0; faceIndex < 6; faceIndex++)
        {
            for (int level = 0; level < mTexStorage->getLevelCount(); level++)
            {
                mImageArray[faceIndex][level]->setManagedSurfaceCube(mTexStorage, faceIndex, level);
            }
        }
    }

    mDirtyImages = true;
}

void TextureD3D_Cube::updateStorage()
{
    ASSERT(mTexStorage != NULL);
    GLint storageLevels = mTexStorage->getLevelCount();
    for (int face = 0; face < 6; face++)
    {
        for (int level = 0; level < storageLevels; level++)
        {
            if (mImageArray[face][level]->isDirty() && isFaceLevelComplete(face, level))
            {
                updateStorageFaceLevel(face, level);
            }
        }
    }
}

bool TextureD3D_Cube::isValidFaceLevel(int faceIndex, int level) const
{
    return (mTexStorage ? (level >= 0 && level < mTexStorage->getLevelCount()) : 0);
}

bool TextureD3D_Cube::isFaceLevelComplete(int faceIndex, int level) const
{
    ASSERT(level >= 0 && faceIndex < 6 && level < (int)ArraySize(mImageArray[faceIndex]) && mImageArray[faceIndex][level] != NULL);

    if (isImmutable())
    {
        return true;
    }

    int baseSize = getBaseLevelWidth();

    if (baseSize <= 0)
    {
        return false;
    }

    // "isCubeComplete" checks for base level completeness and we must call that
    // to determine if any face at level 0 is complete. We omit that check here
    // to avoid re-checking cube-completeness for every face at level 0.
    if (level == 0)
    {
        return true;
    }

    // Check that non-zero levels are consistent with the base level.
    const ImageD3D *faceLevelImage = mImageArray[faceIndex][level];

    if (faceLevelImage->getInternalFormat() != getBaseLevelInternalFormat())
    {
        return false;
    }

    if (faceLevelImage->getWidth() != std::max(1, baseSize >> level))
    {
        return false;
    }

    return true;
}

void TextureD3D_Cube::updateStorageFaceLevel(int faceIndex, int level)
{
    ASSERT(level >= 0 && faceIndex < 6 && level < (int)ArraySize(mImageArray[faceIndex]) && mImageArray[faceIndex][level] != NULL);
    ImageD3D *image = mImageArray[faceIndex][level];

    if (image->isDirty())
    {
        GLenum faceTarget = gl::TextureCubeMap::layerIndexToTarget(faceIndex);
        gl::ImageIndex index = gl::ImageIndex::MakeCube(faceTarget, level);
        gl::Box region(0, 0, 0, image->getWidth(), image->getHeight(), 1);
        commitRegion(index, region);
    }
}

void TextureD3D_Cube::redefineImage(int faceIndex, GLint level, GLenum internalformat, GLsizei width, GLsizei height)
{
    // If there currently is a corresponding storage texture image, it has these parameters
    const int storageWidth = std::max(1, getBaseLevelWidth() >> level);
    const int storageHeight = std::max(1, getBaseLevelHeight() >> level);
    const GLenum storageFormat = getBaseLevelInternalFormat();

    mImageArray[faceIndex][level]->redefine(mRenderer, GL_TEXTURE_CUBE_MAP, internalformat, width, height, 1, false);

    if (mTexStorage)
    {
        const int storageLevels = mTexStorage->getLevelCount();

        if ((level >= storageLevels && storageLevels != 0) ||
            width != storageWidth ||
            height != storageHeight ||
            internalformat != storageFormat)   // Discard mismatched storage
        {
            for (int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
            {
                for (int faceIndex = 0; faceIndex < 6; faceIndex++)
                {
                    mImageArray[faceIndex][level]->markDirty();
                }
            }

            SafeDelete(mTexStorage);

            mDirtyImages = true;
        }
    }
}

gl::Error TextureD3D_Cube::commitRegion(const gl::ImageIndex &index, const gl::Box &region)
{
    ASSERT(index.hasLayer());

    GLint level = index.mipIndex;
    int faceIndex = static_cast<int>(index.layerIndex);

    if (isValidFaceLevel(faceIndex, level))
    {
        ImageD3D *image = mImageArray[faceIndex][level];
        gl::Error error = image->copyToStorageCube(mTexStorage, index, region);
        if (error.isError())
        {
            return error;
        }

        image->markClean();
    }

    return gl::Error(GL_NO_ERROR);
}

gl::ImageIndexIterator TextureD3D_Cube::imageIterator() const
{
    return gl::ImageIndexIterator::MakeCube(0, mTexStorage->getLevelCount());
}

gl::ImageIndex TextureD3D_Cube::getImageIndex(GLint mip, GLint layer) const
{
    // The "layer" of the image index corresponds to the cube face
    return gl::ImageIndex::MakeCube(gl::TextureCubeMap::layerIndexToTarget(layer), mip);
}

TextureD3D_3D::TextureD3D_3D(Renderer *renderer)
    : TextureD3D(renderer)
{
    for (int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; ++i)
    {
        mImageArray[i] = ImageD3D::makeImageD3D(renderer->createImage());
    }
}

TextureD3D_3D::~TextureD3D_3D()
{
    // Delete the Images before the TextureStorage.
    // Images might be relying on the TextureStorage for some of their data.
    // If TextureStorage is deleted before the Images, then their data will be wastefully copied back from the GPU before we delete the Images.
    for (int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; ++i)
    {
        delete mImageArray[i];
    }

    SafeDelete(mTexStorage);
}

Image *TextureD3D_3D::getImage(int level, int layer) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(layer == 0);
    return mImageArray[level];
}

Image *TextureD3D_3D::getImage(const gl::ImageIndex &index) const
{
    ASSERT(index.mipIndex < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(!index.hasLayer());
    ASSERT(index.type == GL_TEXTURE_3D);
    return mImageArray[index.mipIndex];
}

GLsizei TextureD3D_3D::getLayerCount(int level) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    return 1;
}

GLsizei TextureD3D_3D::getWidth(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getWidth();
    else
        return 0;
}

GLsizei TextureD3D_3D::getHeight(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getHeight();
    else
        return 0;
}

GLsizei TextureD3D_3D::getDepth(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getDepth();
    else
        return 0;
}

GLenum TextureD3D_3D::getInternalFormat(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getInternalFormat();
    else
        return GL_NONE;
}

bool TextureD3D_3D::isDepth(GLint level) const
{
    return gl::GetInternalFormatInfo(getInternalFormat(level)).depthBits > 0;
}

gl::Error TextureD3D_3D::setImage(GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth,
                                  GLenum internalFormat, GLenum format, GLenum type, const gl::PixelUnpackState &unpack,
                                  const void *pixels)
{
    ASSERT(target == GL_TEXTURE_3D);
    GLenum sizedInternalFormat = gl::GetSizedInternalFormat(internalFormat, type);

    redefineImage(level, sizedInternalFormat, width, height, depth);

    bool fastUnpacked = false;

    gl::ImageIndex index = gl::ImageIndex::Make3D(level);

    // Attempt a fast gpu copy of the pixel data to the surface if the app bound an unpack buffer
    if (isFastUnpackable(unpack, sizedInternalFormat))
    {
        // Will try to create RT storage if it does not exist
        RenderTarget *destRenderTarget = getRenderTarget(index);
        gl::Box destArea(0, 0, 0, getWidth(level), getHeight(level), getDepth(level));

        if (destRenderTarget)
        {
            gl::Error error = fastUnpackPixels(unpack, pixels, destArea, sizedInternalFormat, type, destRenderTarget);
            if (error.isError())
            {
                return error;
            }

            // Ensure we don't overwrite our newly initialized data
            mImageArray[level]->markClean();

            fastUnpacked = true;
        }
    }

    if (!fastUnpacked)
    {
        gl::Error error = TextureD3D::setImage(unpack, type, pixels, index);
        if (error.isError())
        {
            return error;
        }
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureD3D_3D::setCompressedImage(GLenum target, GLint level, GLenum format,
                                            GLsizei width, GLsizei height,GLsizei depth,
                                            GLsizei imageSize, const void *pixels)
{
    ASSERT(target == GL_TEXTURE_3D);

    // compressed formats don't have separate sized internal formats-- we can just use the compressed format directly
    redefineImage(level, format, width, height, depth);

    return TextureD3D::setCompressedImage(imageSize, pixels, mImageArray[level]);
}

gl::Error TextureD3D_3D::subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                  GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                  const gl::PixelUnpackState &unpack, const void *pixels)
{
    ASSERT(target == GL_TEXTURE_3D);

    bool fastUnpacked = false;

    gl::ImageIndex index = gl::ImageIndex::Make3D(level);

    // Attempt a fast gpu copy of the pixel data to the surface if the app bound an unpack buffer
    if (isFastUnpackable(unpack, getInternalFormat(level)))
    {
        RenderTarget *destRenderTarget = getRenderTarget(index);

        if (destRenderTarget)
        {
            gl::Box destArea(xoffset, yoffset, zoffset, width, height, depth);
            gl::Error error = fastUnpackPixels(unpack, pixels, destArea, getInternalFormat(level), type, destRenderTarget);
            if (error.isError())
            {
                return error;
            }

            // Ensure we don't overwrite our newly initialized data
            mImageArray[level]->markClean();

            fastUnpacked = true;
        }
    }

    if (!fastUnpacked)
    {
        return TextureD3D::subImage(xoffset, yoffset, zoffset, width, height, depth, format, type,
                                    unpack, pixels, index);
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureD3D_3D::subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                            GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                                            GLsizei imageSize, const void *pixels)
{
    ASSERT(target == GL_TEXTURE_3D);

    gl::Error error = TextureD3D::subImageCompressed(xoffset, yoffset, zoffset, width, height, depth,
                                                     format, imageSize, pixels, mImageArray[level]);
    if (error.isError())
    {
        return error;
    }

    gl::ImageIndex index = gl::ImageIndex::Make3D(level);
    gl::Box region(xoffset, yoffset, zoffset, width, height, depth);
    return commitRegion(index, region);
}

void TextureD3D_3D::copyImage(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source)
{
    UNIMPLEMENTED();
}

void TextureD3D_3D::copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source)
{
    ASSERT(target == GL_TEXTURE_3D);

    // can only make our texture storage to a render target if level 0 is defined (with a width & height) and
    // the current level we're copying to is defined (with appropriate format, width & height)
    bool canCreateRenderTarget = isLevelComplete(level) && isLevelComplete(0);

    gl::Rectangle sourceRect(x, y, width, height);

    if (!mImageArray[level]->isRenderableFormat() || (!mTexStorage && !canCreateRenderTarget))
    {
        mImageArray[level]->copy(xoffset, yoffset, zoffset, sourceRect, source);
        mDirtyImages = true;
    }
    else
    {
        ensureRenderTarget();

        if (isValidLevel(level))
        {
            updateStorageLevel(level);

            mRenderer->copyImage3D(source, sourceRect,
                                   gl::GetInternalFormatInfo(getBaseLevelInternalFormat()).format,
                                   xoffset, yoffset, zoffset, mTexStorage, level);
        }
    }
}

void TextureD3D_3D::storage(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    ASSERT(target == GL_TEXTURE_3D);

    for (int level = 0; level < levels; level++)
    {
        GLsizei levelWidth = std::max(1, width >> level);
        GLsizei levelHeight = std::max(1, height >> level);
        GLsizei levelDepth = std::max(1, depth >> level);
        mImageArray[level]->redefine(mRenderer, GL_TEXTURE_3D, internalformat, levelWidth, levelHeight, levelDepth, true);
    }

    for (int level = levels; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        mImageArray[level]->redefine(mRenderer, GL_TEXTURE_3D, GL_NONE, 0, 0, 0, true);
    }

    mImmutable = true;

    bool renderTarget = IsRenderTargetUsage(mUsage);
    TextureStorage *storage = mRenderer->createTextureStorage3D(internalformat, renderTarget, width, height, depth, levels);
    setCompleteTexStorage(storage);
}

void TextureD3D_3D::bindTexImage(egl::Surface *surface)
{
    UNREACHABLE();
}

void TextureD3D_3D::releaseTexImage()
{
    UNREACHABLE();
}


void TextureD3D_3D::initMipmapsImages()
{
    // Purge array levels 1 through q and reset them to represent the generated mipmap levels.
    int levelCount = mipLevels();
    for (int level = 1; level < levelCount; level++)
    {
        redefineImage(level, getBaseLevelInternalFormat(),
                      std::max(getBaseLevelWidth() >> level, 1),
                      std::max(getBaseLevelHeight() >> level, 1),
                      std::max(getBaseLevelDepth() >> level, 1));
    }
}

unsigned int TextureD3D_3D::getRenderTargetSerial(const gl::ImageIndex &index)
{
    return (ensureRenderTarget() ? mTexStorage->getRenderTargetSerial(index) : 0);
}

RenderTarget *TextureD3D_3D::getRenderTarget(const gl::ImageIndex &index)
{
    // ensure the underlying texture is created
    if (!ensureRenderTarget())
    {
        return NULL;
    }

    if (index.hasLayer())
    {
        updateStorage();
    }
    else
    {
        updateStorageLevel(index.mipIndex);
    }

    return mTexStorage->getRenderTarget(index);
}

void TextureD3D_3D::initializeStorage(bool renderTarget)
{
    // Only initialize the first time this texture is used as a render target or shader resource
    if (mTexStorage)
    {
        return;
    }

    // do not attempt to create storage for nonexistant data
    if (!isLevelComplete(0))
    {
        return;
    }

    bool createRenderTarget = (renderTarget || mUsage == GL_FRAMEBUFFER_ATTACHMENT_ANGLE);

    setCompleteTexStorage(createCompleteStorage(createRenderTarget));
    ASSERT(mTexStorage);

    // flush image data to the storage
    updateStorage();
}

TextureStorage *TextureD3D_3D::createCompleteStorage(bool renderTarget) const
{
    GLsizei width = getBaseLevelWidth();
    GLsizei height = getBaseLevelHeight();
    GLsizei depth = getBaseLevelDepth();
    GLenum internalFormat = getBaseLevelInternalFormat();

    ASSERT(width > 0 && height > 0 && depth > 0);

    // use existing storage level count, when previously specified by TexStorage*D
    GLint levels = (mTexStorage ? mTexStorage->getLevelCount() : creationLevels(width, height, depth));

    return mRenderer->createTextureStorage3D(internalFormat, renderTarget, width, height, depth, levels);
}

void TextureD3D_3D::setCompleteTexStorage(TextureStorage *newCompleteTexStorage)
{
    SafeDelete(mTexStorage);
    mTexStorage = newCompleteTexStorage;
    mDirtyImages = true;

    // We do not support managed 3D storage, as that is D3D9/ES2-only
    ASSERT(!mTexStorage->isManaged());
}

void TextureD3D_3D::updateStorage()
{
    ASSERT(mTexStorage != NULL);
    GLint storageLevels = mTexStorage->getLevelCount();
    for (int level = 0; level < storageLevels; level++)
    {
        if (mImageArray[level]->isDirty() && isLevelComplete(level))
        {
            updateStorageLevel(level);
        }
    }
}

bool TextureD3D_3D::isValidLevel(int level) const
{
    return (mTexStorage ? (level >= 0 && level < mTexStorage->getLevelCount()) : 0);
}

bool TextureD3D_3D::isLevelComplete(int level) const
{
    ASSERT(level >= 0 && level < (int)ArraySize(mImageArray) && mImageArray[level] != NULL);

    if (isImmutable())
    {
        return true;
    }

    GLsizei width = getBaseLevelWidth();
    GLsizei height = getBaseLevelHeight();
    GLsizei depth = getBaseLevelDepth();

    if (width <= 0 || height <= 0 || depth <= 0)
    {
        return false;
    }

    if (level == 0)
    {
        return true;
    }

    ImageD3D *levelImage = mImageArray[level];

    if (levelImage->getInternalFormat() != getBaseLevelInternalFormat())
    {
        return false;
    }

    if (levelImage->getWidth() != std::max(1, width >> level))
    {
        return false;
    }

    if (levelImage->getHeight() != std::max(1, height >> level))
    {
        return false;
    }

    if (levelImage->getDepth() != std::max(1, depth >> level))
    {
        return false;
    }

    return true;
}

void TextureD3D_3D::updateStorageLevel(int level)
{
    ASSERT(level >= 0 && level < (int)ArraySize(mImageArray) && mImageArray[level] != NULL);
    ASSERT(isLevelComplete(level));

    if (mImageArray[level]->isDirty())
    {
        gl::ImageIndex index = gl::ImageIndex::Make3D(level);
        gl::Box region(0, 0, 0, getWidth(level), getHeight(level), getDepth(level));
        commitRegion(index, region);
    }
}

void TextureD3D_3D::redefineImage(GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    // If there currently is a corresponding storage texture image, it has these parameters
    const int storageWidth = std::max(1, getBaseLevelWidth() >> level);
    const int storageHeight = std::max(1, getBaseLevelHeight() >> level);
    const int storageDepth = std::max(1, getBaseLevelDepth() >> level);
    const GLenum storageFormat = getBaseLevelInternalFormat();

    mImageArray[level]->redefine(mRenderer, GL_TEXTURE_3D, internalformat, width, height, depth, false);

    if (mTexStorage)
    {
        const int storageLevels = mTexStorage->getLevelCount();

        if ((level >= storageLevels && storageLevels != 0) ||
            width != storageWidth ||
            height != storageHeight ||
            depth != storageDepth ||
            internalformat != storageFormat)   // Discard mismatched storage
        {
            for (int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
            {
                mImageArray[i]->markDirty();
            }

            SafeDelete(mTexStorage);
            mDirtyImages = true;
        }
    }
}

gl::Error TextureD3D_3D::commitRegion(const gl::ImageIndex &index, const gl::Box &region)
{
    ASSERT(!index.hasLayer());
    GLint level = index.mipIndex;

    if (isValidLevel(level))
    {
        ImageD3D *image = mImageArray[level];
        gl::Error error = image->copyToStorage3D(mTexStorage, index, region);
        if (error.isError())
        {
            return error;
        }

        image->markClean();
    }

    return gl::Error(GL_NO_ERROR);
}

gl::ImageIndexIterator TextureD3D_3D::imageIterator() const
{
    return gl::ImageIndexIterator::Make3D(0, mTexStorage->getLevelCount(),
                                          gl::ImageIndex::ENTIRE_LEVEL, gl::ImageIndex::ENTIRE_LEVEL);
}

gl::ImageIndex TextureD3D_3D::getImageIndex(GLint mip, GLint /*layer*/) const
{
    // The "layer" here does not apply to 3D images. We use one Image per mip.
    return gl::ImageIndex::Make3D(mip);
}

TextureD3D_2DArray::TextureD3D_2DArray(Renderer *renderer)
    : TextureD3D(renderer)
{
    for (int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; ++level)
    {
        mLayerCounts[level] = 0;
        mImageArray[level] = NULL;
    }
}

TextureD3D_2DArray::~TextureD3D_2DArray()
{
    // Delete the Images before the TextureStorage.
    // Images might be relying on the TextureStorage for some of their data.
    // If TextureStorage is deleted before the Images, then their data will be wastefully copied back from the GPU before we delete the Images.
    deleteImages();
    SafeDelete(mTexStorage);
}

Image *TextureD3D_2DArray::getImage(int level, int layer) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(layer < mLayerCounts[level]);
    return mImageArray[level][layer];
}

Image *TextureD3D_2DArray::getImage(const gl::ImageIndex &index) const
{
    ASSERT(index.mipIndex < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(index.layerIndex < mLayerCounts[index.mipIndex]);
    ASSERT(index.type == GL_TEXTURE_2D_ARRAY);
    return mImageArray[index.mipIndex][index.layerIndex];
}

GLsizei TextureD3D_2DArray::getLayerCount(int level) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    return mLayerCounts[level];
}

GLsizei TextureD3D_2DArray::getWidth(GLint level) const
{
    return (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS && mLayerCounts[level] > 0) ? mImageArray[level][0]->getWidth() : 0;
}

GLsizei TextureD3D_2DArray::getHeight(GLint level) const
{
    return (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS && mLayerCounts[level] > 0) ? mImageArray[level][0]->getHeight() : 0;
}

GLenum TextureD3D_2DArray::getInternalFormat(GLint level) const
{
    return (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS && mLayerCounts[level] > 0) ? mImageArray[level][0]->getInternalFormat() : GL_NONE;
}

bool TextureD3D_2DArray::isDepth(GLint level) const
{
    return gl::GetInternalFormatInfo(getInternalFormat(level)).depthBits > 0;
}

gl::Error TextureD3D_2DArray::setImage(GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth,
                                       GLenum internalFormat, GLenum format, GLenum type, const gl::PixelUnpackState &unpack,
                                       const void *pixels)
{
    ASSERT(target == GL_TEXTURE_2D_ARRAY);

    GLenum sizedInternalFormat = gl::GetSizedInternalFormat(internalFormat, type);

    redefineImage(level, sizedInternalFormat, width, height, depth);

    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(sizedInternalFormat);
    GLsizei inputDepthPitch = formatInfo.computeDepthPitch(type, width, height, unpack.alignment);

    for (int i = 0; i < depth; i++)
    {
        const void *layerPixels = pixels ? (reinterpret_cast<const unsigned char*>(pixels) + (inputDepthPitch * i)) : NULL;
        gl::ImageIndex index = gl::ImageIndex::Make2DArray(level, i);
        gl::Error error = TextureD3D::setImage(unpack, type, layerPixels, index);
        if (error.isError())
        {
            return error;
        }
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureD3D_2DArray::setCompressedImage(GLenum target, GLint level, GLenum format,
                                                 GLsizei width, GLsizei height, GLsizei depth,
                                                 GLsizei imageSize, const void *pixels)
{
    ASSERT(target == GL_TEXTURE_2D_ARRAY);

    // compressed formats don't have separate sized internal formats-- we can just use the compressed format directly
    redefineImage(level, format, width, height, depth);

    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(format);
    GLsizei inputDepthPitch = formatInfo.computeDepthPitch(GL_UNSIGNED_BYTE, width, height, 1);

    for (int i = 0; i < depth; i++)
    {
        const void *layerPixels = pixels ? (reinterpret_cast<const unsigned char*>(pixels) + (inputDepthPitch * i)) : NULL;
        gl::Error error = TextureD3D::setCompressedImage(imageSize, layerPixels, mImageArray[level][i]);
        if (error.isError())
        {
            return error;
        }
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureD3D_2DArray::subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                       GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                       const gl::PixelUnpackState &unpack, const void *pixels)
{
    ASSERT(target == GL_TEXTURE_2D_ARRAY);

    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(getInternalFormat(level));
    GLsizei inputDepthPitch = formatInfo.computeDepthPitch(type, width, height, unpack.alignment);

    for (int i = 0; i < depth; i++)
    {
        int layer = zoffset + i;
        const void *layerPixels = pixels ? (reinterpret_cast<const unsigned char*>(pixels) + (inputDepthPitch * i)) : NULL;

        gl::ImageIndex index = gl::ImageIndex::Make2DArray(level, layer);
        gl::Error error = TextureD3D::subImage(xoffset, yoffset, zoffset, width, height, 1, format, type,
                                               unpack, layerPixels, index);
        if (error.isError())
        {
            return error;
        }
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureD3D_2DArray::subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                                 GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                                                 GLsizei imageSize, const void *pixels)
{
    ASSERT(target == GL_TEXTURE_2D_ARRAY);

    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(format);
    GLsizei inputDepthPitch = formatInfo.computeDepthPitch(GL_UNSIGNED_BYTE, width, height, 1);

    for (int i = 0; i < depth; i++)
    {
        int layer = zoffset + i;
        const void *layerPixels = pixels ? (reinterpret_cast<const unsigned char*>(pixels) + (inputDepthPitch * i)) : NULL;

        gl::Error error = TextureD3D::subImageCompressed(xoffset, yoffset, zoffset, width, height, 1, format, imageSize, layerPixels, mImageArray[level][layer]);
        if (error.isError())
        {
            return error;
        }

        gl::ImageIndex index = gl::ImageIndex::Make2DArray(level, layer);
        gl::Box region(xoffset, yoffset, 0, width, height, 1);
        error = commitRegion(index, region);
        if (error.isError())
        {
            return error;
        }
    }

    return gl::Error(GL_NO_ERROR);
}

void TextureD3D_2DArray::copyImage(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source)
{
    UNIMPLEMENTED();
}

void TextureD3D_2DArray::copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source)
{
    ASSERT(target == GL_TEXTURE_2D_ARRAY);

    // can only make our texture storage to a render target if level 0 is defined (with a width & height) and
    // the current level we're copying to is defined (with appropriate format, width & height)
    bool canCreateRenderTarget = isLevelComplete(level) && isLevelComplete(0);

    gl::Rectangle sourceRect(x, y, width, height);

    if (!mImageArray[level][0]->isRenderableFormat() || (!mTexStorage && !canCreateRenderTarget))
    {
        mImageArray[level][zoffset]->copy(xoffset, yoffset, 0, sourceRect, source);
        mDirtyImages = true;
    }
    else
    {
        ensureRenderTarget();

        if (isValidLevel(level))
        {
            updateStorageLevel(level);

            mRenderer->copyImage2DArray(source, sourceRect, gl::GetInternalFormatInfo(getInternalFormat(0)).format,
                                        xoffset, yoffset, zoffset, mTexStorage, level);
        }
    }
}

void TextureD3D_2DArray::storage(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    ASSERT(target == GL_TEXTURE_2D_ARRAY);

    deleteImages();

    for (int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        GLsizei levelWidth = std::max(1, width >> level);
        GLsizei levelHeight = std::max(1, height >> level);

        mLayerCounts[level] = (level < levels ? depth : 0);

        if (mLayerCounts[level] > 0)
        {
            // Create new images for this level
            mImageArray[level] = new ImageD3D*[mLayerCounts[level]];

            for (int layer = 0; layer < mLayerCounts[level]; layer++)
            {
                mImageArray[level][layer] = ImageD3D::makeImageD3D(mRenderer->createImage());
                mImageArray[level][layer]->redefine(mRenderer, GL_TEXTURE_2D_ARRAY, internalformat, levelWidth,
                                                    levelHeight, 1, true);
            }
        }
    }

    mImmutable = true;

    bool renderTarget = IsRenderTargetUsage(mUsage);
    TextureStorage *storage = mRenderer->createTextureStorage2DArray(internalformat, renderTarget, width, height, depth, levels);
    setCompleteTexStorage(storage);
}

void TextureD3D_2DArray::bindTexImage(egl::Surface *surface)
{
    UNREACHABLE();
}

void TextureD3D_2DArray::releaseTexImage()
{
    UNREACHABLE();
}


void TextureD3D_2DArray::initMipmapsImages()
{
    int baseWidth = getBaseLevelWidth();
    int baseHeight = getBaseLevelHeight();
    int baseDepth = getBaseLevelDepth();
    GLenum baseFormat = getBaseLevelInternalFormat();

    // Purge array levels 1 through q and reset them to represent the generated mipmap levels.
    int levelCount = mipLevels();
    for (int level = 1; level < levelCount; level++)
    {
        redefineImage(level, baseFormat, std::max(baseWidth >> level, 1), std::max(baseHeight >> level, 1), baseDepth);
    }
}

unsigned int TextureD3D_2DArray::getRenderTargetSerial(const gl::ImageIndex &index)
{
    return (ensureRenderTarget() ? mTexStorage->getRenderTargetSerial(index) : 0);
}

RenderTarget *TextureD3D_2DArray::getRenderTarget(const gl::ImageIndex &index)
{
    // ensure the underlying texture is created
    if (!ensureRenderTarget())
    {
        return NULL;
    }

    updateStorageLevel(index.mipIndex);
    return mTexStorage->getRenderTarget(index);
}

void TextureD3D_2DArray::initializeStorage(bool renderTarget)
{
    // Only initialize the first time this texture is used as a render target or shader resource
    if (mTexStorage)
    {
        return;
    }

    // do not attempt to create storage for nonexistant data
    if (!isLevelComplete(0))
    {
        return;
    }

    bool createRenderTarget = (renderTarget || mUsage == GL_FRAMEBUFFER_ATTACHMENT_ANGLE);

    setCompleteTexStorage(createCompleteStorage(createRenderTarget));
    ASSERT(mTexStorage);

    // flush image data to the storage
    updateStorage();
}

TextureStorage *TextureD3D_2DArray::createCompleteStorage(bool renderTarget) const
{
    GLsizei width = getBaseLevelWidth();
    GLsizei height = getBaseLevelHeight();
    GLsizei depth = getLayerCount(0);
    GLenum internalFormat = getBaseLevelInternalFormat();

    ASSERT(width > 0 && height > 0 && depth > 0);

    // use existing storage level count, when previously specified by TexStorage*D
    GLint levels = (mTexStorage ? mTexStorage->getLevelCount() : creationLevels(width, height, 1));

    return mRenderer->createTextureStorage2DArray(internalFormat, renderTarget, width, height, depth, levels);
}

void TextureD3D_2DArray::setCompleteTexStorage(TextureStorage *newCompleteTexStorage)
{
    SafeDelete(mTexStorage);
    mTexStorage = newCompleteTexStorage;
    mDirtyImages = true;

    // We do not support managed 2D array storage, as managed storage is ES2/D3D9 only
    ASSERT(!mTexStorage->isManaged());
}

void TextureD3D_2DArray::updateStorage()
{
    ASSERT(mTexStorage != NULL);
    GLint storageLevels = mTexStorage->getLevelCount();
    for (int level = 0; level < storageLevels; level++)
    {
        if (isLevelComplete(level))
        {
            updateStorageLevel(level);
        }
    }
}

bool TextureD3D_2DArray::isValidLevel(int level) const
{
    return (mTexStorage ? (level >= 0 && level < mTexStorage->getLevelCount()) : 0);
}

bool TextureD3D_2DArray::isLevelComplete(int level) const
{
    ASSERT(level >= 0 && level < (int)ArraySize(mImageArray));

    if (isImmutable())
    {
        return true;
    }

    GLsizei width = getBaseLevelWidth();
    GLsizei height = getBaseLevelHeight();
    GLsizei layers = getLayerCount(0);

    if (width <= 0 || height <= 0 || layers <= 0)
    {
        return false;
    }

    if (level == 0)
    {
        return true;
    }

    if (getInternalFormat(level) != getInternalFormat(0))
    {
        return false;
    }

    if (getWidth(level) != std::max(1, width >> level))
    {
        return false;
    }

    if (getHeight(level) != std::max(1, height >> level))
    {
        return false;
    }

    if (getLayerCount(level) != layers)
    {
        return false;
    }

    return true;
}

void TextureD3D_2DArray::updateStorageLevel(int level)
{
    ASSERT(level >= 0 && level < (int)ArraySize(mLayerCounts));
    ASSERT(isLevelComplete(level));

    for (int layer = 0; layer < mLayerCounts[level]; layer++)
    {
        ASSERT(mImageArray[level] != NULL && mImageArray[level][layer] != NULL);
        if (mImageArray[level][layer]->isDirty())
        {
            gl::ImageIndex index = gl::ImageIndex::Make2DArray(level, layer);
            gl::Box region(0, 0, 0, getWidth(level), getHeight(level), 1);
            commitRegion(index, region);
        }
    }
}

void TextureD3D_2DArray::deleteImages()
{
    for (int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; ++level)
    {
        for (int layer = 0; layer < mLayerCounts[level]; ++layer)
        {
            delete mImageArray[level][layer];
        }
        delete[] mImageArray[level];
        mImageArray[level] = NULL;
        mLayerCounts[level] = 0;
    }
}

void TextureD3D_2DArray::redefineImage(GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    // If there currently is a corresponding storage texture image, it has these parameters
    const int storageWidth = std::max(1, getBaseLevelWidth() >> level);
    const int storageHeight = std::max(1, getBaseLevelHeight() >> level);
    const int storageDepth = getLayerCount(0);
    const GLenum storageFormat = getBaseLevelInternalFormat();

    for (int layer = 0; layer < mLayerCounts[level]; layer++)
    {
        delete mImageArray[level][layer];
    }
    delete[] mImageArray[level];
    mImageArray[level] = NULL;
    mLayerCounts[level] = depth;

    if (depth > 0)
    {
        mImageArray[level] = new ImageD3D*[depth]();

        for (int layer = 0; layer < mLayerCounts[level]; layer++)
        {
            mImageArray[level][layer] = ImageD3D::makeImageD3D(mRenderer->createImage());
            mImageArray[level][layer]->redefine(mRenderer, GL_TEXTURE_2D_ARRAY, internalformat, width, height, 1, false);
        }
    }

    if (mTexStorage)
    {
        const int storageLevels = mTexStorage->getLevelCount();

        if ((level >= storageLevels && storageLevels != 0) ||
            width != storageWidth ||
            height != storageHeight ||
            depth != storageDepth ||
            internalformat != storageFormat)   // Discard mismatched storage
        {
            for (int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
            {
                for (int layer = 0; layer < mLayerCounts[level]; layer++)
                {
                    mImageArray[level][layer]->markDirty();
                }
            }

            delete mTexStorage;
            mTexStorage = NULL;
            mDirtyImages = true;
        }
    }
}

gl::Error TextureD3D_2DArray::commitRegion(const gl::ImageIndex &index, const gl::Box &region)
{
    ASSERT(index.hasLayer());
    GLint level = index.mipIndex;
    GLint layerTarget = index.layerIndex;

    if (isValidLevel(level) && layerTarget < getLayerCount(level))
    {
        ImageD3D *image = mImageArray[level][layerTarget];
        gl::Error error = image->copyToStorage2DArray(mTexStorage, index, region);
        if (error.isError())
        {
            return error;
        }

        image->markClean();
    }

    return gl::Error(GL_NO_ERROR);
}

gl::ImageIndexIterator TextureD3D_2DArray::imageIterator() const
{
    return gl::ImageIndexIterator::Make2DArray(0, mTexStorage->getLevelCount(), mLayerCounts);
}

gl::ImageIndex TextureD3D_2DArray::getImageIndex(GLint mip, GLint layer) const
{
    return gl::ImageIndex::Make2DArray(mip, layer);
}

}
