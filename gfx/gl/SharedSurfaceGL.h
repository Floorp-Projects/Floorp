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
    } // namespace gl
    namespace gfx {
        class DataSourceSurface;
    } // namespace gfx
} // namespace mozilla

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

    static UniquePtr<SharedSurface_Basic> Wrap(GLContext* gl,
                                               const gfx::IntSize& size,
                                               bool hasAlpha,
                                               GLuint tex);

    static SharedSurface_Basic* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->mType == SharedSurfaceType::Basic);

        return (SharedSurface_Basic*)surf;
    }

protected:
    const GLuint mTex;
    const bool mOwnsTex;
    GLuint mFB;

    SharedSurface_Basic(GLContext* gl,
                        const gfx::IntSize& size,
                        bool hasAlpha,
                        GLuint tex,
                        bool ownsTex);

public:
    virtual ~SharedSurface_Basic();

    virtual void LockProdImpl() override {}
    virtual void UnlockProdImpl() override {}

    virtual void Fence() override {}
    virtual bool WaitSync() override { return true; }
    virtual bool PollSync() override { return true; }

    virtual GLuint ProdTexture() override {
        return mTex;
    }

    virtual bool ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor) override {
        MOZ_CRASH("don't do this");
        return false;
    }
};

class SurfaceFactory_Basic
    : public SurfaceFactory
{
public:
    SurfaceFactory_Basic(GLContext* gl, const SurfaceCaps& caps,
                         const layers::TextureFlags& flags);

    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) override {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_Basic::Create(mGL, mFormats, size, hasAlpha);
    }
};

} // namespace gl

} /* namespace mozilla */

#endif /* SHARED_SURFACE_GL_H_ */
