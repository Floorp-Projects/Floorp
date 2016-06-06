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
DepthFormatForDepthStencilEmu(gl::GLContext* gl)
{
    // We might not be able to get 24-bit, so let's pretend!
    if (gl->IsGLES() && !gl->IsExtensionSupported(gl::GLContext::OES_depth24))
        return LOCAL_GL_DEPTH_COMPONENT16;

    return LOCAL_GL_DEPTH_COMPONENT24;
}

JSObject*
WebGLRenderbuffer::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLRenderbufferBinding::Wrap(cx, this, givenProto);
}

static GLuint
DoCreateRenderbuffer(gl::GLContext* gl)
{
    MOZ_ASSERT(gl->IsCurrent());

    GLuint ret = 0;
    gl->fGenRenderbuffers(1, &ret);
    return ret;
}

static bool
EmulatePackedDepthStencil(gl::GLContext* gl)
{
    return !gl->IsSupported(gl::GLFeature::packed_depth_stencil);
}

WebGLRenderbuffer::WebGLRenderbuffer(WebGLContext* webgl)
    : WebGLContextBoundObject(webgl)
    , mPrimaryRB( DoCreateRenderbuffer(webgl->gl) )
    , mEmulatePackedDepthStencil( EmulatePackedDepthStencil(webgl->gl) )
    , mSecondaryRB(0)
    , mFormat(nullptr)
    , mSamples(0)
    , mImageDataStatus(WebGLImageDataStatus::NoImageData)
    , mHasBeenBound(false)
{
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
}

int64_t
WebGLRenderbuffer::MemoryUsage() const
{
    // If there is no defined format, we're not taking up any memory
    if (!mFormat)
        return 0;

    const auto bytesPerPixel = mFormat->format->estimatedBytesPerPixel;
    const int64_t pixels = int64_t(mWidth) * int64_t(mHeight);

    const int64_t totalSize = pixels * bytesPerPixel;
    return totalSize;
}

static GLenum
DoRenderbufferStorageMaybeMultisample(gl::GLContext* gl, GLsizei samples,
                                      GLenum internalFormat, GLsizei width,
                                      GLsizei height)
{
    MOZ_ASSERT_IF(samples >= 1, gl->IsSupported(gl::GLFeature::framebuffer_multisample));

    // Certain OpenGL ES renderbuffer formats may not exist on desktop OpenGL.
    switch (internalFormat) {
    case LOCAL_GL_RGBA4:
    case LOCAL_GL_RGB5_A1:
        // 16-bit RGBA formats are not supported on desktop GL.
        if (!gl->IsGLES())
            internalFormat = LOCAL_GL_RGBA8;
        break;

    case LOCAL_GL_RGB565:
        // RGB565 is not supported on desktop GL.
        if (!gl->IsGLES())
            internalFormat = LOCAL_GL_RGB8;
        break;

    case LOCAL_GL_DEPTH_COMPONENT16:
        if (!gl->IsGLES() || gl->IsExtensionSupported(gl::GLContext::OES_depth24))
            internalFormat = LOCAL_GL_DEPTH_COMPONENT24;
        else if (gl->IsSupported(gl::GLFeature::packed_depth_stencil))
            internalFormat = LOCAL_GL_DEPTH24_STENCIL8;
        break;

    case LOCAL_GL_DEPTH_STENCIL:
        MOZ_CRASH("GFX: GL_DEPTH_STENCIL is not valid here.");
        break;

    default:
        break;
    }

    gl::GLContext::LocalErrorScope errorScope(*gl);

    if (samples > 0) {
        gl->fRenderbufferStorageMultisample(LOCAL_GL_RENDERBUFFER, samples,
                                            internalFormat, width, height);
    } else {
        gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, internalFormat, width, height);
    }

    return errorScope.GetError();
}

