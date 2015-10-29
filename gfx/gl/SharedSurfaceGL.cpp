/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceGL.h"

#include "GLBlitHelper.h"
#include "GLContext.h"
#include "GLReadTexImageHelper.h"
#include "mozilla/gfx/2D.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace gl {

using gfx::IntSize;
using gfx::SurfaceFormat;

/*static*/ UniquePtr<SharedSurface_Basic>
SharedSurface_Basic::Create(GLContext* gl,
                            const GLFormats& formats,
                            const IntSize& size,
                            bool hasAlpha)
{
    UniquePtr<SharedSurface_Basic> ret;
    gl->MakeCurrent();

    GLContext::LocalErrorScope localError(*gl);
    GLuint tex = CreateTextureForOffscreen(gl, formats, size);

    GLenum err = localError.GetError();
    MOZ_ASSERT_IF(err != LOCAL_GL_NO_ERROR, err == LOCAL_GL_OUT_OF_MEMORY);
    if (err) {
        gl->fDeleteTextures(1, &tex);
        return Move(ret);
    }

    bool ownsTex = true;
    ret.reset( new SharedSurface_Basic(gl, size, hasAlpha, tex, ownsTex) );
    return Move(ret);
}


/*static*/ UniquePtr<SharedSurface_Basic>
SharedSurface_Basic::Wrap(GLContext* gl,
                          const IntSize& size,
                          bool hasAlpha,
                          GLuint tex)
{
    bool ownsTex = false;
    UniquePtr<SharedSurface_Basic> ret( new SharedSurface_Basic(gl, size, hasAlpha, tex,
                                                                ownsTex) );
    return Move(ret);
}

SharedSurface_Basic::SharedSurface_Basic(GLContext* gl,
                                         const IntSize& size,
                                         bool hasAlpha,
                                         GLuint tex,
                                         bool ownsTex)
    : SharedSurface(SharedSurfaceType::Basic,
                    AttachmentType::GLTexture,
                    gl,
                    size,
                    hasAlpha,
                    true)
    , mTex(tex)
    , mOwnsTex(ownsTex)
    , mFB(0)
{
    mGL->MakeCurrent();
    mGL->fGenFramebuffers(1, &mFB);

    ScopedBindFramebuffer autoFB(mGL, mFB);
    mGL->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                              LOCAL_GL_COLOR_ATTACHMENT0,
                              LOCAL_GL_TEXTURE_2D,
                              mTex,
                              0);

    DebugOnly<GLenum> status = mGL->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    MOZ_ASSERT(status == LOCAL_GL_FRAMEBUFFER_COMPLETE);
}

SharedSurface_Basic::~SharedSurface_Basic()
{
    if (!mGL->MakeCurrent())
        return;

    if (mFB)
        mGL->fDeleteFramebuffers(1, &mFB);

    if (mOwnsTex)
        mGL->fDeleteTextures(1, &mTex);
}


////////////////////////////////////////////////////////////////////////

SurfaceFactory_Basic::SurfaceFactory_Basic(GLContext* gl, const SurfaceCaps& caps,
                                           const layers::TextureFlags& flags)
    : SurfaceFactory(SharedSurfaceType::Basic, gl, caps, nullptr, flags)
{ }


////////////////////////////////////////////////////////////////////////
// SharedSurface_GLTexture

/*static*/ UniquePtr<SharedSurface_GLTexture>
SharedSurface_GLTexture::Create(GLContext* prodGL,
                                const GLFormats& formats,
                                const IntSize& size,
                                bool hasAlpha)
{
    MOZ_ASSERT(prodGL);

    prodGL->MakeCurrent();

    UniquePtr<SharedSurface_GLTexture> ret;
    GLContext::LocalErrorScope localError(*prodGL);

    GLuint tex = CreateTextureForOffscreen(prodGL, formats, size);

    GLenum err = localError.GetError();
    MOZ_ASSERT_IF(err, err == LOCAL_GL_OUT_OF_MEMORY);
    if (err) {
        prodGL->fDeleteTextures(1, &tex);
        return Move(ret);
    }

    ret.reset(new SharedSurface_GLTexture(prodGL, size,
                                          hasAlpha, tex));
    return Move(ret);
}

SharedSurface_GLTexture::~SharedSurface_GLTexture()
{
    if (!mGL->MakeCurrent())
        return;

    if (mTex) {
        mGL->fDeleteTextures(1, &mTex);
    }

    if (mSync) {
        mGL->fDeleteSync(mSync);
    }
}

void
SharedSurface_GLTexture::ProducerReleaseImpl()
{
    mGL->MakeCurrent();

    if (mGL->IsSupported(GLFeature::sync)) {
        if (mSync) {
            mGL->fDeleteSync(mSync);
            mSync = 0;
        }

        mSync = mGL->fFenceSync(LOCAL_GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if (mSync) {
            mGL->fFlush();
            return;
        }
    }
    MOZ_ASSERT(!mSync);

    mGL->fFinish();
}

bool
SharedSurface_GLTexture::ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor)
{
    *out_descriptor = layers::SurfaceDescriptorSharedGLTexture(ProdTexture(),
                                                               ProdTextureTarget(),
                                                               (uintptr_t)mSync,
                                                               mSize,
                                                               mHasAlpha);

    // Transfer ownership of the fence to the host
    mSync = nullptr;
    return true;
}


} // namespace gl

} /* namespace mozilla */
