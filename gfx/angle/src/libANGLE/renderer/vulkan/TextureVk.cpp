//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureVk.cpp:
//    Implements the class methods for TextureVk.
//

#include "libANGLE/renderer/vulkan/TextureVk.h"

#include "common/debug.h"

namespace rx
{

TextureVk::TextureVk(const gl::TextureState &state) : TextureImpl(state)
{
}

TextureVk::~TextureVk()
{
}

gl::Error TextureVk::setImage(const gl::Context *context,
                              GLenum target,
                              size_t level,
                              GLenum internalFormat,
                              const gl::Extents &size,
                              GLenum format,
                              GLenum type,
                              const gl::PixelUnpackState &unpack,
                              const uint8_t *pixels)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setSubImage(const gl::Context *context,
                                 GLenum target,
                                 size_t level,
                                 const gl::Box &area,
                                 GLenum format,
                                 GLenum type,
                                 const gl::PixelUnpackState &unpack,
                                 const uint8_t *pixels)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setCompressedImage(const gl::Context *context,
                                        GLenum target,
                                        size_t level,
                                        GLenum internalFormat,
                                        const gl::Extents &size,
                                        const gl::PixelUnpackState &unpack,
                                        size_t imageSize,
                                        const uint8_t *pixels)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setCompressedSubImage(const gl::Context *context,
                                           GLenum target,
                                           size_t level,
                                           const gl::Box &area,
                                           GLenum format,
                                           const gl::PixelUnpackState &unpack,
                                           size_t imageSize,
                                           const uint8_t *pixels)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::copyImage(const gl::Context *context,
                               GLenum target,
                               size_t level,
                               const gl::Rectangle &sourceArea,
                               GLenum internalFormat,
                               const gl::Framebuffer *source)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::copySubImage(const gl::Context *context,
                                  GLenum target,
                                  size_t level,
                                  const gl::Offset &destOffset,
                                  const gl::Rectangle &sourceArea,
                                  const gl::Framebuffer *source)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setStorage(const gl::Context *context,
                                GLenum target,
                                size_t levels,
                                GLenum internalFormat,
                                const gl::Extents &size)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setEGLImageTarget(const gl::Context *context, GLenum target, egl::Image *image)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setImageExternal(const gl::Context *context,
                                      GLenum target,
                                      egl::Stream *stream,
                                      const egl::Stream::GLTextureDescription &desc)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::generateMipmap(const gl::Context *context)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::setBaseLevel(const gl::Context *context, GLuint baseLevel)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::releaseTexImage(const gl::Context *context)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureVk::getAttachmentRenderTarget(const gl::Context *context,
                                               GLenum binding,
                                               const gl::ImageIndex &imageIndex,
                                               FramebufferAttachmentRenderTarget **rtOut)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

void TextureVk::syncState(const gl::Texture::DirtyBits &dirtyBits)
{
    UNIMPLEMENTED();
}

gl::Error TextureVk::setStorageMultisample(const gl::Context *context,
                                           GLenum target,
                                           GLsizei samples,
                                           GLint internalformat,
                                           const gl::Extents &size,
                                           GLboolean fixedSampleLocations)
{
    UNIMPLEMENTED();
    return gl::InternalError() << "setStorageMultisample is unimplemented.";
}

}  // namespace rx
