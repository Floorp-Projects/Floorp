//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureImpl.h: Defines the abstract rx::TextureImpl classes.

#ifndef LIBGLESV2_RENDERER_TEXTUREIMPL_H_
#define LIBGLESV2_RENDERER_TEXTUREIMPL_H_

#include "common/angleutils.h"
#include "libGLESv2/Error.h"

#include "angle_gl.h"

#include "libGLESv2/ImageIndex.h"

namespace egl
{
class Surface;
}

namespace gl
{
class Framebuffer;
struct PixelUnpackState;
struct SamplerState;
}

namespace rx
{

class Image;
class Renderer;
class TextureStorage;

class TextureImpl
{
  public:
    virtual ~TextureImpl() {};

    // TODO: If this methods could go away that would be ideal;
    // TextureStorage should only be necessary for the D3D backend, and as such
    // higher level code should not rely on it.
    virtual TextureStorage *getNativeTexture() = 0;

    // Deprecated in favour of the ImageIndex method
    virtual Image *getImage(int level, int layer) const = 0;
    virtual Image *getImage(const gl::ImageIndex &index) const = 0;
    virtual GLsizei getLayerCount(int level) const = 0;

    virtual void setUsage(GLenum usage) = 0;

    virtual gl::Error setImage(GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth, GLenum internalFormat, GLenum format, GLenum type, const gl::PixelUnpackState &unpack, const void *pixels) = 0;
    virtual gl::Error setCompressedImage(GLenum target, GLint level, GLenum format, GLsizei width, GLsizei height, GLsizei depth, GLsizei imageSize, const void *pixels) = 0;
    virtual gl::Error subImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const gl::PixelUnpackState &unpack, const void *pixels) = 0;
    virtual gl::Error subImageCompressed(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *pixels) = 0;
    virtual void copyImage(GLenum target, GLint level, GLenum format, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source) = 0;
    virtual void copySubImage(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, gl::Framebuffer *source) = 0;
    virtual void storage(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) = 0;

    virtual void generateMipmaps() = 0;

    virtual void bindTexImage(egl::Surface *surface) = 0;
    virtual void releaseTexImage() = 0;
};

}

#endif // LIBGLESV2_RENDERER_TEXTUREIMPL_H_
