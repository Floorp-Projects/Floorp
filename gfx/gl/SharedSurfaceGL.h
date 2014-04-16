/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_GL_H_
#define SHARED_SURFACE_GL_H_

#include "ScopedGLHelpers.h"
#include "SharedSurface.h"
#include "SurfaceFactory.h"
#include "SurfaceTypes.h"
#include "GLContextTypes.h"
#include "nsAutoPtr.h"
#include "gfxTypes.h"
#include "mozilla/Mutex.h"

#include <queue>

// Forwards:
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

class SurfaceFactory_GL;

class SharedSurface_GL
    : public gfx::SharedSurface
{
protected:
    typedef class gfx::SharedSurface SharedSurface;
    typedef gfx::SharedSurfaceType SharedSurfaceType;
    typedef gfx::APITypeT APITypeT;
    typedef gfx::AttachmentType AttachmentType;

    GLContext* const mGL;

    SharedSurface_GL(SharedSurfaceType type,
                     AttachmentType attachType,
                     GLContext* gl,
                     const gfx::IntSize& size,
                     bool hasAlpha)
        : SharedSurface(type, APITypeT::OpenGL, attachType, size, hasAlpha)
        , mGL(gl)
    {}

public:
    static void ProdCopy(SharedSurface_GL* src, SharedSurface_GL* dest,
                         SurfaceFactory_GL* factory);

    static SharedSurface_GL* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->APIType() == APITypeT::OpenGL);

        return (SharedSurface_GL*)surf;
    }

    // For use when AttachType is correct.
    virtual GLuint ProdTexture() {
        MOZ_ASSERT(AttachType() == AttachmentType::GLTexture);
        MOZ_CRASH("Did you forget to override this function?");
    }

    virtual GLenum ProdTextureTarget() const {
        return LOCAL_GL_TEXTURE_2D;
    }

    virtual GLuint ProdRenderbuffer() {
        MOZ_ASSERT(AttachType() == AttachmentType::GLRenderbuffer);
        MOZ_CRASH("Did you forget to override this function?");
    }

    virtual bool ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                            GLenum format, GLenum type, GLvoid *pixels) {
        return false;
    }

    virtual void LockProd() MOZ_OVERRIDE;
    virtual void UnlockProd() MOZ_OVERRIDE;

    GLContext* GL() const {
        return mGL;
    }
};

class SurfaceFactory_GL
    : public gfx::SurfaceFactory
{
protected:
    typedef struct gfx::SurfaceCaps SurfaceCaps;
    typedef class gfx::SurfaceFactory SurfaceFactory;
    typedef class gfx::SharedSurface SharedSurface;
    typedef gfx::SharedSurfaceType SharedSurfaceType;

    GLContext* const mGL;
    const GLFormats mFormats;

    SurfaceCaps mDrawCaps;
    SurfaceCaps mReadCaps;

    // This uses ChooseBufferBits to pick drawBits/readBits.
    SurfaceFactory_GL(GLContext* gl,
                      SharedSurfaceType type,
                      const SurfaceCaps& caps);

    virtual void ChooseBufferBits(const SurfaceCaps& caps,
                                  SurfaceCaps& drawCaps,
                                  SurfaceCaps& readCaps) const;

public:
    GLContext* GL() const {
        return mGL;
    }

    const GLFormats& Formats() const {
        return mFormats;
    }

    const SurfaceCaps& DrawCaps() const {
        return mDrawCaps;
    }

    const SurfaceCaps& ReadCaps() const {
        return mReadCaps;
    }
};

// For readback and bootstrapping:
class SharedSurface_Basic
    : public SharedSurface_GL
{
public:
    static SharedSurface_Basic* Create(GLContext* gl,
                                       const GLFormats& formats,
                                       const gfx::IntSize& size,
                                       bool hasAlpha);

    static SharedSurface_Basic* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->Type() == SharedSurfaceType::Basic);

        return (SharedSurface_Basic*)surf;
    }

protected:
    const GLuint mTex;
    GLuint mFB;

    RefPtr<gfx::DataSourceSurface> mData;

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

    virtual bool WaitSync() MOZ_OVERRIDE {
        // Since we already store the data in Fence, we're always done already.
        return true;
    }

    virtual GLuint ProdTexture() MOZ_OVERRIDE {
        return mTex;
    }

    // Implementation-specific functions below:
    gfx::DataSourceSurface* GetData() {
        return mData;
    }
};

class SurfaceFactory_Basic
    : public SurfaceFactory_GL
{
public:
    SurfaceFactory_Basic(GLContext* gl, const SurfaceCaps& caps)
        : SurfaceFactory_GL(gl, SharedSurfaceType::Basic, caps)
    {}

    virtual SharedSurface* CreateShared(const gfx::IntSize& size) MOZ_OVERRIDE {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_Basic::Create(mGL, mFormats, size, hasAlpha);
    }
};


// Using shared GL textures:
class SharedSurface_GLTexture
    : public SharedSurface_GL
{
public:
    static SharedSurface_GLTexture* Create(GLContext* prodGL,
                                           GLContext* consGL,
                                           const GLFormats& formats,
                                           const gfx::IntSize& size,
                                           bool hasAlpha,
                                           GLuint texture = 0);

    static SharedSurface_GLTexture* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->Type() == SharedSurfaceType::GLTextureShare);

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
        : SharedSurface_GL(SharedSurfaceType::GLTextureShare,
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
    : public SurfaceFactory_GL
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
        : SurfaceFactory_GL(prodGL, SharedSurfaceType::GLTextureShare, caps)
        , mConsGL(consGL)
    {
        MOZ_ASSERT(consGL != prodGL);
    }

    virtual SharedSurface* CreateShared(const gfx::IntSize& size) MOZ_OVERRIDE {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_GLTexture::Create(mGL, mConsGL, mFormats, size, hasAlpha);
    }
};

} /* namespace gfx */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_GL_H_ */
