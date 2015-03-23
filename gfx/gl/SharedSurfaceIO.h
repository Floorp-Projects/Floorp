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
public:
    static UniquePtr<SharedSurface_IOSurface> Create(const RefPtr<MacIOSurface>& ioSurf,
                                                     GLContext* gl,
                                                     bool hasAlpha);

    ~SharedSurface_IOSurface();

    virtual void LockProdImpl() override { }
    virtual void UnlockProdImpl() override { }

    virtual void Fence() override;
    virtual bool WaitSync() override { return true; }
    virtual bool PollSync() override { return true; }

    virtual bool CopyTexImage2D(GLenum target, GLint level, GLenum internalformat,
                                GLint x, GLint y, GLsizei width, GLsizei height,
                                GLint border) override;
    virtual bool ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                            GLenum format, GLenum type, GLvoid *pixels) override;

    virtual GLuint ProdTexture() override {
        return mProdTex;
    }

    virtual GLenum ProdTextureTarget() const override {
        return LOCAL_GL_TEXTURE_RECTANGLE_ARB;
    }

    static SharedSurface_IOSurface* Cast(SharedSurface *surf) {
        MOZ_ASSERT(surf->mType == SharedSurfaceType::IOSurface);
        return static_cast<SharedSurface_IOSurface*>(surf);
    }

    MacIOSurface* GetIOSurface() const {
        return mIOSurf;
    }

    virtual bool NeedsIndirectReads() const override {
        return true;
    }

private:
    SharedSurface_IOSurface(const RefPtr<MacIOSurface>& ioSurf,
                            GLContext* gl, const gfx::IntSize& size,
                            bool hasAlpha);

    RefPtr<MacIOSurface> mIOSurf;
    GLuint mProdTex;
};

class SurfaceFactory_IOSurface : public SurfaceFactory
{
public:
    // Infallible.
    static UniquePtr<SurfaceFactory_IOSurface> Create(GLContext* gl,
                                                      const SurfaceCaps& caps);
protected:
    const gfx::IntSize mMaxDims;

    SurfaceFactory_IOSurface(GLContext* gl,
                             const SurfaceCaps& caps,
                             const gfx::IntSize& maxDims)
        : SurfaceFactory(gl, SharedSurfaceType::IOSurface, caps)
        , mMaxDims(maxDims)
    {
    }

    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) override;
};

} /* namespace gfx */
} /* namespace mozilla */

#endif /* SHARED_SURFACEIO_H_ */
