//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureNULL.cpp:
//    Implements the class methods for TextureNULL.
//

#include "libANGLE/renderer/null/TextureNULL.h"

#include "common/debug.h"

namespace rx
{

TextureNULL::TextureNULL(const gl::TextureState &state) : TextureImpl(state)
{
}

TextureNULL::~TextureNULL()
{
}

gl::Error TextureNULL::setImage(GLenum target,
                                size_t level,
                                GLenum internalFormat,
                                const gl::Extents &size,
                                GLenum format,
                                GLenum type,
                                const gl::PixelUnpackState &unpack,
                                const uint8_t *pixels)
{
    // TODO(geofflang): Read all incoming pixel data (maybe hash it?) to make sure we don't read out
    // of bounds due to validation bugs.
    return gl::NoError();
}

gl::Error TextureNULL::setSubImage(GLenum target,
                                   size_t level,
                                   const gl::Box &area,
                                   GLenum format,
                                   GLenum type,
                                   const gl::PixelUnpackState &unpack,
                                   const uint8_t *pixels)
{
    return gl::NoError();
}

gl::Error TextureNULL::setCompressedImage(GLenum target,
                                          size_t level,
                                          GLenum internalFormat,
                                          const gl::Extents &size,
                                          const gl::PixelUnpackState &unpack,
                                          size_t imageSize,
                                          const uint8_t *pixels)
{
    return gl::NoError();
}

gl::Error TextureNULL::setCompressedSubImage(GLenum target,
                                             size_t level,
                                             const gl::Box &area,
                                             GLenum format,
                                             const gl::PixelUnpackState &unpack,
                                             size_t imageSize,
                                             const uint8_t *pixels)
{
    return gl::NoError();
}

gl::Error TextureNULL::copyImage(GLenum target,
                                 size_t level,
                                 const gl::Rectangle &sourceArea,
                                 GLenum internalFormat,
                                 const gl::Framebuffer *source)
{
    return gl::NoError();
}

gl::Error TextureNULL::copySubImage(GLenum target,
                                    size_t level,
                                    const gl::Offset &destOffset,
                                    const gl::Rectangle &sourceArea,
                                    const gl::Framebuffer *source)
{
    return gl::NoError();
}

gl::Error TextureNULL::setStorage(GLenum target,
                                  size_t levels,
                                  GLenum internalFormat,
                                  const gl::Extents &size)
{
    return gl::NoError();
}

gl::Error TextureNULL::setEGLImageTarget(GLenum target, egl::Image *image)
{
    return gl::NoError();
}

gl::Error TextureNULL::setImageExternal(GLenum target,
                                        egl::Stream *stream,
                                        const egl::Stream::GLTextureDescription &desc)
{
    return gl::NoError();
}

gl::Error TextureNULL::generateMipmap()
{
    return gl::NoError();
}

void TextureNULL::setBaseLevel(GLuint baseLevel)
{
}

void TextureNULL::bindTexImage(egl::Surface *surface)
{
}

void TextureNULL::releaseTexImage()
{
}

void TextureNULL::syncState(const gl::Texture::DirtyBits &dirtyBits)
{
}

}  // namespace rx
