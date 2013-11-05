/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::gl;

static GLenum
DepthStencilDepthFormat(GLContext* gl) {
    // We might not be able to get 24-bit, so let's pretend!
    if (gl->IsGLES2() && !gl->IsExtensionSupported(gl::GLContext::OES_depth24))
        return LOCAL_GL_DEPTH_COMPONENT16;
    
    return LOCAL_GL_DEPTH_COMPONENT24;
}

static bool
SupportsDepthStencil(GLContext* gl) {
    return gl->IsExtensionSupported(GLContext::EXT_packed_depth_stencil) ||
           gl->IsExtensionSupported(GLContext::OES_packed_depth_stencil);
}

static bool
NeedsDepthStencilEmu(GLContext* gl, GLenum internalFormat) {
    MOZ_ASSERT(internalFormat != LOCAL_GL_DEPTH_STENCIL);
    if (internalFormat != LOCAL_GL_DEPTH24_STENCIL8)
        return false;

    return !SupportsDepthStencil(gl);
}

JSObject*
WebGLRenderbuffer::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope) {
    return dom::WebGLRenderbufferBinding::Wrap(cx, scope, this);
}

WebGLRenderbuffer::WebGLRenderbuffer(WebGLContext *context)
    : WebGLContextBoundObject(context)
    , mPrimaryRB(0)
    , mSecondaryRB(0)
    , mInternalFormat(0)
    , mInternalFormatForGL(0)
    , mHasEverBeenBound(false)
    , mImageDataStatus(WebGLImageDataStatus::NoImageData)
{
    SetIsDOMBinding();
    mContext->MakeContextCurrent();

    mContext->gl->fGenRenderbuffers(1, &mPrimaryRB);
    if (!SupportsDepthStencil(mContext->gl))
        mContext->gl->fGenRenderbuffers(1, &mSecondaryRB);

    mContext->mRenderbuffers.insertBack(this);
}

void
WebGLRenderbuffer::Delete() {
    mContext->MakeContextCurrent();

    mContext->gl->fDeleteRenderbuffers(1, &mPrimaryRB);
    if (mSecondaryRB)
        mContext->gl->fDeleteRenderbuffers(1, &mSecondaryRB);

    LinkedListElement<WebGLRenderbuffer>::removeFrom(mContext->mRenderbuffers);
}

int64_t
WebGLRenderbuffer::MemoryUsage() const {
    int64_t pixels = int64_t(Width()) * int64_t(Height());

    GLenum primaryFormat = InternalFormatForGL();
    // If there is no defined format, we're not taking up any memory
    if (!primaryFormat) {
        return 0;
    }

    int64_t secondarySize = 0;
    if (mSecondaryRB) {
        if (NeedsDepthStencilEmu(mContext->gl, primaryFormat)) {
            primaryFormat = DepthStencilDepthFormat(mContext->gl);
            secondarySize = 1*pixels; // STENCIL_INDEX8
        } else {
            secondarySize = 1*1*2; // 1x1xRGBA4
        }
    }

    int64_t primarySize = 0;
    switch (primaryFormat) {
        case LOCAL_GL_STENCIL_INDEX8:
            primarySize = 1 * pixels;
            break;
        case LOCAL_GL_RGBA4:
        case LOCAL_GL_RGB5_A1:
        case LOCAL_GL_RGB565:
        case LOCAL_GL_DEPTH_COMPONENT16:
            primarySize = 2 * pixels;
            break;
        case LOCAL_GL_RGB8:
        case LOCAL_GL_DEPTH_COMPONENT24:
            primarySize = 3*pixels;
            break;
        case LOCAL_GL_RGBA8:
        case LOCAL_GL_SRGB8_ALPHA8_EXT:
        case LOCAL_GL_DEPTH24_STENCIL8:
        case LOCAL_GL_DEPTH_COMPONENT32:
            primarySize = 4*pixels;
            break;
        default:
            MOZ_ASSERT(false, "Unknown `primaryFormat`.");
            break;
    }

    return primarySize + secondarySize;
}

