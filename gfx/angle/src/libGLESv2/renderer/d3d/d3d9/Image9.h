//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image9.h: Defines the rx::Image9 class, which acts as the interface to
// the actual underlying surfaces of a Texture.

#ifndef LIBGLESV2_RENDERER_IMAGE9_H_
#define LIBGLESV2_RENDERER_IMAGE9_H_

#include "libGLESv2/renderer/d3d/ImageD3D.h"
#include "common/debug.h"

namespace gl
{
class Framebuffer;
}

namespace rx
{
class Renderer;
class Renderer9;

class Image9 : public ImageD3D
{
  public:
    Image9();
    ~Image9();

    static Image9 *makeImage9(Image *img);

    static void generateMipmap(Image9 *dest, Image9 *source);
    static void generateMip(IDirect3DSurface9 *destSurface, IDirect3DSurface9 *sourceSurface);
    static void copyLockableSurfaces(IDirect3DSurface9 *dest, IDirect3DSurface9 *source);

    virtual bool redefine(Renderer *renderer, GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, bool forceRelease);

    D3DFORMAT getD3DFormat() const;

    virtual bool isDirty() const;
    IDirect3DSurface9 *getSurface();

    virtual void setManagedSurface2D(TextureStorage *storage, int level);
    virtual void setManagedSurfaceCube(TextureStorage *storage, int face, int level);
    virtual gl::Error copyToStorage2D(TextureStorage *storage, int level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);
    virtual gl::Error copyToStorageCube(TextureStorage *storage, int face, int level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);
    virtual gl::Error copyToStorage3D(TextureStorage *storage, int level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth);
    virtual gl::Error copyToStorage2DArray(TextureStorage *storage, int level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height);

    virtual gl::Error loadData(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                               GLint unpackAlignment, GLenum type, const void *input);
    virtual gl::Error loadCompressedData(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                         const void *input);

    virtual void copy(GLint xoffset, GLint yoffset, GLint zoffset,GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source);

  private:
    DISALLOW_COPY_AND_ASSIGN(Image9);

    void createSurface();
    void setManagedSurface(IDirect3DSurface9 *surface);
    gl::Error copyToSurface(IDirect3DSurface9 *dest, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height);

    HRESULT lock(D3DLOCKED_RECT *lockedRect, const RECT *rect);
    void unlock();

    Renderer9 *mRenderer;

    D3DPOOL mD3DPool;   // can only be D3DPOOL_SYSTEMMEM or D3DPOOL_MANAGED since it needs to be lockable.
    D3DFORMAT mD3DFormat;

    IDirect3DSurface9 *mSurface;
};
}

#endif   // LIBGLESV2_RENDERER_IMAGE9_H_
