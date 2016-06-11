/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACEIO_H_
#define SHARED_SURFACEIO_H_

#include "mozilla/RefPtr.h"
#include "SharedSurface.h"

class MacIOSurface;

namespace mozilla {
namespace gl {

class SharedSurface_IOSurface : public SharedSurface
{
private:
    const RefPtr<MacIOSurface> mIOSurf;
    GLuint mProdTex;

public:
    static UniquePtr<SharedSurface_IOSurface> Create(const RefPtr<MacIOSurface>& ioSurf,
                                                     GLContext* gl,
                                                     bool hasAlpha);

private:
    SharedSurface_IOSurface(const RefPtr<MacIOSurface>& ioSurf,
                            GLContext* gl, const gfx::IntSize& size,
                            bool hasAlpha);

public:
    ~SharedSurface_IOSurface();

    virtual void LockProdImpl() override { }
    virtual void UnlockProdImpl() override { }

    virtual void ProducerAcquireImpl() override {}
    virtual void ProducerReleaseImpl() override;

    virtual bool CopyTexImage2D(GLenum target, GLint level, GLenum internalformat,
                                GLint x, GLint y, GLsizei width, GLsizei height,
                                GLint border) override;
    virtual bool ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                            GLenum format, GLenum type, GLvoid* pixels) override;

    virtual GLuint ProdTexture() override {
        return mProdTex;
    }

    virtual GLenum ProdTextureTarget() const override {
        return LOCAL_GL_TEXTURE_RECTANGLE_ARB;
    }

    static SharedSurface_IOSurface* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->mType == SharedSurfaceType::IOSurface);
        return static_cast<SharedSurface_IOSurface*>(surf);
    }

    MacIOSurface* GetIOSurface() const {
        return mIOSurf;
    }

    virtual bool NeedsIndirectReads() const override {
        return true;
    }

    virtual bool ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor) override;

    virtual bool ReadbackBySharedHandle(gfx::DataSourceSurface* out_surface) override;
};

class SurfaceFactory_IOSurface : public SurfaceFactory
{
public:
    // Infallible.
    static UniquePtr<SurfaceFactory_IOSurface> Create(GLContext* gl,
                                                      const SurfaceCaps& caps,
                                                      const RefPtr<layers::ClientIPCAllocator>& allocator,
                                                      const layers::TextureFlags& flags);
protected:
    const gfx::IntSize mMaxDims;

    SurfaceFactory_IOSurface(GLContext* gl, const SurfaceCaps& caps,
                             const RefPtr<layers::ClientIPCAllocator>& allocator,
                             const layers::TextureFlags& flags,
                             const gfx::IntSize& maxDims)
        : SurfaceFactory(SharedSurfaceType::IOSurface, gl, caps, allocator, flags)
        , mMaxDims(maxDims)
    { }

    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) override;
};

} // namespace gl

} /* namespace mozilla */

#endif /* SHARED_SURFACEIO_H_ */
