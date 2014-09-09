#include "precompiled.h"
//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.cpp: Implements the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#include "libGLESv2/Framebuffer.h"

#include "libGLESv2/main.h"
#include "common/utilities.h"
#include "libGLESv2/formatutils.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/Context.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/FramebufferAttachment.h"

namespace gl
{

Framebuffer::Framebuffer(rx::Renderer *renderer)
    : mRenderer(renderer),
      mReadBufferState(GL_COLOR_ATTACHMENT0_EXT),
      mDepthbuffer(NULL),
      mStencilbuffer(NULL)
{
    for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        mColorbuffers[colorAttachment] = NULL;
        mDrawBufferStates[colorAttachment] = GL_NONE;
    }
    mDrawBufferStates[0] = GL_COLOR_ATTACHMENT0_EXT;
}

Framebuffer::~Framebuffer()
{
    for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        SafeDelete(mColorbuffers[colorAttachment]);
    }
    SafeDelete(mDepthbuffer);
    SafeDelete(mStencilbuffer);
}

FramebufferAttachment *Framebuffer::createAttachment(GLenum type, GLuint handle, GLint level, GLint layer) const
{
    if (handle == 0)
    {
        return NULL;
    }

    gl::Context *context = gl::getContext();

    switch (type)
    {
      case GL_NONE:
        return NULL;

      case GL_RENDERBUFFER:
        return new RenderbufferAttachment(context->getRenderbuffer(handle));

      case GL_TEXTURE_2D:
        {
            Texture *texture = context->getTexture(handle);
            if (texture && texture->getTarget() == GL_TEXTURE_2D)
            {
                Texture2D *tex2D = static_cast<Texture2D*>(texture);
                return new Texture2DAttachment(tex2D, level);
            }
            else
            {
                return NULL;
            }
        }

      case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        {
            Texture *texture = context->getTexture(handle);
            if (texture && texture->getTarget() == GL_TEXTURE_CUBE_MAP)
            {
                TextureCubeMap *texCube = static_cast<TextureCubeMap*>(texture);
                return new TextureCubeMapAttachment(texCube, type, level);
            }
            else
            {
                return NULL;
            }
        }

      case GL_TEXTURE_3D:
        {
            Texture *texture = context->getTexture(handle);
            if (texture && texture->getTarget() == GL_TEXTURE_3D)
            {
                Texture3D *tex3D = static_cast<Texture3D*>(texture);
                return new Texture3DAttachment(tex3D, level, layer);
            }
            else
            {
                return NULL;
            }
        }

      case GL_TEXTURE_2D_ARRAY:
        {
            Texture *texture = context->getTexture(handle);
            if (texture && texture->getTarget() == GL_TEXTURE_2D_ARRAY)
            {
                Texture2DArray *tex2DArray = static_cast<Texture2DArray*>(texture);
                return new Texture2DArrayAttachment(tex2DArray, level, layer);
            }
            else
            {
                return NULL;
            }
        }

      default:
        UNREACHABLE();
        return NULL;
    }
}

void Framebuffer::setColorbuffer(unsigned int colorAttachment, GLenum type, GLuint colorbuffer, GLint level, GLint layer)
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);
    SafeDelete(mColorbuffers[colorAttachment]);
    mColorbuffers[colorAttachment] = createAttachment(type, colorbuffer, level, layer);
}

void Framebuffer::setDepthbuffer(GLenum type, GLuint depthbuffer, GLint level, GLint layer)
{
    SafeDelete(mDepthbuffer);
    mDepthbuffer = createAttachment(type, depthbuffer, level, layer);
}

void Framebuffer::setStencilbuffer(GLenum type, GLuint stencilbuffer, GLint level, GLint layer)
{
    SafeDelete(mStencilbuffer);
    mStencilbuffer = createAttachment(type, stencilbuffer, level, layer);
}

void Framebuffer::setDepthStencilBuffer(GLenum type, GLuint depthStencilBuffer, GLint level, GLint layer)
{
    FramebufferAttachment *attachment = createAttachment(type, depthStencilBuffer, level, layer);

    SafeDelete(mDepthbuffer);
    SafeDelete(mStencilbuffer);

    // ensure this is a legitimate depth+stencil format
    if (attachment && attachment->getDepthSize() > 0 && attachment->getStencilSize() > 0)
    {
        mDepthbuffer = attachment;

        // Make a new attachment object to ensure we do not double-delete
        // See angle issue 686
        mStencilbuffer = createAttachment(type, depthStencilBuffer, level, layer);
    }
}