GLenum
WebGLRenderbuffer::DoRenderbufferStorage(uint32_t samples,
                                         const webgl::FormatUsageInfo* format,
                                         uint32_t width, uint32_t height)
{
    MOZ_ASSERT(mContext->mBoundRenderbuffer == this);

    gl::GLContext* gl = mContext->gl;
    MOZ_ASSERT(samples <= 256); // Sanity check.

    GLenum primaryFormat = format->format->sizedFormat;
    GLenum secondaryFormat = 0;

    if (mEmulatePackedDepthStencil && primaryFormat == LOCAL_GL_DEPTH24_STENCIL8) {
        primaryFormat = DepthFormatForDepthStencilEmu(gl);
        secondaryFormat = LOCAL_GL_STENCIL_INDEX8;
    }

    gl->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, mPrimaryRB);
    GLenum error = DoRenderbufferStorageMaybeMultisample(gl, samples, primaryFormat,
                                                         width, height);
    if (error)
        return error;

    if (secondaryFormat) {
        if (!mSecondaryRB) {
            gl->fGenRenderbuffers(1, &mSecondaryRB);
        }

        gl->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, mSecondaryRB);
        error = DoRenderbufferStorageMaybeMultisample(gl, samples, secondaryFormat,
                                                      width, height);
        if (error)
            return error;
    } else if (mSecondaryRB) {
        gl->fDeleteRenderbuffers(1, &mSecondaryRB);
        mSecondaryRB = 0;
    }

    return 0;
}

void
WebGLRenderbuffer::RenderbufferStorage(const char* funcName, uint32_t samples,
                                       GLenum internalFormat, uint32_t width,
                                       uint32_t height)
{
    const auto usage = mContext->mFormatUsage->GetRBUsage(internalFormat);
    if (!usage) {
        mContext->ErrorInvalidEnum("%s: Invalid `internalFormat`: 0x%04x.", funcName,
                                   internalFormat);
        return;
    }

    if (width > mContext->mImplMaxRenderbufferSize ||
        height > mContext->mImplMaxRenderbufferSize)
    {
        mContext->ErrorInvalidValue("%s: Width or height exceeds maximum renderbuffer"
                                    " size.",
                                    funcName);
        return;
    }

    mContext->MakeContextCurrent();

    if (!usage->maxSamplesKnown) {
        const_cast<webgl::FormatUsageInfo*>(usage)->ResolveMaxSamples(mContext->gl);
    }
    MOZ_ASSERT(usage->maxSamplesKnown);

    if (samples > usage->maxSamples) {
        mContext->ErrorInvalidValue("%s: `samples` is out of the valid range.", funcName);
        return;
    }

    // Validation complete.

    const GLenum error = DoRenderbufferStorage(samples, usage, width, height);
    if (error) {
        const char* errorName = mContext->ErrorName(error);
        mContext->GenerateWarning("%s generated error %s", funcName, errorName);
        return;
    }

    mSamples = samples;
    mFormat = usage;
    mWidth = width;
    mHeight = height;
    mImageDataStatus = WebGLImageDataStatus::UninitializedImageData;

    InvalidateStatusOfAttachedFBs();
}

void
WebGLRenderbuffer::DoFramebufferRenderbuffer(GLenum attachment) const
{
    gl::GLContext* gl = mContext->gl;

    if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        const GLuint stencilRB = (mSecondaryRB ? mSecondaryRB : mPrimaryRB);
        gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                     LOCAL_GL_DEPTH_ATTACHMENT,
                                     LOCAL_GL_RENDERBUFFER, mPrimaryRB);
        gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                     LOCAL_GL_STENCIL_ATTACHMENT,
                                     LOCAL_GL_RENDERBUFFER, stencilRB);
        return;
    }

    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, attachment,
                                 LOCAL_GL_RENDERBUFFER, mPrimaryRB);
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

    case LOCAL_GL_RENDERBUFFER_SAMPLES:
    case LOCAL_GL_RENDERBUFFER_WIDTH:
    case LOCAL_GL_RENDERBUFFER_HEIGHT:
    case LOCAL_GL_RENDERBUFFER_RED_SIZE:
    case LOCAL_GL_RENDERBUFFER_GREEN_SIZE:
    case LOCAL_GL_RENDERBUFFER_BLUE_SIZE:
    case LOCAL_GL_RENDERBUFFER_ALPHA_SIZE:
    case LOCAL_GL_RENDERBUFFER_DEPTH_SIZE:
        {
            gl->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, mPrimaryRB);
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

    MOZ_ASSERT(false, "This function should only be called with valid `pname`.");
    return 0;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLRenderbuffer)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLRenderbuffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLRenderbuffer, Release)

} // namespace mozilla
