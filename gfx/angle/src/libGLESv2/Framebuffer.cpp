//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.cpp: Implements the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/main.h"
#include "libGLESv2/formatutils.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/Context.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/FramebufferAttachment.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/renderer/RenderTarget.h"
#include "libGLESv2/renderer/Workarounds.h"
#include "libGLESv2/renderer/d3d/TextureD3D.h"

#include "common/utilities.h"

namespace rx
{
gl::Error GetAttachmentRenderTarget(gl::FramebufferAttachment *attachment, RenderTarget **outRT)
{
    if (attachment->isTexture())
    {
        gl::Texture *texture = attachment->getTexture();
        ASSERT(texture);
        TextureD3D *textureD3D = TextureD3D::makeTextureD3D(texture->getImplementation());
        const gl::ImageIndex *index = attachment->getTextureImageIndex();
        ASSERT(index);
        return textureD3D->getRenderTarget(*index, outRT);
    }
    else
    {
        gl::Renderbuffer *renderbuffer = attachment->getRenderbuffer();
        ASSERT(renderbuffer);

        // TODO: cast to RenderbufferD3D
        *outRT = renderbuffer->getStorage()->getRenderTarget();
        return gl::Error(GL_NO_ERROR);
    }
}

// Note: RenderTarget serials should ideally be in the RenderTargets themselves.
unsigned int GetAttachmentSerial(gl::FramebufferAttachment *attachment)
{
    if (attachment->isTexture())
    {
        gl::Texture *texture = attachment->getTexture();
        ASSERT(texture);
        TextureD3D *textureD3D = TextureD3D::makeTextureD3D(texture->getImplementation());
        const gl::ImageIndex *index = attachment->getTextureImageIndex();
        ASSERT(index);
        return textureD3D->getRenderTargetSerial(*index);
    }

    gl::Renderbuffer *renderbuffer = attachment->getRenderbuffer();
    ASSERT(renderbuffer);

    // TODO: cast to RenderbufferD3D
    return renderbuffer->getStorage()->getSerial();
}

}

namespace gl
{

Framebuffer::Framebuffer(rx::Renderer *renderer, GLuint id)
    : mRenderer(renderer),
      mId(id),
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

FramebufferAttachment *Framebuffer::createAttachment(GLenum binding, GLenum type, GLuint handle, GLint level, GLint layer) const
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
        return new RenderbufferAttachment(binding, context->getRenderbuffer(handle));

