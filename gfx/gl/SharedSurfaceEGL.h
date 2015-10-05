/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_EGL_H_
#define SHARED_SURFACE_EGL_H_

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "nsAutoPtr.h"
#include "SharedSurface.h"

namespace mozilla {
namespace gl {

class GLContext;
class GLLibraryEGL;
class TextureGarbageBin;

class SharedSurface_EGLImage
    : public SharedSurface
{
public:
    static UniquePtr<SharedSurface_EGLImage> Create(GLContext* prodGL,
                                                    const GLFormats& formats,
                                                    const gfx::IntSize& size,
                                                    bool hasAlpha,
                                                    EGLContext context);

    static SharedSurface_EGLImage* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->mType == SharedSurfaceType::EGLImageShare);

        return (SharedSurface_EGLImage*)surf;
    }

    static bool HasExtensions(GLLibraryEGL* egl, GLContext* gl);

protected:
    mutable Mutex mMutex;
    GLLibraryEGL* const mEGL;
    const GLFormats mFormats;
    GLuint mProdTex;
public:
    const EGLImage mImage;
protected:
    GLContext* mCurConsGL;
    GLuint mConsTex;
    nsRefPtr<TextureGarbageBin> mGarbageBin;
    EGLSync mSync;

    SharedSurface_EGLImage(GLContext* gl,
                           GLLibraryEGL* egl,
                           const gfx::IntSize& size,
                           bool hasAlpha,
                           const GLFormats& formats,
                           GLuint prodTex,
                           EGLImage image);

    EGLDisplay Display() const;
    void UpdateProdTexture(const MutexAutoLock& curAutoLock);

public:
    virtual ~SharedSurface_EGLImage();

    virtual void LockProdImpl() override {}
    virtual void UnlockProdImpl() override {}

    virtual void Fence() override;
    virtual bool WaitSync() override;
    virtual bool PollSync() override;

    virtual GLuint ProdTexture() override {
      return mProdTex;
    }

    // Implementation-specific functions below:
    // Returns texture and target
    void AcquireConsumerTexture(GLContext* consGL, GLuint* out_texture, GLuint* out_target);

    virtual bool ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor) override;
};



class SurfaceFactory_EGLImage
    : public SurfaceFactory
{
public:
    // Fallible:
    static UniquePtr<SurfaceFactory_EGLImage> Create(GLContext* prodGL,
                                                     const SurfaceCaps& caps,
                                                     const RefPtr<layers::ISurfaceAllocator>& allocator,
                                                     const layers::TextureFlags& flags);

protected:
    const EGLContext mContext;

    SurfaceFactory_EGLImage(GLContext* prodGL, const SurfaceCaps& caps,
                            const RefPtr<layers::ISurfaceAllocator>& allocator,
                            const layers::TextureFlags& flags,
                            EGLContext context)
        : SurfaceFactory(SharedSurfaceType::EGLImageShare, prodGL, caps, allocator, flags)
        , mContext(context)
    { }

public:
    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) override {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_EGLImage::Create(mGL, mFormats, size, hasAlpha, mContext);
    }
};

} // namespace gl

} /* namespace mozilla */

#endif /* SHARED_SURFACE_EGL_H_ */