void
WebGLRenderbuffer::BindRenderbuffer() const {
    /* Do this explicitly here, since the meaning changes for depth-stencil emu.
     * Under normal circumstances, there's only one RB: `mPrimaryRB`.
     * `mSecondaryRB` is used when we have to pretend that the renderbuffer is
     * DEPTH_STENCIL, when it's actually one DEPTH buffer `mPrimaryRB` and one
     * STENCIL buffer `mSecondaryRB`.
     * 
     * In the DEPTH_STENCIL emulation case, we're actually juggling two RBs, but
     * we can only bind one of them at a time. We choose to unconditionally bind
     * the depth RB. When we need to ask about the stencil buffer (say, how many
     * stencil bits we have), we temporarily bind the stencil RB, so that it
     * looks like we're just asking the question of a combined DEPTH_STENCIL
     * buffer.
     */
    mContext->gl->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, mPrimaryRB);
}

void
WebGLRenderbuffer::RenderbufferStorage(GLenum internalFormat, GLsizei width, GLsizei height) const {
    GLContext* gl = mContext->gl;

    GLenum primaryFormat = internalFormat;
    GLenum secondaryFormat = 0;

    if (NeedsDepthStencilEmu(mContext->gl, primaryFormat)) {
        primaryFormat = DepthStencilDepthFormat(gl);
        secondaryFormat = LOCAL_GL_STENCIL_INDEX8;
    }

    gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, primaryFormat, width, height);
    
    if (!mSecondaryRB) {
        MOZ_ASSERT(!secondaryFormat);
        return;
    }
    // We can't leave the secondary RB unspecified either, since we should
    // handle the case where we attach a non-depth-stencil RB to a
    // depth-stencil attachment point, or attach this depth-stencil RB to a
    // non-depth-stencil attachment point.
    ScopedBindRenderbuffer autoRB(gl, mSecondaryRB);
    if (secondaryFormat) {
        gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, secondaryFormat, width, height);
    } else {
        gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, LOCAL_GL_RGBA4, 1, 1);
    }
}

void
WebGLRenderbuffer::FramebufferRenderbuffer(GLenum attachment) const {
    GLContext* gl = mContext->gl;
    if (attachment != LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, attachment, LOCAL_GL_RENDERBUFFER, mPrimaryRB);
        return;
    }

    GLuint stencilRB = mPrimaryRB;
    if (NeedsDepthStencilEmu(mContext->gl, InternalFormatForGL())) {
        printf_stderr("DEV-ONLY: Using secondary buffer to emulate DepthStencil.\n");
        MOZ_ASSERT(mSecondaryRB);
        stencilRB = mSecondaryRB;
    }
    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,   LOCAL_GL_RENDERBUFFER, mPrimaryRB);
    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT, LOCAL_GL_RENDERBUFFER, stencilRB);
}

GLint
WebGLRenderbuffer::GetRenderbufferParameter(GLenum target, GLenum pname) const {
    GLContext* gl = mContext->gl;

    switch (pname) {
        case LOCAL_GL_RENDERBUFFER_STENCIL_SIZE: {
            if (NeedsDepthStencilEmu(mContext->gl, InternalFormatForGL())) {
                if (gl->WorkAroundDriverBugs() &&
                    gl->Renderer() == GLContext::RendererTegra)
                {
                    return 8;
                }

                ScopedBindRenderbuffer autoRB(gl, mSecondaryRB);

                GLint i = 0;
                gl->fGetRenderbufferParameteriv(target, pname, &i);
                return i;
            }
            // Fall through otherwise.
        }
        case LOCAL_GL_RENDERBUFFER_WIDTH:
        case LOCAL_GL_RENDERBUFFER_HEIGHT:
        case LOCAL_GL_RENDERBUFFER_RED_SIZE:
        case LOCAL_GL_RENDERBUFFER_GREEN_SIZE:
        case LOCAL_GL_RENDERBUFFER_BLUE_SIZE:
        case LOCAL_GL_RENDERBUFFER_ALPHA_SIZE:
        case LOCAL_GL_RENDERBUFFER_DEPTH_SIZE: {
            GLint i = 0;
            gl->fGetRenderbufferParameteriv(target, pname, &i);
            return i;
        }
    }

    MOZ_ASSERT(false, "This function should only be called with valid `pname`.");
    return 0;
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLRenderbuffer)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLRenderbuffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLRenderbuffer, Release)
