//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.h: Defines the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#ifndef LIBGLESV2_FRAMEBUFFER_H_
#define LIBGLESV2_FRAMEBUFFER_H_

#include "common/angleutils.h"
#include "common/RefCountObject.h"
#include "constants.h"

namespace rx
{
class Renderer;
}

namespace gl
{
class FramebufferAttachment;
class Colorbuffer;
class Depthbuffer;
class Stencilbuffer;
class DepthStencilbuffer;

class Framebuffer
{
  public:
    explicit Framebuffer(rx::Renderer *renderer);

    virtual ~Framebuffer();

    void setColorbuffer(unsigned int colorAttachment, GLenum type, GLuint colorbuffer, GLint level, GLint layer);
    void setDepthbuffer(GLenum type, GLuint depthbuffer, GLint level, GLint layer);
    void setStencilbuffer(GLenum type, GLuint stencilbuffer, GLint level, GLint layer);
    void setDepthStencilBuffer(GLenum type, GLuint depthStencilBuffer, GLint level, GLint layer);

    void detachTexture(GLuint texture);
    void detachRenderbuffer(GLuint renderbuffer);

    FramebufferAttachment *getColorbuffer(unsigned int colorAttachment) const;
    FramebufferAttachment *getDepthbuffer() const;
    FramebufferAttachment *getStencilbuffer() const;
    FramebufferAttachment *getDepthStencilBuffer() const;
    FramebufferAttachment *getDepthOrStencilbuffer() const;
    FramebufferAttachment *getReadColorbuffer() const;
    GLenum getReadColorbufferType() const;
    FramebufferAttachment *getFirstColorbuffer() const;

    GLenum getColorbufferType(unsigned int colorAttachment) const;
    GLenum getDepthbufferType() const;
    GLenum getStencilbufferType() const;
    GLenum getDepthStencilbufferType() const;

    GLuint getColorbufferHandle(unsigned int colorAttachment) const;
    GLuint getDepthbufferHandle() const;
    GLuint getStencilbufferHandle() const;
    GLuint getDepthStencilbufferHandle() const;

    GLint getColorbufferMipLevel(unsigned int colorAttachment) const;
    GLint getDepthbufferMipLevel() const;
    GLint getStencilbufferMipLevel() const;
    GLint getDepthStencilbufferMipLevel() const;

    GLint getColorbufferLayer(unsigned int colorAttachment) const;
    GLint getDepthbufferLayer() const;
    GLint getStencilbufferLayer() const;
    GLint getDepthStencilbufferLayer() const;

    GLenum getDrawBufferState(unsigned int colorAttachment) const;
    void setDrawBufferState(unsigned int colorAttachment, GLenum drawBuffer);

    bool isEnabledColorAttachment(unsigned int colorAttachment) const;
    bool hasEnabledColorAttachment() const;
    bool hasStencil() const;
    int getSamples() const;
    bool usingExtendedDrawBuffers() const;

    virtual GLenum completeness() const;

  protected:
    rx::Renderer *mRenderer;

    FramebufferAttachment *mColorbuffers[IMPLEMENTATION_MAX_DRAW_BUFFERS];
    GLenum mDrawBufferStates[IMPLEMENTATION_MAX_DRAW_BUFFERS];
    GLenum mReadBufferState;

    FramebufferAttachment *mDepthbuffer;
    FramebufferAttachment *mStencilbuffer;

    bool hasValidDepthStencil() const;

private:
    DISALLOW_COPY_AND_ASSIGN(Framebuffer);

    FramebufferAttachment *createAttachment(GLenum type, GLuint handle, GLint level, GLint layer) const;
};

class DefaultFramebuffer : public Framebuffer
{
  public:
    DefaultFramebuffer(rx::Renderer *Renderer, Colorbuffer *colorbuffer, DepthStencilbuffer *depthStencil);

    virtual GLenum completeness() const;

  private:
    DISALLOW_COPY_AND_ASSIGN(DefaultFramebuffer);
};

}

#endif   // LIBGLESV2_FRAMEBUFFER_H_
