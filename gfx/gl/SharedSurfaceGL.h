/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_GL_H_
#define SHARED_SURFACE_GL_H_

#include "SharedSurface.h"
#include "SurfaceFactory.h"
#include "SurfaceTypes.h"
#include "GLContextTypes.h"
#include "nsAutoPtr.h"
#include "gfxASurface.h"
#include "mozilla/Mutex.h"

#include <queue>

// Forwards:
class gfxImageSurface;
namespace mozilla {
    namespace gl {
        class GLContext;
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
                     const gfxIntSize& size,
                     bool hasAlpha)
        : SharedSurface(type, APITypeT::OpenGL, attachType, size, hasAlpha)
        , mGL(gl)
    {}

public:
    static void Copy(SharedSurface_GL* src, SharedSurface_GL* dest,
                     SurfaceFactory_GL* factory);

    static SharedSurface_GL* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->APIType() == APITypeT::OpenGL);

        return (SharedSurface_GL*)surf;
    }

    virtual void LockProd();
    virtual void UnlockProd();

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
                                       const gfxIntSize& size,
                                       bool hasAlpha);

    static SharedSurface_Basic* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->Type() == SharedSurfaceType::Basic);

        return (SharedSurface_Basic*)surf;
    }

protected:
    const GLuint mTex;
    nsRefPtr<gfxImageSurface> mData;

    SharedSurface_Basic(GLContext* gl,
                        const gfxIntSize& size,
                        bool hasAlpha,
                        gfxASurface::gfxImageFormat format,
                        GLuint tex);

public:
    virtual ~SharedSurface_Basic();

    virtual void LockProdImpl() {}
    virtual void UnlockProdImpl() {}


    virtual void Fence();

    virtual bool WaitSync() {
        // Since we already store the data in Fence, we're always done already.
        return true;
    }


    virtual GLuint Texture() const {
        return mTex;
    }

    // Implementation-specific functions below:
    gfxImageSurface* GetData() {
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

    virtual SharedSurface* CreateShared(const gfxIntSize& size) {
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
                                           const gfxIntSize& size,
                                           bool hasAlpha);

    static SharedSurface_GLTexture* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->Type() == SharedSurfaceType::GLTextureShare);

        return (SharedSurface_GLTexture*)surf;
    }

protected:
    GLContext* mConsGL;
    const GLuint mTex;
    GLsync mSync;
    mutable Mutex mMutex;

    SharedSurface_GLTexture(GLContext* prodGL,
                            GLContext* consGL,
                            const gfxIntSize& size,
                            bool hasAlpha,
                            GLuint tex)
        : SharedSurface_GL(SharedSurfaceType::GLTextureShare,
                           AttachmentType::GLTexture,
                           prodGL,
                           size,
                           hasAlpha)
        , mConsGL(consGL)
        , mTex(tex)
        , mSync(0)
        , mMutex("SharedSurface_GLTexture mutex")
    {
    }

public:
    virtual ~SharedSurface_GLTexture();

    virtual void LockProdImpl() {}
    virtual void UnlockProdImpl() {}


    virtual void Fence();
    virtual bool WaitSync();


    virtual GLuint Texture() const {
        return mTex;
    }

    // Custom:
    void SetConsumerGL(GLContext* consGL);
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

    virtual SharedSurface* CreateShared(const gfxIntSize& size) {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_GLTexture::Create(mGL, mConsGL, mFormats, size, hasAlpha);
    }
};

} /* namespace gfx */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_GL_H_ */
