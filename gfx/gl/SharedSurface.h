/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* SharedSurface abstracts an actual surface (can be a GL texture, but
 * not necessarily) that handles sharing.
 * Its specializations are:
 *     SharedSurface_Basic (client-side bitmap, does readback)
 *     SharedSurface_GLTexture
 *     SharedSurface_EGLImage
 *     SharedSurface_ANGLEShareHandle
 */

#ifndef SHARED_SURFACE_H_
#define SHARED_SURFACE_H_

#include <queue>
#include <stdint.h>

#include "GLContextTypes.h"
#include "GLDefs.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "ScopedGLHelpers.h"
#include "SurfaceTypes.h"

class nsIThread;

namespace mozilla {
namespace gl {

class GLContext;
class SurfaceFactory;
class ShSurfHandle;

class SharedSurface
{
public:
    static void ProdCopy(SharedSurface* src, SharedSurface* dest,
                         SurfaceFactory* factory);

    const SharedSurfaceType mType;
    const AttachmentType mAttachType;
    GLContext* const mGL;
    const gfx::IntSize mSize;
    const bool mHasAlpha;
protected:
    bool mIsLocked;
    DebugOnly<nsIThread* const> mOwningThread;

    SharedSurface(SharedSurfaceType type,
                  AttachmentType attachType,
                  GLContext* gl,
                  const gfx::IntSize& size,
                  bool hasAlpha);

public:
    virtual ~SharedSurface() {
    }

    bool IsLocked() const {
        return mIsLocked;
    }

    // This locks the SharedSurface as the production buffer for the context.
    // This is needed by backends which use PBuffers and/or EGLSurfaces.
    void LockProd();

    // Unlocking is harmless if we're already unlocked.
    void UnlockProd();

protected:
    virtual void LockProdImpl() = 0;
    virtual void UnlockProdImpl() = 0;

public:
    virtual void Fence() = 0;
    virtual bool WaitSync() = 0;
    virtual bool PollSync() = 0;

    // Use these if you can. They can only be called from the Content
    // thread, though!
    void Fence_ContentThread();
    bool WaitSync_ContentThread();
    bool PollSync_ContentThread();

protected:
    virtual void Fence_ContentThread_Impl() {
        Fence();
    }
    virtual bool WaitSync_ContentThread_Impl() {
        return WaitSync();
    }
    virtual bool PollSync_ContentThread_Impl() {
        return PollSync();
    }

public:
    // This function waits until the buffer is no longer being used.
    // To optimize the performance, some implementaions recycle SharedSurfaces
    // even when its buffer is still being used.
    virtual void WaitForBufferOwnership() {}

    // For use when AttachType is correct.
    virtual GLenum ProdTextureTarget() const {
        MOZ_ASSERT(mAttachType == AttachmentType::GLTexture);
        return LOCAL_GL_TEXTURE_2D;
    }

    virtual GLuint ProdTexture() {
        MOZ_ASSERT(mAttachType == AttachmentType::GLTexture);
        MOZ_CRASH("Did you forget to override this function?");
    }

    virtual GLuint ProdRenderbuffer() {
        MOZ_ASSERT(mAttachType == AttachmentType::GLRenderbuffer);
        MOZ_CRASH("Did you forget to override this function?");
    }

    virtual bool ReadPixels(GLint x, GLint y,
                            GLsizei width, GLsizei height,
                            GLenum format, GLenum type,
                            GLvoid* pixels)
    {
        return false;
    }

    virtual bool NeedsIndirectReads() const {
        return false;
    }
};

template<typename T>
class UniquePtrQueue
{
    std::queue<T*> mQueue;

public:
    ~UniquePtrQueue() {
        MOZ_ASSERT(Empty());
    }

    bool Empty() const {
        return mQueue.empty();
    }

    void Push(UniquePtr<T> up) {
        T* p = up.release();
        mQueue.push(p);
    }

    UniquePtr<T> Pop() {
        UniquePtr<T> ret;

        if (!mQueue.empty()) {
            ret.reset(mQueue.front());
            mQueue.pop();
        }

        return Move(ret);
    }
};

class SurfaceFactory : public SupportsWeakPtr<SurfaceFactory>
{
public:
    // Should use the VIRTUAL version, but it's currently incompatible
    // with SupportsWeakPtr. (bug 1049278)
    MOZ_DECLARE_REFCOUNTED_TYPENAME(SurfaceFactory)

    GLContext* const mGL;
    const SurfaceCaps mCaps;
    const SharedSurfaceType mType;
    const GLFormats mFormats;

protected:
    SurfaceCaps mDrawCaps;
    SurfaceCaps mReadCaps;

    SurfaceFactory(GLContext* gl,
                   SharedSurfaceType type,
                   const SurfaceCaps& caps);

public:
    virtual ~SurfaceFactory();

    const SurfaceCaps& DrawCaps() const {
        return mDrawCaps;
    }

    const SurfaceCaps& ReadCaps() const {
        return mReadCaps;
    }

protected:
    virtual UniquePtr<SharedSurface> CreateShared(const gfx::IntSize& size) = 0;

    UniquePtrQueue<SharedSurface> mScraps;

public:
    UniquePtr<SharedSurface> NewSharedSurface(const gfx::IntSize& size);
    TemporaryRef<ShSurfHandle> NewShSurfHandle(const gfx::IntSize& size);

    // Auto-deletes surfs of the wrong type.
    void Recycle(UniquePtr<SharedSurface> surf);
};

class ShSurfHandle : public RefCounted<ShSurfHandle>
{
public:
    MOZ_DECLARE_REFCOUNTED_TYPENAME(ShSurfHandle)

private:
    const WeakPtr<SurfaceFactory> mFactory;
    UniquePtr<SharedSurface> mSurf;

public:
    ShSurfHandle(SurfaceFactory* factory, UniquePtr<SharedSurface> surf)
        : mFactory(factory)
        , mSurf(Move(surf))
    {
        MOZ_ASSERT(mFactory);
        MOZ_ASSERT(mSurf);
    }

    ~ShSurfHandle() {
        if (mFactory) {
            mFactory->Recycle(Move(mSurf));
        }
    }

    SharedSurface* Surf() const {
        MOZ_ASSERT(mSurf.get());
        return mSurf.get();
    }
};

class ScopedReadbackFB
{
    GLContext* const mGL;
    ScopedBindFramebuffer mAutoFB;
    GLuint mTempFB;
    GLuint mTempTex;
    SharedSurface* mSurfToUnlock;
    SharedSurface* mSurfToLock;

public:
    ScopedReadbackFB(SharedSurface* src);
    ~ScopedReadbackFB();
};

} // namespace gl
} // namespace mozilla

#endif // SHARED_SURFACE_H_
