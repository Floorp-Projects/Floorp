//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.cpp: Implements the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#include "libGLESv2/Framebuffer.h"

#include "libGLESv2/main.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/utilities.h"

namespace gl
{

Framebuffer::Framebuffer()
{
    mColorbufferType = GL_NONE;
    mDepthbufferType = GL_NONE;
    mStencilbufferType = GL_NONE;
}

Framebuffer::~Framebuffer()
{
    mColorbufferPointer.set(NULL);
    mDepthbufferPointer.set(NULL);
    mStencilbufferPointer.set(NULL);
}

Renderbuffer *Framebuffer::lookupRenderbuffer(GLenum type, GLuint handle) const
{
    gl::Context *context = gl::getContext();
    Renderbuffer *buffer = NULL;

    if (type == GL_NONE)
    {
        buffer = NULL;
    }
    else if (type == GL_RENDERBUFFER)
    {
        buffer = context->getRenderbuffer(handle);
    }
    else if (IsTextureTarget(type))
    {
        buffer = context->getTexture(handle)->getColorbuffer(type);
    }
    else
    {
        UNREACHABLE();
    }

    return buffer;
}

void Framebuffer::setColorbuffer(GLenum type, GLuint colorbuffer)
{
    mColorbufferType = type;
    mColorbufferPointer.set(lookupRenderbuffer(type, colorbuffer));
}

void Framebuffer::setDepthbuffer(GLenum type, GLuint depthbuffer)
{
    mDepthbufferType = type;
    mDepthbufferPointer.set(lookupRenderbuffer(type, depthbuffer));
}

void Framebuffer::setStencilbuffer(GLenum type, GLuint stencilbuffer)
{
    mStencilbufferType = type;
    mStencilbufferPointer.set(lookupRenderbuffer(type, stencilbuffer));
}

void Framebuffer::detachTexture(GLuint texture)
{
    if (mColorbufferPointer.id() == texture && IsTextureTarget(mColorbufferType))
    {
        mColorbufferType = GL_NONE;
        mColorbufferPointer.set(NULL);
    }

    if (mDepthbufferPointer.id() == texture && IsTextureTarget(mDepthbufferType))
    {
        mDepthbufferType = GL_NONE;
        mDepthbufferPointer.set(NULL);
    }

    if (mStencilbufferPointer.id() == texture && IsTextureTarget(mStencilbufferType))
    {
        mStencilbufferType = GL_NONE;
        mStencilbufferPointer.set(NULL);
    }
}

void Framebuffer::detachRenderbuffer(GLuint renderbuffer)
{
    if (mColorbufferPointer.id() == renderbuffer && mColorbufferType == GL_RENDERBUFFER)
    {
        mColorbufferType = GL_NONE;
        mColorbufferPointer.set(NULL);
    }

    if (mDepthbufferPointer.id() == renderbuffer && mDepthbufferType == GL_RENDERBUFFER)
    {
        mDepthbufferType = GL_NONE;
        mDepthbufferPointer.set(NULL);
    }

    if (mStencilbufferPointer.id() == renderbuffer && mStencilbufferType == GL_RENDERBUFFER)
    {
        mStencilbufferType = GL_NONE;
        mStencilbufferPointer.set(NULL);
    }
}

unsigned int Framebuffer::getRenderTargetSerial()
{
    Renderbuffer *colorbuffer = mColorbufferPointer.get();

    if (colorbuffer)
    {
        return colorbuffer->getSerial();
    }

    return 0;
}

IDirect3DSurface9 *Framebuffer::getRenderTarget()
{
    Renderbuffer *colorbuffer = mColorbufferPointer.get();

    if (colorbuffer)
    {
        return colorbuffer->getRenderTarget();
    }

    return NULL;
}

unsigned int Framebuffer::getDepthbufferSerial()
{
    Renderbuffer *depthbuffer = mDepthbufferPointer.get();

    if (depthbuffer)
    {
        return depthbuffer->getSerial();
    }

    return 0;
}

Colorbuffer *Framebuffer::getColorbuffer()
{
    Renderbuffer *rb = mColorbufferPointer.get();

    if (rb != NULL && rb->isColorbuffer())
    {
        return static_cast<Colorbuffer*>(rb->getStorage());
    }
    else
    {
        return NULL;
    }
}

DepthStencilbuffer *Framebuffer::getDepthbuffer()
{
    Renderbuffer *rb = mDepthbufferPointer.get();

    if (rb != NULL && rb->isDepthbuffer())
    {
        return static_cast<DepthStencilbuffer*>(rb->getStorage());
    }
    else
    {
        return NULL;
    }
}

DepthStencilbuffer *Framebuffer::getStencilbuffer()
{
    Renderbuffer *rb = mStencilbufferPointer.get();

    if (rb != NULL && rb->isStencilbuffer())
    {
        return static_cast<DepthStencilbuffer*>(rb->getStorage());
    }
    else
    {
        return NULL;
    }
}

GLenum Framebuffer::getColorbufferType()
{
    return mColorbufferType;
}

GLenum Framebuffer::getDepthbufferType()
{
    return mDepthbufferType;
}

GLenum Framebuffer::getStencilbufferType()
{
    return mStencilbufferType;
}

GLuint Framebuffer::getColorbufferHandle()
{
    return mColorbufferPointer.id();
}

GLuint Framebuffer::getDepthbufferHandle()
{
    return mDepthbufferPointer.id();
}

GLuint Framebuffer::getStencilbufferHandle()
{
    return mStencilbufferPointer.id();
}

GLenum Framebuffer::completeness()
{
    int width = 0;
    int height = 0;

    if (mColorbufferType != GL_NONE)
    {
        Colorbuffer *colorbuffer = getColorbuffer();

        if (!colorbuffer)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (colorbuffer->getWidth() == 0 || colorbuffer->getHeight() == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        width = colorbuffer->getWidth();
        height = colorbuffer->getHeight();
    }
    else
    {
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    }

    DepthStencilbuffer *depthbuffer = NULL;
    DepthStencilbuffer *stencilbuffer = NULL;

    if (mDepthbufferType != GL_NONE)
    {
        depthbuffer = getDepthbuffer();

        if (!depthbuffer)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (depthbuffer->getWidth() == 0 || depthbuffer->getHeight() == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (width == 0)
        {
            width = depthbuffer->getWidth();
            height = depthbuffer->getHeight();
        }
        else if (width != depthbuffer->getWidth() || height != depthbuffer->getHeight())
        {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
    }

    if (mStencilbufferType != GL_NONE)
    {
        stencilbuffer = getStencilbuffer();

        if (!stencilbuffer)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (stencilbuffer->getWidth() == 0 || stencilbuffer->getHeight() == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (width == 0)
        {
            width = stencilbuffer->getWidth();
            height = stencilbuffer->getHeight();
        }
        else if (width != stencilbuffer->getWidth() || height != stencilbuffer->getHeight())
        {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
    }

    if (mDepthbufferType == GL_RENDERBUFFER && mStencilbufferType == GL_RENDERBUFFER)
    {
        if (depthbuffer->getFormat() != GL_DEPTH24_STENCIL8_OES ||
            stencilbuffer->getFormat() != GL_DEPTH24_STENCIL8_OES ||
            depthbuffer->getSerial() != stencilbuffer->getSerial())
        {
            return GL_FRAMEBUFFER_UNSUPPORTED;
        }
    }

    return GL_FRAMEBUFFER_COMPLETE;
}

DefaultFramebuffer::DefaultFramebuffer(Colorbuffer *color, DepthStencilbuffer *depthStencil)
{
    mColorbufferType = GL_RENDERBUFFER;
    mDepthbufferType = GL_RENDERBUFFER;
    mStencilbufferType = GL_RENDERBUFFER;

    mColorbufferPointer.set(new Renderbuffer(0, color));

    Renderbuffer *depthStencilRenderbuffer = new Renderbuffer(0, depthStencil);
    mDepthbufferPointer.set(depthStencilRenderbuffer);
    mStencilbufferPointer.set(depthStencilRenderbuffer);
}

GLenum DefaultFramebuffer::completeness()
{
    return GL_FRAMEBUFFER_COMPLETE;
}

}
