/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_GL_H_
#define SHARED_SURFACE_GL_H_

#include "ScopedGLHelpers.h"
#include "SharedSurface.h"
#include "SurfaceTypes.h"
#include "GLContextTypes.h"
#include "nsAutoPtr.h"
#include "gfxTypes.h"
#include "mozilla/Mutex.h"

#include <queue>

namespace mozilla {
    namespace gl {
        class GLContext;
    }
    namespace gfx {
        class DataSourceSurface;
    }
}

namespace mozilla {
namespace gl {

// For readback and bootstrapping:
class SharedSurface_Basic
    : public SharedSurface
{
public:
    static UniquePtr<SharedSurface_Basic> Create(GLContext* gl,
                                                 const GLFormats& formats,
                                                 const gfx::IntSize& size,
                                                 bool hasAlpha);

    static SharedSurface_Basic* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->mType == SharedSurfaceType::Basic);

        return (SharedSurface_Basic*)surf;
    }

protected:
    const GLuint mTex;
    GLuint mFB;
    RefPtr<gfx::DataSourceSurface> mData;
    bool mIsDataCurrent;

    SharedSurface_Basic(GLContext* gl,
                        const gfx::IntSize& size,
                        bool hasAlpha,
                        gfx::SurfaceFormat format,
                        GLuint tex);

public:
    virtual ~SharedSurface_Basic();

    virtual void LockProdImpl() MOZ_OVERRIDE {}
    virtual void UnlockProdImpl() MOZ_OVERRIDE {}


    virtual void Fence() MOZ_OVERRIDE;
    virtual bool WaitSync() MOZ_OVERRIDE;
    virtual bool PollSync() MOZ_OVERRIDE;

    virtual void Fence_ContentThread_Impl() MOZ_OVERRIDE;
    virtual bool WaitSync_ContentThread_Impl() MOZ_OVERRIDE;
    virtual bool PollSync_ContentThread_Impl() MOZ_OVERRIDE;

    virtual GLuint ProdTexture() MOZ_OVERRIDE {
        return mTex;
    }

    // Implementation-specific functions below:
    gfx::DataSourceSurface* GetData() {
        return mData;
    }
};

class SurfaceFactory_Basic
    : public SurfaceFactory
{
public:
    SurfaceFactory_Basic(GLContext* gl, const SurfaceCaps& caps)
        : SurfaceFactory(gl, SharedSurfaceType::Basic, caps)
    {}

    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) MOZ_OVERRIDE {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_Basic::Create(mGL, mFormats, size, hasAlpha);
    }
};


// Using shared GL textures:
class SharedSurface_GLTexture
    : public SharedSurface
{
public:
    static UniquePtr<SharedSurface_GLTexture> Create(GLContext* prodGL,
                                                     GLContext* consGL,
                                                     const GLFormats& formats,
                                                     const gfx::IntSize& size,
                                                     bool hasAlpha,
                                                     GLuint texture = 0);

    static SharedSurface_GLTexture* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->mType == SharedSurfaceType::GLTextureShare);

        return (SharedSurface_GLTexture*)surf;
    }

protected:
    GLContext* mConsGL;
    const GLuint mTex;
    const bool mOwnsTex;
    GLsync mSync;
    mutable Mutex mMutex;

    SharedSurface_GLTexture(GLContext* prodGL,
                            GLContext* consGL,
                            const gfx::IntSize& size,
                            bool hasAlpha,
                            GLuint tex,
                            bool ownsTex)
        : SharedSurface(SharedSurfaceType::GLTextureShare,
                        AttachmentType::GLTexture,
                        prodGL,
                        size,
                        hasAlpha)
        , mConsGL(consGL)
        , mTex(tex)
        , mOwnsTex(ownsTex)
        , mSync(0)
        , mMutex("SharedSurface_GLTexture mutex")
    {
    }

public:
    virtual ~SharedSurface_GLTexture();

    virtual void LockProdImpl() MOZ_OVERRIDE {}
    virtual void UnlockProdImpl() MOZ_OVERRIDE {}

    virtual void Fence() MOZ_OVERRIDE;
    virtual bool WaitSync() MOZ_OVERRIDE;
    virtual bool PollSync() MOZ_OVERRIDE;

    virtual GLuint ProdTexture() MOZ_OVERRIDE {
        return mTex;
    }

    // Custom:

    GLuint ConsTexture(GLContext* consGL);

    GLenum ConsTextureTarget() const {
        return ProdTextureTarget();
    }
};

class SurfaceFactory_GLTexture
    : public SurfaceFactory
{
protected:
    GLContext* const mConsGL;

public:
    // If we don't know `consGL` at construction time, use `nullptr`, and call
    // `SetConsumerGL()` on each `SharedSurface_GLTexture` before calling its
    // `WaitSync()`.
    SurfaceFactory_GLTexture(GLContext* prodGL,
                             GLContext* consGL,
                             const SurfaceCaps& caps)
        : SurfaceFactory(prodGL, SharedSurfaceType::GLTextureShare, caps)
        , mConsGL(consGL)
    {
        MOZ_ASSERT(consGL != prodGL);
    }

    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) MOZ_OVERRIDE {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_GLTexture::Create(mGL, mConsGL, mFormats, size, hasAlpha);
    }
};

} /* namespace gfx */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_GL_H_ */
