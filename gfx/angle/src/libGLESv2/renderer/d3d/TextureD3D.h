//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureD3D.h: Implementations of the Texture interfaces shared betweeen the D3D backends.

#ifndef LIBGLESV2_RENDERER_TEXTURED3D_H_
#define LIBGLESV2_RENDERER_TEXTURED3D_H_

#include "libGLESv2/renderer/TextureImpl.h"
#include "libGLESv2/angletypes.h"
#include "libGLESv2/Constants.h"

namespace gl
{
class Framebuffer;
}

namespace rx
{

class Image;
class ImageD3D;
class Renderer;
class TextureStorageInterface;
class TextureStorageInterface2D;
class TextureStorageInterfaceCube;
class TextureStorageInterface3D;
class TextureStorageInterface2DArray;

class TextureD3D : public TextureImpl
{
  public:
    TextureD3D(Renderer *renderer);
    virtual ~TextureD3D();

    static TextureD3D *makeTextureD3D(TextureImpl *texture);

    virtual TextureStorageInterface *getNativeTexture();

    virtual void setUsage(GLenum usage) { mUsage = usage; }
    bool hasDirtyImages() const { return mDirtyImages; }
    void resetDirty() { mDirtyImages = false; }

    GLint getBaseLevelWidth() const;
    GLint getBaseLevelHeight() const;
    GLint getBaseLevelDepth() const;
    GLenum getBaseLevelInternalFormat() const;

    bool isImmutable() const { return mImmutable; }

  protected:
    void setImage(const gl::PixelUnpackState &unpack, GLenum type, const void *pixels, Image *image);
    bool subImage(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                  GLenum format, GLenum type, const gl::PixelUnpackState &unpack, const void *pixels, Image *image);
    void setCompressedImage(GLsizei imageSize, const void *pixels, Image *image);
    bool subImageCompressed(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                            GLenum format, GLsizei imageSize, const void *pixels, Image *image);
    bool isFastUnpackable(const gl::PixelUnpackState &unpack, GLenum sizedInternalFormat);
    bool fastUnpackPixels(const gl::PixelUnpackState &unpack, const void *pixels, const gl::Box &destArea,
                                  GLenum sizedInternalFormat, GLenum type, RenderTarget *destRenderTarget);

    GLint creationLevels(GLsizei width, GLsizei height, GLsizei depth) const;
    int mipLevels() const;

    Renderer *mRenderer;

    GLenum mUsage;

    bool mDirtyImages;

    bool mImmutable;

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureD3D);

    virtual void initializeStorage(bool renderTarget) = 0;

    virtual void updateStorage() = 0;
    virtual TextureStorageInterface *getBaseLevelStorage() = 0;
    virtual const ImageD3D *getBaseLevelImage() const = 0;
};

class TextureD3D_2D : public TextureD3D
{
  public:
    TextureD3D_2D(Renderer *renderer);
    virtual ~TextureD3D_2D();

    virtual Image *getImage(int level, int layer) const;
    virtual GLsizei getLayerCount(int level) const;

    GLsizei getWidth(GLint level) const;
    GLsizei getHeight(GLint level) const;
    GLenum getInternalFormat(GLint level) const;
    GLenum getActualFormat(GLint level) const;
    bool isDepth(GLint level) const;

    virtual void setImage(GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth, GLenum internalFormat, GLenum format, GLenum type, const gl::PixelUnpackState &unpack, const void *pixels);
    virtual void setCompressedImage(GLenum target, GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLsizei imageSize, const void *pixels);
    virtual void subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const gl::PixelUnpackState &unpack, const void *pixels);
    virtual void subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *pixels);
    virtual void copyImage(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source);
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source);
    virtual void storage(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);

    virtual void bindTexImage(egl::Surface *surface);
    virtual void releaseTexImage();

    virtual void generateMipmaps();

    virtual unsigned int getRenderTargetSerial(GLint level, GLint layer);

    virtual RenderTarget *getRenderTarget(GLint level, GLint layer);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureD3D_2D);

    virtual void initializeStorage(bool renderTarget);
    TextureStorageInterface2D *createCompleteStorage(bool renderTarget) const;
    void setCompleteTexStorage(TextureStorageInterface2D *newCompleteTexStorage);

    virtual void updateStorage();
    bool ensureRenderTarget();
    virtual TextureStorageInterface *getBaseLevelStorage();
    virtual const ImageD3D *getBaseLevelImage() const;

    bool isValidLevel(int level) const;
    bool isLevelComplete(int level) const;

    void updateStorageLevel(int level);

    void redefineImage(GLint level, GLenum internalformat, GLsizei width, GLsizei height);
    void commitRect(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);

    TextureStorageInterface2D *mTexStorage;
    ImageD3D *mImageArray[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];
};

class TextureD3D_Cube : public TextureD3D
{
  public:
    TextureD3D_Cube(Renderer *renderer);
    virtual ~TextureD3D_Cube();

    virtual Image *getImage(int level, int layer) const;
    virtual GLsizei getLayerCount(int level) const;

    virtual bool hasDirtyImages() const { return mDirtyImages; }
    virtual void resetDirty() { mDirtyImages = false; }
    virtual void setUsage(GLenum usage) { mUsage = usage; }

    GLenum getInternalFormat(GLint level, GLint layer) const;
    bool isDepth(GLint level, GLint layer) const;

