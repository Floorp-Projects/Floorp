/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozFramebuffer.h"

#include "GLContext.h"
#include "mozilla/gfx/Logging.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace gl {

static void
DeleteByTarget(GLContext* const gl, const GLenum target, const GLuint name)
{
    if (target == LOCAL_GL_RENDERBUFFER) {
        gl->DeleteRenderbuffer(name);
    } else {
        gl->DeleteTexture(name);
    }
}

UniquePtr<MozFramebuffer>
MozFramebuffer::Create(GLContext* const gl, const gfx::IntSize& size,
                       const uint32_t samples, const bool depthStencil)
{
    if (samples && !gl->IsSupported(GLFeature::framebuffer_multisample))
        return nullptr;

    if (uint32_t(size.width) > gl->MaxTexOrRbSize() ||
        uint32_t(size.height) > gl->MaxTexOrRbSize() ||
        samples > gl->MaxSamples())
    {
        return nullptr;
    }

    gl->MakeCurrent();

    GLContext::LocalErrorScope errorScope(*gl);

    GLenum colorTarget;
    GLuint colorName;
    if (samples) {
        colorTarget = LOCAL_GL_RENDERBUFFER;
        colorName = gl->CreateRenderbuffer();
        const ScopedBindRenderbuffer bindRB(gl, colorName);
        gl->fRenderbufferStorageMultisample(colorTarget, samples, LOCAL_GL_RGBA8,
                                            size.width, size.height);
    } else {
        colorTarget = LOCAL_GL_TEXTURE_2D;
        colorName = gl->CreateTexture();
        const ScopedBindTexture bindTex(gl, colorName);
        gl->TexParams_SetClampNoMips();
        gl->fTexImage2D(colorTarget, 0, LOCAL_GL_RGBA,
                        size.width, size.height, 0,
                        LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, nullptr);
    }

    const auto err = errorScope.GetError();
    if (err) {
        if (err != LOCAL_GL_OUT_OF_MEMORY) {
            gfxCriticalNote << "Unexpected error: " << gfx::hexa(err) << ": "
                            << GLContext::GLErrorToString(err);
        }
        DeleteByTarget(gl, colorTarget, colorName);
        return nullptr;
    }

    return CreateWith(gl, size, samples, depthStencil, colorTarget, colorName);
}

UniquePtr<MozFramebuffer>
MozFramebuffer::CreateWith(GLContext* const gl, const gfx::IntSize& size,
                           const uint32_t samples, const bool depthStencil,
                           const GLenum colorTarget, const GLuint colorName)
{
    UniquePtr<MozFramebuffer> mozFB(new MozFramebuffer(gl, size, samples, depthStencil,
                                                       colorTarget, colorName));

    const ScopedBindFramebuffer bindFB(gl, mozFB->mFB);

    if (colorTarget == LOCAL_GL_RENDERBUFFER) {
        gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                     colorTarget, colorName);
    } else {
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                  colorTarget, colorName, 0);
    }

    const auto fnAllocRB = [&](GLuint rb, GLenum format) {
        const ScopedBindRenderbuffer bindRB(gl, rb);
        if (samples) {
            gl->fRenderbufferStorageMultisample(LOCAL_GL_RENDERBUFFER, samples, format,
                                                size.width, size.height);
        } else {
            gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, format, size.width,
                                     size.height);
        }
        return rb;
    };

    if (depthStencil) {
        GLuint depthRB, stencilRB;

        {
            GLContext::LocalErrorScope errorScope(*gl);

            if (gl->IsSupported(GLFeature::packed_depth_stencil)) {
                depthRB = fnAllocRB(mozFB->mDepthRB, LOCAL_GL_DEPTH24_STENCIL8);
                stencilRB = depthRB; // Ignore unused mStencilRB.
            } else {
                depthRB   = fnAllocRB(mozFB->mDepthRB  , LOCAL_GL_DEPTH_COMPONENT24);
                stencilRB = fnAllocRB(mozFB->mStencilRB, LOCAL_GL_STENCIL_INDEX8);
            }

            const auto err = errorScope.GetError();
            if (err) {
                MOZ_ASSERT(err == LOCAL_GL_OUT_OF_MEMORY);
                return nullptr;
            }
        }

        gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                     LOCAL_GL_RENDERBUFFER, depthRB);
        gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                                     LOCAL_GL_RENDERBUFFER, stencilRB);
    }

    const auto status = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        MOZ_ASSERT(false);
        return nullptr;
    }

    return mozFB;
}

////////////////////

MozFramebuffer::MozFramebuffer(GLContext* const gl, const gfx::IntSize& size,
                               const uint32_t samples, const bool depthStencil,
                               const GLenum colorTarget, const GLuint colorName)
    : mWeakGL(gl)
    , mSize(size)
    , mSamples(samples)
    , mFB(gl->CreateFramebuffer())
    , mColorTarget(colorTarget)
    , mColorName(colorName)
    , mDepthRB(depthStencil ? gl->CreateRenderbuffer() : 0)
    , mStencilRB(depthStencil ? gl->CreateRenderbuffer() : 0)
{
    MOZ_ASSERT(mColorTarget);
    MOZ_ASSERT(mColorName);
}

MozFramebuffer::~MozFramebuffer()
{
    GLContext* const gl = mWeakGL;
    if (!gl || !gl->MakeCurrent())
        return;

    gl->DeleteFramebuffer(mFB);
    gl->DeleteRenderbuffer(mDepthRB);
    gl->DeleteRenderbuffer(mStencilRB);

    DeleteByTarget(gl, mColorTarget, mColorName);
}

} // namespace gl
} // namespace mozilla