void Framebuffer::detachTexture(GLuint textureId)
{
    for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        FramebufferAttachment *attachment = mColorbuffers[colorAttachment];

        if (attachment && attachment->isTextureWithId(textureId))
        {
            SafeDelete(mColorbuffers[colorAttachment]);
        }
    }

    if (mDepthbuffer && mDepthbuffer->isTextureWithId(textureId))
    {
        SafeDelete(mDepthbuffer);
    }

    if (mStencilbuffer && mStencilbuffer->isTextureWithId(textureId))
    {
        SafeDelete(mStencilbuffer);
    }
}

void Framebuffer::detachRenderbuffer(GLuint renderbufferId)
{
    for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        FramebufferAttachment *attachment = mColorbuffers[colorAttachment];

        if (attachment && attachment->isRenderbufferWithId(renderbufferId))
        {
            SafeDelete(mColorbuffers[colorAttachment]);
        }
    }

    if (mDepthbuffer && mDepthbuffer->isRenderbufferWithId(renderbufferId))
    {
        SafeDelete(mDepthbuffer);
    }

    if (mStencilbuffer && mStencilbuffer->isRenderbufferWithId(renderbufferId))
    {
        SafeDelete(mStencilbuffer);
    }
}

FramebufferAttachment *Framebuffer::getColorbuffer(unsigned int colorAttachment) const
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);
    return mColorbuffers[colorAttachment];
}

FramebufferAttachment *Framebuffer::getDepthbuffer() const
{
    return mDepthbuffer;
}

FramebufferAttachment *Framebuffer::getStencilbuffer() const
{
    return mStencilbuffer;
}

FramebufferAttachment *Framebuffer::getDepthStencilBuffer() const
{
    return (hasValidDepthStencil() ? mDepthbuffer : NULL);
}

FramebufferAttachment *Framebuffer::getDepthOrStencilbuffer() const
{
    FramebufferAttachment *depthstencilbuffer = mDepthbuffer;
    
    if (!depthstencilbuffer)
    {
        depthstencilbuffer = mStencilbuffer;
    }

    return depthstencilbuffer;
}

FramebufferAttachment *Framebuffer::getReadColorbuffer() const
{
    // Will require more logic if glReadBuffers is supported
    return mColorbuffers[0];
}

GLenum Framebuffer::getReadColorbufferType() const
{
    // Will require more logic if glReadBuffers is supported
    return (mColorbuffers[0] ? mColorbuffers[0]->type() : GL_NONE);
}

FramebufferAttachment *Framebuffer::getFirstColorbuffer() const
{
    for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        if (mColorbuffers[colorAttachment])
        {
            return mColorbuffers[colorAttachment];
        }
    }

    return NULL;
}

GLenum Framebuffer::getColorbufferType(unsigned int colorAttachment) const
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);
    return (mColorbuffers[colorAttachment] ? mColorbuffers[colorAttachment]->type() : GL_NONE);
}

GLenum Framebuffer::getDepthbufferType() const
{
    return (mDepthbuffer ? mDepthbuffer->type() : GL_NONE);
}

GLenum Framebuffer::getStencilbufferType() const
{
    return (mStencilbuffer ? mStencilbuffer->type() : GL_NONE);
}

GLenum Framebuffer::getDepthStencilbufferType() const
{
    return (hasValidDepthStencil() ? mDepthbuffer->type() : GL_NONE);
}

GLuint Framebuffer::getColorbufferHandle(unsigned int colorAttachment) const
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);
    return (mColorbuffers[colorAttachment] ? mColorbuffers[colorAttachment]->id() : 0);
}

GLuint Framebuffer::getDepthbufferHandle() const
{
    return (mDepthbuffer ? mDepthbuffer->id() : 0);
}

GLuint Framebuffer::getStencilbufferHandle() const
{
    return (mStencilbuffer ? mStencilbuffer->id() : 0);
}

GLuint Framebuffer::getDepthStencilbufferHandle() const
{
    return (hasValidDepthStencil() ? mDepthbuffer->id() : 0);
}

GLint Framebuffer::getColorbufferMipLevel(unsigned int colorAttachment) const
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);
    return (mColorbuffers[colorAttachment] ? mColorbuffers[colorAttachment]->mipLevel() : 0);
}

GLint Framebuffer::getDepthbufferMipLevel() const
{
    return (mDepthbuffer ? mDepthbuffer->mipLevel() : 0);
}

GLint Framebuffer::getStencilbufferMipLevel() const
{
    return (mStencilbuffer ? mStencilbuffer->mipLevel() : 0);
}

GLint Framebuffer::getDepthStencilbufferMipLevel() const
{
    return (hasValidDepthStencil() ? mDepthbuffer->mipLevel() : 0);
}