    virtual void setImage(GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth, GLenum internalFormat, GLenum format, GLenum type, const gl::PixelUnpackState &unpack, const void *pixels);
    virtual void setCompressedImage(GLenum target, GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLsizei imageSize, const void *pixels);
    virtual void subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const gl::PixelUnpackState &unpack, const void *pixels);
    virtual void subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *pixels);
    virtual void copyImage(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source);
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source);
    virtual void storage(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);

    virtual void bindTexImage(egl::Surface *surface);
    virtual void releaseTexImage();

    virtual void generateMipmaps();

    virtual unsigned int getRenderTargetSerial(GLint level, GLint layer);

    virtual RenderTarget *getRenderTarget(GLint level, GLint layer);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureD3D_Cube);

    virtual void initializeStorage(bool renderTarget);
    TextureStorageInterfaceCube *createCompleteStorage(bool renderTarget) const;
    void setCompleteTexStorage(TextureStorageInterfaceCube *newCompleteTexStorage);

    virtual void updateStorage();
    bool ensureRenderTarget();
    virtual TextureStorageInterface *getBaseLevelStorage();
    virtual const ImageD3D *getBaseLevelImage() const;

    bool isValidFaceLevel(int faceIndex, int level) const;
    bool isFaceLevelComplete(int faceIndex, int level) const;
    bool isCubeComplete() const;
    void updateStorageFaceLevel(int faceIndex, int level);

    void redefineImage(int faceIndex, GLint level, GLenum internalformat, GLsizei width, GLsizei height);
    void commitRect(int faceIndex, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);

    ImageD3D *mImageArray[6][gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    TextureStorageInterfaceCube *mTexStorage;
};

class TextureD3D_3D : public TextureD3D
{
  public:
    TextureD3D_3D(Renderer *renderer);
    virtual ~TextureD3D_3D();

    virtual Image *getImage(int level, int layer) const;
    virtual GLsizei getLayerCount(int level) const;

    GLsizei getWidth(GLint level) const;
    GLsizei getHeight(GLint level) const;
    GLsizei getDepth(GLint level) const;
    GLenum getInternalFormat(GLint level) const;
    bool isDepth(GLint level) const;

    virtual void setImage(GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth, GLenum internalFormat, GLenum format, GLenum type, const gl::PixelUnpackState &unpack, const void *pixels);
    virtual void setCompressedImage(GLenum target, GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLsizei imageSize, const void *pixels);
    virtual void subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const gl::PixelUnpackState &unpack, const void *pixels);
    virtual void subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *pixels);
    virtual void copyImage(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source);
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source);
    virtual void storage(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);

    virtual void bindTexImage(egl::Surface *surface);
    virtual void releaseTexImage();

    virtual void generateMipmaps();

    virtual unsigned int getRenderTargetSerial(GLint level, GLint layer);

    virtual RenderTarget *getRenderTarget(GLint level);
    virtual RenderTarget *getRenderTarget(GLint level, GLint layer);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureD3D_3D);

    virtual void initializeStorage(bool renderTarget);
    TextureStorageInterface3D *createCompleteStorage(bool renderTarget) const;
    void setCompleteTexStorage(TextureStorageInterface3D *newCompleteTexStorage);

    virtual void updateStorage();
    bool ensureRenderTarget();
    virtual TextureStorageInterface *getBaseLevelStorage();
    virtual const ImageD3D *getBaseLevelImage() const;

    bool isValidLevel(int level) const;
    bool isLevelComplete(int level) const;
    void updateStorageLevel(int level);

    void redefineImage(GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
    void commitRect(GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth);

    ImageD3D *mImageArray[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    TextureStorageInterface3D *mTexStorage;
};

class TextureD3D_2DArray : public TextureD3D
{
  public:
    TextureD3D_2DArray(Renderer *renderer);
    virtual ~TextureD3D_2DArray();

    virtual Image *getImage(int level, int layer) const;
    virtual GLsizei getLayerCount(int level) const;

    GLsizei getWidth(GLint level) const;
    GLsizei getHeight(GLint level) const;
    GLsizei getLayers(GLint level) const;
    GLenum getInternalFormat(GLint level) const;
    bool isDepth(GLint level) const;

    virtual void setImage(GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth, GLenum internalFormat, GLenum format, GLenum type, const gl::PixelUnpackState &unpack, const void *pixels);
    virtual void setCompressedImage(GLenum target, GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLsizei imageSize, const void *pixels);
    virtual void subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const gl::PixelUnpackState &unpack, const void *pixels);
    virtual void subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *pixels);
    virtual void copyImage(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source);
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source);
    virtual void storage(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);

    virtual void bindTexImage(egl::Surface *surface);
    virtual void releaseTexImage();

    virtual void generateMipmaps();

    virtual unsigned int getRenderTargetSerial(GLint level, GLint layer);

    virtual RenderTarget *getRenderTarget(GLint level, GLint layer);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureD3D_2DArray);

    virtual void initializeStorage(bool renderTarget);
    TextureStorageInterface2DArray *createCompleteStorage(bool renderTarget) const;
    void setCompleteTexStorage(TextureStorageInterface2DArray *newCompleteTexStorage);

    virtual void updateStorage();
    bool ensureRenderTarget();
    virtual TextureStorageInterface *getBaseLevelStorage();
    virtual const ImageD3D *getBaseLevelImage() const;

    bool isValidLevel(int level) const;
    bool isLevelComplete(int level) const;
    void updateStorageLevel(int level);

    void deleteImages();
    void redefineImage(GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
    void commitRect(GLint level, GLint xoffset, GLint yoffset, GLint layerTarget, GLsizei width, GLsizei height);

    // Storing images as an array of single depth textures since D3D11 treats each array level of a
    // Texture2D object as a separate subresource.  Each layer would have to be looped over
    // to update all the texture layers since they cannot all be updated at once and it makes the most
    // sense for the Image class to not have to worry about layer subresource as well as mip subresources.
    GLsizei mLayerCounts[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];
    ImageD3D **mImageArray[gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS];

    TextureStorageInterface2DArray *mTexStorage;
};

}

#endif // LIBGLESV2_RENDERER_TEXTURED3D_H_