      case GL_TEXTURE_2D:
        {
            Texture *texture = context->getTexture(handle);
            if (texture && texture->getTarget() == GL_TEXTURE_2D)
            {
                return new TextureAttachment(binding, texture, ImageIndex::Make2D(level));
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
                return new TextureAttachment(binding, texture, ImageIndex::MakeCube(type, level));
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
                return new TextureAttachment(binding, texture, ImageIndex::Make3D(level, layer));
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
                return new TextureAttachment(binding, texture, ImageIndex::Make2DArray(level, layer));
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
    GLenum binding = colorAttachment + GL_COLOR_ATTACHMENT0;
    mColorbuffers[colorAttachment] = createAttachment(binding, type, colorbuffer, level, layer);
}

void Framebuffer::setDepthbuffer(GLenum type, GLuint depthbuffer, GLint level, GLint layer)
{
    SafeDelete(mDepthbuffer);
    mDepthbuffer = createAttachment(GL_DEPTH_ATTACHMENT, type, depthbuffer, level, layer);
}

void Framebuffer::setStencilbuffer(GLenum type, GLuint stencilbuffer, GLint level, GLint layer)
{
    SafeDelete(mStencilbuffer);
    mStencilbuffer = createAttachment(GL_STENCIL_ATTACHMENT, type, stencilbuffer, level, layer);
}

void Framebuffer::setDepthStencilBuffer(GLenum type, GLuint depthStencilBuffer, GLint level, GLint layer)
{
    FramebufferAttachment *attachment = createAttachment(GL_DEPTH_STENCIL_ATTACHMENT, type, depthStencilBuffer, level, layer);

    SafeDelete(mDepthbuffer);
    SafeDelete(mStencilbuffer);

    // ensure this is a legitimate depth+stencil format
    if (attachment && attachment->getDepthSize() > 0 && attachment->getStencilSize() > 0)
    {
        mDepthbuffer = attachment;

        // Make a new attachment object to ensure we do not double-delete
        // See angle issue 686
        mStencilbuffer = createAttachment(GL_DEPTH_STENCIL_ATTACHMENT, type, depthStencilBuffer, level, layer);
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

FramebufferAttachment *Framebuffer::getAttachment(GLenum attachment) const
{
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15)
    {
        return getColorbuffer(attachment - GL_COLOR_ATTACHMENT0);
    }
    else
    {
        switch (attachment)
        {
          case GL_DEPTH_ATTACHMENT:
            return getDepthbuffer();
          case GL_STENCIL_ATTACHMENT:
            return getStencilbuffer();
          case GL_DEPTH_STENCIL_ATTACHMENT:
            return getDepthStencilBuffer();
          default:
            UNREACHABLE();
            return NULL;
        }
    }
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
            // TODO(geofflang): use context's texture caps
            const TextureCaps &formatCaps = mRenderer->getRendererTextureCaps().get(internalformat);
            const InternalFormat &formatInfo = GetInternalFormatInfo(internalformat);
            if (colorbuffer->isTexture())
            {
                if (!formatCaps.renderable)
                {
                    return GL_FRAMEBUFFER_UNSUPPORTED;
                }

                if (formatInfo.depthBits > 0 || formatInfo.stencilBits > 0)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                }
            }
            else
            {
                if (!formatCaps.renderable || formatInfo.depthBits > 0 || formatInfo.stencilBits > 0)
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
                    if (formatInfo.pixelBytes != colorbufferSize)
                    {
                        return GL_FRAMEBUFFER_UNSUPPORTED;
                    }
                }

                // D3D11 does not allow for overlapping RenderTargetViews, so ensure uniqueness
                for (unsigned int previousColorAttachment = 0; previousColorAttachment < colorAttachment; previousColorAttachment++)
                {
                    const FramebufferAttachment *previousAttachment = mColorbuffers[previousColorAttachment];

                    if (previousAttachment &&
                        (colorbuffer->id() == previousAttachment->id() &&
                         colorbuffer->type() == previousAttachment->type()))
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
                colorbufferSize = formatInfo.pixelBytes;
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
        // TODO(geofflang): use context's texture caps
        const TextureCaps &formatCaps = mRenderer->getRendererTextureCaps().get(internalformat);
        const InternalFormat &formatInfo = GetInternalFormatInfo(internalformat);
        if (mDepthbuffer->isTexture())
        {
            // depth texture attachments require OES/ANGLE_depth_texture
            // TODO(geofflang): use context's extensions
            if (!mRenderer->getRendererExtensions().depthTextures)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!formatCaps.renderable)
            {
                return GL_FRAMEBUFFER_UNSUPPORTED;
            }

            if (formatInfo.depthBits == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else
        {
            if (!formatCaps.renderable || formatInfo.depthBits == 0)
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
        // TODO(geofflang): use context's texture caps
        const TextureCaps &formatCaps = mRenderer->getRendererTextureCaps().get(internalformat);
        const InternalFormat &formatInfo = GetInternalFormatInfo(internalformat);
        if (mStencilbuffer->isTexture())
        {
            // texture stencil attachments come along as part
            // of OES_packed_depth_stencil + OES/ANGLE_depth_texture
            // TODO(geofflang): use context's extensions
            if (!mRenderer->getRendererExtensions().depthTextures)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!formatCaps.renderable)
            {
                return GL_FRAMEBUFFER_UNSUPPORTED;
            }

            if (formatInfo.stencilBits == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else
        {
            if (!formatCaps.renderable || formatInfo.stencilBits == 0)
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

Error Framebuffer::invalidate(const Caps &caps, GLsizei numAttachments, const GLenum *attachments)
{
    GLuint maxDimension = caps.maxRenderbufferSize;
    return invalidateSub(numAttachments, attachments, 0, 0, maxDimension, maxDimension);
}

Error Framebuffer::invalidateSub(GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height)
{
    ASSERT(completeness() == GL_FRAMEBUFFER_COMPLETE);
    for (GLsizei attachIndex = 0; attachIndex < numAttachments; ++attachIndex)
    {
        GLenum attachmentTarget = attachments[attachIndex];

        FramebufferAttachment *attachment = (attachmentTarget == GL_DEPTH_STENCIL_ATTACHMENT) ? getDepthOrStencilbuffer()
                                                                                              : getAttachment(attachmentTarget);

        if (attachment)
        {
            rx::RenderTarget *renderTarget = NULL;
            Error error = rx::GetAttachmentRenderTarget(attachment, &renderTarget);
            if (error.isError())
            {
                return error;
            }

            renderTarget->invalidate(x, y, width, height);
        }
    }

    return Error(GL_NO_ERROR);
}

DefaultFramebuffer::DefaultFramebuffer(rx::Renderer *renderer, Colorbuffer *colorbuffer, DepthStencilbuffer *depthStencil)
    : Framebuffer(renderer, 0)
{
    Renderbuffer *colorRenderbuffer = new Renderbuffer(0, colorbuffer);
    mColorbuffers[0] = new RenderbufferAttachment(GL_BACK, colorRenderbuffer);

    GLenum depthStencilActualFormat = depthStencil->getActualFormat();
    const gl::InternalFormat &depthStencilFormatInfo = GetInternalFormatInfo(depthStencilActualFormat);

    if (depthStencilFormatInfo.depthBits != 0 || depthStencilFormatInfo.stencilBits != 0)
    {
        Renderbuffer *depthStencilBuffer = new Renderbuffer(0, depthStencil);

        // Make a new attachment objects to ensure we do not double-delete
        // See angle issue 686
        mDepthbuffer = (depthStencilFormatInfo.depthBits != 0 ? new RenderbufferAttachment(GL_DEPTH_ATTACHMENT, depthStencilBuffer) : NULL);
        mStencilbuffer = (depthStencilFormatInfo.stencilBits != 0 ? new RenderbufferAttachment(GL_STENCIL_ATTACHMENT, depthStencilBuffer) : NULL);
    }
    else
    {
        // This method transfers ownership, so delete the unused storage if we don't keep it.
        SafeDelete(depthStencil);
    }

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

ColorbufferInfo Framebuffer::getColorbuffersForRender() const
{
    ColorbufferInfo colorbuffersForRender;

    for (size_t colorAttachment = 0; colorAttachment < IMPLEMENTATION_MAX_DRAW_BUFFERS; ++colorAttachment)
    {
        GLenum drawBufferState = mDrawBufferStates[colorAttachment];
        FramebufferAttachment *colorbuffer = mColorbuffers[colorAttachment];

        if (colorbuffer != NULL && drawBufferState != GL_NONE)
        {
            ASSERT(drawBufferState == GL_BACK || drawBufferState == (GL_COLOR_ATTACHMENT0_EXT + colorAttachment));
            colorbuffersForRender.push_back(colorbuffer);
        }
        else if (!mRenderer->getWorkarounds().mrtPerfWorkaround)
        {
            colorbuffersForRender.push_back(NULL);
        }
    }

    return colorbuffersForRender;
}

GLenum DefaultFramebuffer::completeness() const
{
    // The default framebuffer *must* always be complete, though it may not be
    // subject to the same rules as application FBOs. ie, it could have 0x0 size.
    return GL_FRAMEBUFFER_COMPLETE;
}

FramebufferAttachment *DefaultFramebuffer::getAttachment(GLenum attachment) const
{
    switch (attachment)
    {
      case GL_COLOR:
      case GL_BACK:
        return getColorbuffer(0);
      case GL_DEPTH:
        return getDepthbuffer();
      case GL_STENCIL:
        return getStencilbuffer();
      case GL_DEPTH_STENCIL:
        return getDepthStencilBuffer();
      default:
        UNREACHABLE();
        return NULL;
    }
}

}
