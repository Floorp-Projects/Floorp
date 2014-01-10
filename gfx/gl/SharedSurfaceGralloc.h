/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_GRALLOC_H_
#define SHARED_SURFACE_GRALLOC_H_

#include "SharedSurfaceGL.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/TextureClient.h"

namespace mozilla {
namespace layers {
class ISurfaceAllocator;
}

namespace gl {
class GLContext;
class GLLibraryEGL;

class SharedSurface_Gralloc
    : public SharedSurface_GL
{
public:
    static SharedSurface_Gralloc* Create(GLContext* prodGL,
                                         const GLFormats& formats,
                                         const gfx::IntSize& size,
                                         bool hasAlpha,
                                         layers::ISurfaceAllocator* allocator);

    static SharedSurface_Gralloc* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->Type() == SharedSurfaceType::Gralloc);

        return (SharedSurface_Gralloc*)surf;
    }

protected:
    GLLibraryEGL* const mEGL;
    RefPtr<layers::ISurfaceAllocator> mAllocator;
    RefPtr<layers::TextureClient> mTextureClient;
    const GLuint mProdTex;

    SharedSurface_Gralloc(GLContext* prodGL,
                          const gfx::IntSize& size,
                          bool hasAlpha,
                          GLLibraryEGL* egl,
                          layers::ISurfaceAllocator* allocator,
                          layers::TextureClient* textureClient,
                          GLuint prodTex)
        : SharedSurface_GL(SharedSurfaceType::Gralloc,
                           AttachmentType::GLTexture,
                           prodGL,
                           size,
                           hasAlpha)
        , mEGL(egl)
        , mAllocator(allocator)
        , mTextureClient(textureClient)
        , mProdTex(prodTex)
    {}

    static bool HasExtensions(GLLibraryEGL* egl, GLContext* gl);

public:
    virtual ~SharedSurface_Gralloc();

    virtual void Fence();
    virtual bool WaitSync();

    virtual void LockProdImpl();
    virtual void UnlockProdImpl();

    virtual GLuint Texture() const {
        return mProdTex;
    }

    layers::TextureClient* GetTextureClient() {
        return mTextureClient;
    }
};

class SurfaceFactory_Gralloc
    : public SurfaceFactory_GL
{
protected:
    RefPtr<layers::ISurfaceAllocator> mAllocator;

public:
    SurfaceFactory_Gralloc(GLContext* prodGL,
                           const SurfaceCaps& caps,
                           layers::ISurfaceAllocator* allocator = nullptr);

    virtual SharedSurface* CreateShared(const gfx::IntSize& size) {
        bool hasAlpha = mReadCaps.alpha;
        if (!mAllocator) {
            return nullptr;
        }
        return SharedSurface_Gralloc::Create(mGL, mFormats, size, hasAlpha, mAllocator);
    }
};

} /* namespace gl */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_GRALLOC_H_ */