GLint Framebuffer::getColorbufferLayer(unsigned int colorAttachment) const
{
    ASSERT(colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS);
    return (mColorbuffers[colorAttachment] ? mColorbuffers[colorAttachment]->layer() : 0);
}

GLint Framebuffer::getDepthbufferLayer() const
{
    return (mDepthbuffer ? mDepthbuffer->layer() : 0);
}

GLint Framebuffer::getStencilbufferLayer() const
{
    return (mStencilbuffer ? mStencilbuffer->layer() : 0);
}

GLint Framebuffer::getDepthStencilbufferLayer() const
{
    return (hasValidDepthStencil() ? mDepthbuffer->layer() : 0);
}

GLenum Framebuffer::getDrawBufferState(unsigned int colorAttachment) const
{
    return mDrawBufferStates[colorAttachment];
}

void Framebuffer::setDrawBufferState(unsigned int colorAttachment, GLenum drawBuffer)
{
    mDrawBufferStates[colorAttachment] = drawBuffer;
}

bool Framebuffer::isEnabledColorAttachment(unsigned int colorAttachment) const
{
    return (mColorbuffers[colorAttachment] && mDrawBufferStates[colorAttachment] != GL_NONE);
}

bool Framebuffer::hasEnabledColorAttachment() const
{
    for (unsigned int colorAttachment = 0; colorAttachment < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        if (isEnabledColorAttachment(colorAttachment))
        {
            return true;
        }
    }

    return false;
}

bool Framebuffer::hasStencil() const
{
    return (mStencilbuffer && mStencilbuffer->getStencilSize() > 0);
}

bool Framebuffer::usingExtendedDrawBuffers() const
{
    for (unsigned int colorAttachment = 1; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        if (isEnabledColorAttachment(colorAttachment))
        {
            return true;
        }
    }

    return false;
}

