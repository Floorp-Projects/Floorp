//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image11.h: Defines the rx::Image11 class, which acts as the interface to
// the actual underlying resources of a Texture

#ifndef LIBGLESV2_RENDERER_IMAGE11_H_
#define LIBGLESV2_RENDERER_IMAGE11_H_

#include "libGLESv2/renderer/d3d/ImageD3D.h"

#include "common/debug.h"

namespace gl
{
class Framebuffer;
}

namespace rx
{
class Renderer;
class Renderer11;
class TextureStorageInterface2D;
class TextureStorageInterfaceCube;
class TextureStorage11;

class Image11 : public ImageD3D
{
  public:
    Image11();
    virtual ~Image11();

    static Image11 *makeImage11(Image *img);

    static void generateMipmap(Image11 *dest, Image11 *src);

    virtual bool isDirty() const;

    virtual bool copyToStorage(TextureStorageInterface2D *storage, int level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);
    virtual bool copyToStorage(TextureStorageInterfaceCube *storage, int face, int level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);
    virtual bool copyToStorage(TextureStorageInterface3D *storage, int level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth);
    virtual bool copyToStorage(TextureStorageInterface2DArray *storage, int level, GLint xoffset, GLint yoffset, GLint arrayLayer, GLsizei width, GLsizei height);

    virtual bool redefine(Renderer *renderer, GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, bool forceRelease);

    DXGI_FORMAT getDXGIFormat() const;
    
    virtual void loadData(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                          GLint unpackAlignment, GLenum type, const void *input);
    virtual void loadCompressedData(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                    const void *input);

    virtual void copy(GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source);

    bool recoverFromAssociatedStorage();
    bool isAssociatedStorageValid(TextureStorage11* textureStorage) const;
    void disassociateStorage();

  protected:
    HRESULT map(D3D11_MAP mapType, D3D11_MAPPED_SUBRESOURCE *map);
    void unmap();

  private:
    DISALLOW_COPY_AND_ASSIGN(Image11);

    bool copyToStorageImpl(TextureStorage11 *storage11, int level, int layerTarget, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);

    ID3D11Resource *getStagingTexture();
    unsigned int getStagingSubresource();
    void createStagingTexture();
    void releaseStagingTexture();

    Renderer11 *mRenderer;

    DXGI_FORMAT mDXGIFormat;
    ID3D11Resource *mStagingTexture;
    unsigned int mStagingSubresource;

    bool mRecoverFromStorage;
    TextureStorage11 *mAssociatedStorage;
    int mAssociatedStorageLevel;
    int mAssociatedStorageLayerTarget;
    unsigned int mRecoveredFromStorageCount;
};

}

#endif // LIBGLESV2_RENDERER_IMAGE11_H_
