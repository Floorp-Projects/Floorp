/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_GRALLOC_H_
#define SHARED_SURFACE_GRALLOC_H_

#include "SharedSurfaceGL.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/ISurfaceAllocator.h"

namespace mozilla {
namespace layers {
class ISurfaceAllocator;
class SurfaceDescriptorGralloc;
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
                                         const gfxIntSize& size,
                                         bool hasAlpha,
                                         layers::ISurfaceAllocator* allocator);

    static SharedSurface_Gralloc* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->Type() == SharedSurfaceType::Gralloc);

        return (SharedSurface_Gralloc*)surf;
    }

protected:
    GLLibraryEGL* const mEGL;
    layers::ISurfaceAllocator* const mAllocator;
    // We keep the SurfaceDescriptor around, because we'll end up
    // using it often and it's handy to do so.  The actual
    // GraphicBuffer is kept alive by the sp<GraphicBuffer> in
    // GrallocBufferActor; the actor will stay alive until we
    // explicitly destroy this descriptor (and thus deallocate the
    // actor) it in the destructor of this class.  This is okay to do
    // on the client, but is very bad to do on the server (because on
    // the client, the actor has no chance of going away unless the
    // whole app died).
    layers::SurfaceDescriptorGralloc mDesc;
    const GLuint mProdTex;

    SharedSurface_Gralloc(GLContext* prodGL,
                          const gfxIntSize& size,
                          bool hasAlpha,
                          GLLibraryEGL* egl,
                          layers::ISurfaceAllocator* allocator,
                          layers::SurfaceDescriptorGralloc& desc,
                          GLuint prodTex)
        : SharedSurface_GL(SharedSurfaceType::Gralloc,
                           AttachmentType::GLTexture,
                           prodGL,
                           size,
                           hasAlpha)
        , mEGL(egl)
        , mAllocator(allocator)
        , mDesc(desc)
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

    layers::SurfaceDescriptorGralloc& GetDescriptor() {
        return mDesc;
    }
};

class SurfaceFactory_Gralloc
    : public SurfaceFactory_GL
{
protected:
    WeakPtr<layers::ISurfaceAllocator> mAllocator;

public:
    SurfaceFactory_Gralloc(GLContext* prodGL,
                           const SurfaceCaps& caps,
                           layers::ISurfaceAllocator* allocator = nullptr);

    virtual SharedSurface* CreateShared(const gfxIntSize& size) {
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
