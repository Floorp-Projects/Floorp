/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLRenderbuffer.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "ScopedGLHelpers.h"
#include "WebGLContext.h"
#include "WebGLStrongTypes.h"
#include "WebGLTexture.h"

namespace mozilla {

static GLenum
DepthStencilDepthFormat(gl::GLContext* gl)
{
    // We might not be able to get 24-bit, so let's pretend!
    if (gl->IsGLES() && !gl->IsExtensionSupported(gl::GLContext::OES_depth24))
        return LOCAL_GL_DEPTH_COMPONENT16;

    return LOCAL_GL_DEPTH_COMPONENT24;
}

static bool
SupportsDepthStencil(gl::GLContext* gl)
{
    return gl->IsExtensionSupported(gl::GLContext::EXT_packed_depth_stencil) ||
           gl->IsExtensionSupported(gl::GLContext::OES_packed_depth_stencil);
}

static bool
NeedsDepthStencilEmu(gl::GLContext* gl, GLenum internalFormat)
{
    MOZ_ASSERT(internalFormat != LOCAL_GL_DEPTH_STENCIL);
    if (internalFormat != LOCAL_GL_DEPTH24_STENCIL8)
        return false;

    return !SupportsDepthStencil(gl);
}

JSObject*
WebGLRenderbuffer::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLRenderbufferBinding::Wrap(cx, this, givenProto);
}

WebGLRenderbuffer::WebGLRenderbuffer(WebGLContext* webgl)
    : WebGLContextBoundObject(webgl)
    , mPrimaryRB(0)
    , mSecondaryRB(0)
    , mFormat(nullptr)
    , mImageDataStatus(WebGLImageDataStatus::NoImageData)
    , mSamples(1)
    , mIsUsingSecondary(false)
#ifdef ANDROID
    , mIsRB(false)
#endif
{
    mContext->MakeContextCurrent();

    mContext->gl->fGenRenderbuffers(1, &mPrimaryRB);
    if (!SupportsDepthStencil(mContext->gl))
        mContext->gl->fGenRenderbuffers(1, &mSecondaryRB);

    mContext->mRenderbuffers.insertBack(this);
}

void
WebGLRenderbuffer::Delete()
{
    mContext->MakeContextCurrent();

    mContext->gl->fDeleteRenderbuffers(1, &mPrimaryRB);
    if (mSecondaryRB)
        mContext->gl->fDeleteRenderbuffers(1, &mSecondaryRB);

    LinkedListElement<WebGLRenderbuffer>::removeFrom(mContext->mRenderbuffers);
#ifdef ANDROID
    mIsRB = false;
#endif
}

int64_t
WebGLRenderbuffer::MemoryUsage() const
{
    // If there is no defined format, we're not taking up any memory
    if (!mFormat)
        return 0;

    auto bytesPerPixel = mFormat->format->estimatedBytesPerPixel;
    uint64_t pixels = uint64_t(mWidth) * uint64_t(mHeight);

    uint64_t totalSize = pixels * bytesPerPixel;

    // If we have the same bytesPerPixel whether or not we have a secondary RB.
    if (mSecondaryRB && !mIsUsingSecondary) {
        totalSize += 2; // 1x1xRGBA4
    }

    return int64_t(totalSize);
}

void
WebGLRenderbuffer::BindRenderbuffer() const
{
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

static void
RenderbufferStorageMaybeMultisample(gl::GLContext* gl, GLsizei samples,
                                    GLenum internalFormat, GLsizei width,
                                    GLsizei height)
{
    MOZ_ASSERT_IF(samples >= 1, gl->IsSupported(gl::GLFeature::framebuffer_multisample));
    MOZ_ASSERT(samples >= 0);
    MOZ_ASSERT(samples <= gl->MaxSamples());

    // certain OpenGL ES renderbuffer formats may not exist on desktop OpenGL
    GLenum internalFormatForGL = internalFormat;

    switch (internalFormat) {
    case LOCAL_GL_RGBA4:
    case LOCAL_GL_RGB5_A1:
        // 16-bit RGBA formats are not supported on desktop GL
        if (!gl->IsGLES())
            internalFormatForGL = LOCAL_GL_RGBA8;
        break;

    case LOCAL_GL_RGB565:
        // the RGB565 format is not supported on desktop GL
        if (!gl->IsGLES())
            internalFormatForGL = LOCAL_GL_RGB8;
        break;

    case LOCAL_GL_DEPTH_COMPONENT16:
        if (!gl->IsGLES() || gl->IsExtensionSupported(gl::GLContext::OES_depth24))
            internalFormatForGL = LOCAL_GL_DEPTH_COMPONENT24;
        else if (gl->IsExtensionSupported(gl::GLContext::OES_packed_depth_stencil))
            internalFormatForGL = LOCAL_GL_DEPTH24_STENCIL8;
        break;

    case LOCAL_GL_DEPTH_STENCIL:
        // We emulate this in WebGLRenderbuffer if we don't have the requisite extension.
        internalFormatForGL = LOCAL_GL_DEPTH24_STENCIL8;
        break;

    default:
        break;
    }

    if (samples > 0) {
        gl->fRenderbufferStorageMultisample(LOCAL_GL_RENDERBUFFER, samples,
                                            internalFormatForGL, width, height);
    } else {
        gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, internalFormatForGL, width,
                                 height);
    }
}

