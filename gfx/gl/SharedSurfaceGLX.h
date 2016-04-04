/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_GLX_H_
#define SHARED_SURFACE_GLX_H_

#include "SharedSurface.h"
#include "mozilla/RefPtr.h"

class gfxXlibSurface;

namespace mozilla {
namespace gl {

class SharedSurface_GLXDrawable
    : public SharedSurface
{
public:
    static UniquePtr<SharedSurface_GLXDrawable> Create(GLContext* prodGL,
                                                       const SurfaceCaps& caps,
                                                       const gfx::IntSize& size,
                                                       bool deallocateClient,
                                                       bool inSameProcess);

    virtual void ProducerAcquireImpl() override {}
    virtual void ProducerReleaseImpl() override;

    virtual void LockProdImpl() override;
    virtual void UnlockProdImpl() override;

    virtual bool ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor) override;

    virtual bool ReadbackBySharedHandle(gfx::DataSourceSurface* out_surface) override;
private:
    SharedSurface_GLXDrawable(GLContext* gl,
                              const gfx::IntSize& size,
                              bool inSameProcess,
                              const RefPtr<gfxXlibSurface>& xlibSurface);

    RefPtr<gfxXlibSurface> mXlibSurface;
    bool mInSameProcess;
};

class SurfaceFactory_GLXDrawable
    : public SurfaceFactory
{
public:
    static UniquePtr<SurfaceFactory_GLXDrawable> Create(GLContext* prodGL,
                                                        const SurfaceCaps& caps,
                                                        const RefPtr<layers::ClientIPCAllocator>& allocator,
                                                        const layers::TextureFlags& flags);

    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) override;

private:
    SurfaceFactory_GLXDrawable(GLContext* prodGL, const SurfaceCaps& caps,
                               const RefPtr<layers::ClientIPCAllocator>& allocator,
                               const layers::TextureFlags& flags)
        : SurfaceFactory(SharedSurfaceType::GLXDrawable, prodGL, caps, allocator, flags)
    { }
};

} // namespace gl
} // namespace mozilla

#endif // SHARED_SURFACE_GLX_H_