GLenum Framebuffer::completeness() const
{
    int width = 0;
    int height = 0;
    unsigned int colorbufferSize = 0;
    int samples = -1;
    bool missingAttachment = true;
    GLuint clientVersion = mRenderer->getCurrentClientVersion();

    for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
    {
        const FramebufferAttachment *colorbuffer = mColorbuffers[colorAttachment];

        if (colorbuffer)
        {
            if (colorbuffer->getWidth() == 0 || colorbuffer->getHeight() == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            GLenum internalformat = colorbuffer->getInternalFormat();
            const TextureCaps &formatCaps = mRenderer->getCaps().textureCaps.get(internalformat);
            if (colorbuffer->isTexture())
            {
                if (!formatCaps.colorRendering)
                {
                    return GL_FRAMEBUFFER_UNSUPPORTED;
                }

                if (gl::GetDepthBits(internalformat) > 0 ||
                    gl::GetStencilBits(internalformat) > 0)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                }
            }
            else
            {
                if (!formatCaps.colorRendering)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                }
            }

            if (!missingAttachment)
            {
                // all color attachments must have the same width and height
                if (colorbuffer->getWidth() != width || colorbuffer->getHeight() != height)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
                }

                // APPLE_framebuffer_multisample, which EXT_draw_buffers refers to, requires that
                // all color attachments have the same number of samples for the FBO to be complete.
                if (colorbuffer->getSamples() != samples)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT;
                }

                // in GLES 2.0, all color attachments attachments must have the same number of bitplanes
                // in GLES 3.0, there is no such restriction
                if (clientVersion < 3)
                {
                    if (gl::GetPixelBytes(colorbuffer->getInternalFormat()) != colorbufferSize)
                    {
                        return GL_FRAMEBUFFER_UNSUPPORTED;
                    }
                }

                // D3D11 does not allow for overlapping RenderTargetViews, so ensure uniqueness
                for (unsigned int previousColorAttachment = 0; previousColorAttachment < colorAttachment; previousColorAttachment++)
                {
                    if (colorbuffer->id() == getColorbufferHandle(previousColorAttachment) &&
                        colorbuffer->type() == getColorbufferType(previousColorAttachment))
                    {
                        return GL_FRAMEBUFFER_UNSUPPORTED;
                    }
                }
            }
            else
            {
                width = colorbuffer->getWidth();
                height = colorbuffer->getHeight();
                samples = colorbuffer->getSamples();
                colorbufferSize = gl::GetPixelBytes(colorbuffer->getInternalFormat());
                missingAttachment = false;
            }
        }
    }

    if (mDepthbuffer)
    {
        if (mDepthbuffer->getWidth() == 0 || mDepthbuffer->getHeight() == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        GLenum internalformat = mDepthbuffer->getInternalFormat();
        const TextureCaps &formatCaps = mRenderer->getCaps().textureCaps.get(internalformat);
        if (mDepthbuffer->isTexture())
        {
            GLenum internalformat = mDepthbuffer->getInternalFormat();

            // depth texture attachments require OES/ANGLE_depth_texture
            if (!mRenderer->getCaps().extensions.depthTextures)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!formatCaps.depthRendering)
            {
                return GL_FRAMEBUFFER_UNSUPPORTED;
            }

            if (gl::GetDepthBits(internalformat) == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else
        {
            if (!formatCaps.depthRendering)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }

        if (missingAttachment)
        {
            width = mDepthbuffer->getWidth();
            height = mDepthbuffer->getHeight();
            samples = mDepthbuffer->getSamples();
            missingAttachment = false;
        }
        else if (width != mDepthbuffer->getWidth() || height != mDepthbuffer->getHeight())
        {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
        else if (samples != mDepthbuffer->getSamples())
        {
            return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE;
        }
    }

    if (mStencilbuffer)
    {
        if (mStencilbuffer->getWidth() == 0 || mStencilbuffer->getHeight() == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        GLenum internalformat = mStencilbuffer->getInternalFormat();
        const TextureCaps &formatCaps = mRenderer->getCaps().textureCaps.get(internalformat);
        if (mStencilbuffer->isTexture())
        {
            GLenum internalformat = mStencilbuffer->getInternalFormat();

            // texture stencil attachments come along as part
            // of OES_packed_depth_stencil + OES/ANGLE_depth_texture
            if (!mRenderer->getCaps().extensions.depthTextures)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!formatCaps.stencilRendering)
            {
                return GL_FRAMEBUFFER_UNSUPPORTED;
            }

            if (gl::GetStencilBits(internalformat) == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else
        {
            if (!formatCaps.stencilRendering)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }

        if (missingAttachment)
        {
            width = mStencilbuffer->getWidth();
            height = mStencilbuffer->getHeight();
            samples = mStencilbuffer->getSamples();
            missingAttachment = false;
        }
        else if (width != mStencilbuffer->getWidth() || height != mStencilbuffer->getHeight())
        {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
        else if (samples != mStencilbuffer->getSamples())
        {
            return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE;
        }
    }

    // if we have both a depth and stencil buffer, they must refer to the same object
    // since we only support packed_depth_stencil and not separate depth and stencil
    if (mDepthbuffer && mStencilbuffer && !hasValidDepthStencil())
    {
        return GL_FRAMEBUFFER_UNSUPPORTED;
    }

    // we need to have at least one attachment to be complete
    if (missingAttachment)
    {
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    }

    return GL_FRAMEBUFFER_COMPLETE;
}

DefaultFramebuffer::DefaultFramebuffer(rx::Renderer *renderer, Colorbuffer *colorbuffer, DepthStencilbuffer *depthStencil)
    : Framebuffer(renderer)
{
    Renderbuffer *colorRenderbuffer = new Renderbuffer(0, colorbuffer);
    mColorbuffers[0] = new RenderbufferAttachment(colorRenderbuffer);

    Renderbuffer *depthStencilBuffer = new Renderbuffer(0, depthStencil);

    // Make a new attachment objects to ensure we do not double-delete
    // See angle issue 686
    mDepthbuffer = (depthStencilBuffer->getDepthSize() != 0 ? new RenderbufferAttachment(depthStencilBuffer) : NULL);
    mStencilbuffer = (depthStencilBuffer->getStencilSize() != 0 ? new RenderbufferAttachment(depthStencilBuffer) : NULL);

    mDrawBufferStates[0] = GL_BACK;
    mReadBufferState = GL_BACK;
}

int Framebuffer::getSamples() const
{
    if (completeness() == GL_FRAMEBUFFER_COMPLETE)
    {
        // for a complete framebuffer, all attachments must have the same sample count
        // in this case return the first nonzero sample size
        for (unsigned int colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; colorAttachment++)
        {
            if (mColorbuffers[colorAttachment])
            {
                return mColorbuffers[colorAttachment]->getSamples();
            }
        }
    }

    return 0;
}

bool Framebuffer::hasValidDepthStencil() const
{
    // A valid depth-stencil attachment has the same resource bound to both the
    // depth and stencil attachment points.
    return (mDepthbuffer && mStencilbuffer &&
            mDepthbuffer->type() == mStencilbuffer->type() &&
            mDepthbuffer->id() == mStencilbuffer->id());
}

GLenum DefaultFramebuffer::completeness() const
{
    // The default framebuffer *must* always be complete, though it may not be
    // subject to the same rules as application FBOs. ie, it could have 0x0 size.
    return GL_FRAMEBUFFER_COMPLETE;
}

}