void
WebGLRenderbuffer::RenderbufferStorage(GLsizei samples,
                                       const webgl::FormatUsageInfo* format,
                                       GLsizei width, GLsizei height)
{
    MOZ_ASSERT(mContext->mBoundRenderbuffer == this);


    gl::GLContext* gl = mContext->gl;
    MOZ_ASSERT(samples >= 0 && samples <= 256); // Sanity check.

    GLenum primaryFormat = format->format->sizedFormat;
    GLenum secondaryFormat = 0;

    if (NeedsDepthStencilEmu(mContext->gl, primaryFormat)) {
        primaryFormat = DepthStencilDepthFormat(gl);
        secondaryFormat = LOCAL_GL_STENCIL_INDEX8;
    }

    RenderbufferStorageMaybeMultisample(gl, samples, primaryFormat, width,
                                        height);

    if (mSecondaryRB) {
        // We can't leave the secondary RB unspecified either, since we should
        // handle the case where we attach a non-depth-stencil RB to a
        // depth-stencil attachment point, or attach this depth-stencil RB to a
        // non-depth-stencil attachment point.
        gl::ScopedBindRenderbuffer autoRB(gl, mSecondaryRB);
        if (secondaryFormat) {
            RenderbufferStorageMaybeMultisample(gl, samples, secondaryFormat, width,
                                                height);
        } else {
            RenderbufferStorageMaybeMultisample(gl, samples, LOCAL_GL_RGBA4, 1, 1);
        }
    }

    mSamples = samples;
    mFormat = format;
    mWidth = width;
    mHeight = height;
    mImageDataStatus = WebGLImageDataStatus::UninitializedImageData;
    mIsUsingSecondary = bool(secondaryFormat);

    InvalidateStatusOfAttachedFBs();
}

void
WebGLRenderbuffer::FramebufferRenderbuffer(GLenum attachment) const
{
    gl::GLContext* gl = mContext->gl;
    if (attachment != LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, attachment,
                                     LOCAL_GL_RENDERBUFFER, mPrimaryRB);
        return;
    }

    GLuint stencilRB = mPrimaryRB;
    if (mIsUsingSecondary) {
        MOZ_ASSERT(mSecondaryRB);
        stencilRB = mSecondaryRB;
    }
    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_DEPTH_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER, mPrimaryRB);
    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_STENCIL_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER, stencilRB);
}

GLint
WebGLRenderbuffer::GetRenderbufferParameter(RBTarget target,
                                            RBParam pname) const
{
    gl::GLContext* gl = mContext->gl;

    switch (pname.get()) {
    case LOCAL_GL_RENDERBUFFER_STENCIL_SIZE:
        if (!mFormat)
            return 0;

        if (!mFormat->format->hasStencil)
            return 0;

        return 8;

    case LOCAL_GL_RENDERBUFFER_WIDTH:
    case LOCAL_GL_RENDERBUFFER_HEIGHT:
    case LOCAL_GL_RENDERBUFFER_RED_SIZE:
    case LOCAL_GL_RENDERBUFFER_GREEN_SIZE:
    case LOCAL_GL_RENDERBUFFER_BLUE_SIZE:
    case LOCAL_GL_RENDERBUFFER_ALPHA_SIZE:
    case LOCAL_GL_RENDERBUFFER_DEPTH_SIZE:
        {
            GLint i = 0;
            gl->fGetRenderbufferParameteriv(target.get(), pname.get(), &i);
            return i;
        }

    case LOCAL_GL_RENDERBUFFER_INTERNAL_FORMAT:
        {
            GLenum ret = 0;
            if (mFormat) {
                ret = mFormat->format->sizedFormat;

                if (!mContext->IsWebGL2() && ret == LOCAL_GL_DEPTH24_STENCIL8) {
                    ret = LOCAL_GL_DEPTH_STENCIL;
                }
            }
            return ret;
        }
    }

    MOZ_ASSERT(false,
               "This function should only be called with valid `pname`.");
    return 0;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLRenderbuffer)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLRenderbuffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLRenderbuffer, Release)

} // namespace mozilla
