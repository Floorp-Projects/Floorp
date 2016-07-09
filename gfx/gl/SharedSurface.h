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
#include <set>
#include <stdint.h>

#include "GLContextTypes.h"
#include "GLDefs.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/Mutex.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "ScopedGLHelpers.h"
#include "SurfaceTypes.h"

class nsIThread;

namespace mozilla {
namespace gfx {
class DataSourceSurface;
class DrawTarget;
} // namespace gfx

namespace layers {
class ClientIPCAllocator;
class SharedSurfaceTextureClient;
enum class TextureFlags : uint32_t;
class SurfaceDescriptor;
class TextureClient;
} // namespace layers

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
    const bool mCanRecycle;
protected:
    bool mIsLocked;
    bool mIsProducerAcquired;
#ifdef DEBUG
    nsIThread* const mOwningThread;
#endif

    SharedSurface(SharedSurfaceType type,
                  AttachmentType attachType,
                  GLContext* gl,
                  const gfx::IntSize& size,
                  bool hasAlpha,
                  bool canRecycle);

public:
    virtual ~SharedSurface() {
    }

    // Specifies to the TextureClient any flags which
    // are required by the SharedSurface backend.
    virtual layers::TextureFlags GetTextureFlags() const;

    bool IsLocked() const { return mIsLocked; }
    bool IsProducerAcquired() const { return mIsProducerAcquired; }

    // This locks the SharedSurface as the production buffer for the context.
    // This is needed by backends which use PBuffers and/or EGLSurfaces.
    void LockProd();

    // Unlocking is harmless if we're already unlocked.
    void UnlockProd();

protected:
    virtual void LockProdImpl() = 0;
    virtual void UnlockProdImpl() = 0;

    virtual void ProducerAcquireImpl() = 0;
    virtual void ProducerReleaseImpl() = 0;
    virtual void ProducerReadAcquireImpl() { ProducerAcquireImpl(); }
    virtual void ProducerReadReleaseImpl() { ProducerReleaseImpl(); }

public:
    void ProducerAcquire() {
        MOZ_ASSERT(!mIsProducerAcquired);
        ProducerAcquireImpl();
        mIsProducerAcquired = true;
    }
    void ProducerRelease() {
        MOZ_ASSERT(mIsProducerAcquired);
        ProducerReleaseImpl();
        mIsProducerAcquired = false;
    }
    void ProducerReadAcquire() {
        MOZ_ASSERT(!mIsProducerAcquired);
        ProducerReadAcquireImpl();
        mIsProducerAcquired = true;
    }
    void ProducerReadRelease() {
        MOZ_ASSERT(mIsProducerAcquired);
        ProducerReadReleaseImpl();
        mIsProducerAcquired = false;
    }

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
        MOZ_CRASH("GFX: Did you forget to override this function?");
    }

    virtual GLuint ProdRenderbuffer() {
        MOZ_ASSERT(mAttachType == AttachmentType::GLRenderbuffer);
        MOZ_CRASH("GFX: Did you forget to override this function?");
    }

    virtual bool CopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x,
                                GLint y, GLsizei width, GLsizei height, GLint border)
    {
        return false;
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

    virtual bool ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor) = 0;

    virtual bool ReadbackBySharedHandle(gfx::DataSourceSurface* out_surface) {
        return false;
    }
};

template<typename T>
class RefSet
{
    std::set<T*> mSet;

public:
    ~RefSet() {
        clear();
    }

    auto begin() -> decltype(mSet.begin()) {
        return mSet.begin();
    }

    void clear() {
        for (auto itr = mSet.begin(); itr != mSet.end(); ++itr) {
            (*itr)->Release();
        }
        mSet.clear();
    }

    bool empty() const {
        return mSet.empty();
    }

    bool insert(T* x) {
        if (mSet.insert(x).second) {
            x->AddRef();
            return true;
        }

        return false;
    }

    bool erase(T* x) {
        if (mSet.erase(x)) {
            x->Release();
            return true;
        }

        return false;
    }
};

template<typename T>
class RefQueue
{
    std::queue<T*> mQueue;

public:
    ~RefQueue() {
        clear();
    }

    void clear() {
        while (!empty()) {
            pop();
        }
    }

    bool empty() const {
        return mQueue.empty();
    }

    size_t size() const {
        return mQueue.size();
    }

    void push(T* x) {
        mQueue.push(x);
        x->AddRef();
    }

    T* front() const {
        return mQueue.front();
    }

    void pop() {
        T* x = mQueue.front();
        x->Release();
        mQueue.pop();
    }
};

class SurfaceFactory : public SupportsWeakPtr<SurfaceFactory>
{
public:
    // Should use the VIRTUAL version, but it's currently incompatible
    // with SupportsWeakPtr. (bug 1049278)
    MOZ_DECLARE_WEAKREFERENCE_TYPENAME(SurfaceFactory)

    const SharedSurfaceType mType;
    GLContext* const mGL;
    const SurfaceCaps mCaps;
    const RefPtr<layers::ClientIPCAllocator> mAllocator;
    const layers::TextureFlags mFlags;
    const GLFormats mFormats;
    Mutex mMutex;
protected:
    SurfaceCaps mDrawCaps;
    SurfaceCaps mReadCaps;
    RefQueue<layers::SharedSurfaceTextureClient> mRecycleFreePool;
    RefSet<layers::SharedSurfaceTextureClient> mRecycleTotalPool;

    SurfaceFactory(SharedSurfaceType type, GLContext* gl, const SurfaceCaps& caps,
                   const RefPtr<layers::ClientIPCAllocator>& allocator,
                   const layers::TextureFlags& flags);

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

    void StartRecycling(layers::SharedSurfaceTextureClient* tc);
    void SetRecycleCallback(layers::SharedSurfaceTextureClient* tc);
    void StopRecycling(layers::SharedSurfaceTextureClient* tc);

public:
    UniquePtr<SharedSurface> NewSharedSurface(const gfx::IntSize& size);
    //already_AddRefed<ShSurfHandle> NewShSurfHandle(const gfx::IntSize& size);
    already_AddRefed<layers::SharedSurfaceTextureClient> NewTexClient(const gfx::IntSize& size);

    static void RecycleCallback(layers::TextureClient* tc, void* /*closure*/);

    // Auto-deletes surfs of the wrong type.
    bool Recycle(layers::SharedSurfaceTextureClient* texClient);
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
    explicit ScopedReadbackFB(SharedSurface* src);
    ~ScopedReadbackFB();
};

bool ReadbackSharedSurface(SharedSurface* src, gfx::DrawTarget* dst);
uint32_t ReadPixel(SharedSurface* src);

} // namespace gl
} // namespace mozilla

#endif // SHARED_SURFACE_H_
