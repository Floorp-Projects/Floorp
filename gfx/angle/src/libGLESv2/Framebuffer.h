//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.h: Defines the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#ifndef LIBGLESV2_FRAMEBUFFER_H_
#define LIBGLESV2_FRAMEBUFFER_H_

#include "libGLESv2/Error.h"

#include "common/angleutils.h"
#include "common/RefCountObject.h"
#include "Constants.h"

#include <vector>

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
struct Caps;

typedef std::vector<FramebufferAttachment *> ColorbufferInfo;

class Framebuffer
{
  public:
    Framebuffer(rx::Renderer *renderer, GLuint id);

    virtual ~Framebuffer();

    GLuint id() const { return mId; }

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

    virtual FramebufferAttachment *getAttachment(GLenum attachment) const;

    GLenum getDrawBufferState(unsigned int colorAttachment) const;
    void setDrawBufferState(unsigned int colorAttachment, GLenum drawBuffer);

    bool isEnabledColorAttachment(unsigned int colorAttachment) const;
    bool hasEnabledColorAttachment() const;
    bool hasStencil() const;
    int getSamples() const;
    bool usingExtendedDrawBuffers() const;

    virtual GLenum completeness() const;
    bool hasValidDepthStencil() const;

    Error invalidate(const Caps &caps, GLsizei numAttachments, const GLenum *attachments);
    Error invalidateSub(GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height);

    // Use this method to retrieve the color buffer map when doing rendering.
    // It will apply a workaround for poor shader performance on some systems
    // by compacting the list to skip NULL values.
    ColorbufferInfo getColorbuffersForRender() const;

  protected:
    rx::Renderer *mRenderer;

    GLuint mId;

    FramebufferAttachment *mColorbuffers[IMPLEMENTATION_MAX_DRAW_BUFFERS];
    GLenum mDrawBufferStates[IMPLEMENTATION_MAX_DRAW_BUFFERS];
    GLenum mReadBufferState;

    FramebufferAttachment *mDepthbuffer;
    FramebufferAttachment *mStencilbuffer;

  private:
    DISALLOW_COPY_AND_ASSIGN(Framebuffer);

    FramebufferAttachment *createAttachment(GLenum binding, GLenum type, GLuint handle, GLint level, GLint layer) const;
};

class DefaultFramebuffer : public Framebuffer
{
  public:
    DefaultFramebuffer(rx::Renderer *Renderer, Colorbuffer *colorbuffer, DepthStencilbuffer *depthStencil);

    virtual GLenum completeness() const;
    virtual FramebufferAttachment *getAttachment(GLenum attachment) const;

  private:
    DISALLOW_COPY_AND_ASSIGN(DefaultFramebuffer);
};

}

namespace rx
{
class RenderTarget;

// TODO: place this in FramebufferD3D.h
gl::Error GetAttachmentRenderTarget(gl::FramebufferAttachment *attachment, RenderTarget **outRT);
unsigned int GetAttachmentSerial(gl::FramebufferAttachment *attachment);

}

#endif   // LIBGLESV2_FRAMEBUFFER_H_
