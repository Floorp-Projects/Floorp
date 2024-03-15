/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_SurfacePoolCA_h
#define mozilla_layers_SurfacePoolCA_h

#include <IOSurface/IOSurfaceRef.h>

#include <deque>
#include <unordered_map>

#include "mozilla/Atomics.h"
#include "mozilla/DataMutex.h"

#include "mozilla/layers/SurfacePool.h"
#include "CFTypeRefPtr.h"
#include "MozFramebuffer.h"
#include "nsISupportsImpl.h"

namespace mozilla {

namespace gl {
class MozFramebuffer;
}  // namespace gl

namespace layers {

class SurfacePoolHandleCA;
struct SurfacePoolCAWrapperForGL;

// An implementation of SurfacePool for IOSurfaces and GL framebuffers.
// The goal of having this pool is to avoid creating and destroying IOSurfaces
// and framebuffers frequently, because doing so is expensive.
// SurfacePoolCA is threadsafe. All its data is wrapped inside LockedPool, and
// each access to LockedPool is guarded with a lock through DataMutex.
//
// The pool satisfies the following requirements:
//  - It can be shared across windows, even across windows with different
//  GLContexts.
//  - The number of unused surfaces that are available for recycling is capped
//  to a fixed value per pool, regardless of how many windows use that pool.
//  - When all windows are closed (all handles are gone), no surfaces are kept
//  alive (the pool is destroyed).
//  - There is an explicit way of deleting GL resources for a GLContext so that
//  it can happen at a deterministic time on the right thread.
//  - Additionally, once a GLContext is no longer being used in any window
//  (really: any pool handle), all surface-associated GL resources of that
//  context are destroyed.
//  - For every IOSurface, only one set of GL resources is in existence at any
//  given time. We don't want there to be framebuffers in two different
//  GLContexts for one surface.
//  - We do not want to recycle an IOSurface that currently has GL resources of
//  context A for a pool handle that uses context B.
//  - We need to delay IOSurface recycling until the window server is done with
//  the surface (`!IOSurfaceIsInUse(surf)`)
class SurfacePoolCA final : public SurfacePool {
 public:
  // Get a handle for a new window. aGL can be nullptr.
  RefPtr<SurfacePoolHandle> GetHandleForGL(gl::GLContext* aGL) override;

  // Destroy all GL resources associated with aGL managed by this pool.
  void DestroyGLResourcesForContext(gl::GLContext* aGL) override;

 private:
  friend struct SurfacePoolCAWrapperForGL;
  friend class SurfacePoolHandleCA;
  friend RefPtr<SurfacePool> SurfacePool::Create(size_t aPoolSizeLimit);

  explicit SurfacePoolCA(size_t aPoolSizeLimit);
  ~SurfacePoolCA() override;

  // Get an existing surface of aSize from the pool or create a new surface.
  // The returned surface is guaranteed not to be in use by the window server.
  CFTypeRefPtr<IOSurfaceRef> ObtainSurfaceFromPool(const gfx::IntSize& aSize,
                                                   gl::GLContext* aGL);

  // Place a surface that was previously obtained from this pool back into the
  // pool. aSurface may or may not be in use by the window server.
  void ReturnSurfaceToPool(CFTypeRefPtr<IOSurfaceRef> aSurface);

  // Re-run checks whether the window server still uses IOSurfaces which are
  // eligible for recycling. The purpose of the "generation" counter is to
  // reduce the number of calls to IOSurfaceIsInUse in a scenario where many
  // windows / handles are calling CollectPendingSurfaces in the same frame
  // (i.e. multiple simultaneously-animating windows).
  uint64_t CollectPendingSurfaces(uint64_t aCheckGenerationsUpTo);

  // Enforce the pool size limit by evicting surfaces as necessary. This should
  // happen at the end of the frame so that we can temporarily exceed the limit
  // within a frame.
  void EnforcePoolSizeLimit();

  // Get or create the framebuffer for the given surface and GL context.
  // The returned framebuffer handle will become invalid once
  // DestroyGLResourcesForContext or DecrementGLContextHandleCount are called.
  // The framebuffer's depth buffer (if present) may be shared between multiple
  // framebuffers! Do not assume anything about the depth buffer's existing
  // contents (i.e. clear it at the beginning of the draw), and do not
  // interleave drawing commands to different framebuffers in such a way that
  // the shared depth buffer could cause trouble.
  Maybe<GLuint> GetFramebufferForSurface(CFTypeRefPtr<IOSurfaceRef> aSurface,
                                         gl::GLContext* aGL,
                                         bool aNeedsDepthBuffer);

  // Called by the destructor of SurfacePoolCAWrapperForGL so that we can clear
  // our weak reference to it and delete GL resources.
  void OnWrapperDestroyed(gl::GLContext* aGL,
                          SurfacePoolCAWrapperForGL* aWrapper);

  // The actual pool implementation lives in LockedPool, which is accessed in
  // a thread-safe manner.
  struct LockedPool {
    explicit LockedPool(size_t aPoolSizeLimit);
    LockedPool(LockedPool&&) = default;
    ~LockedPool();

    RefPtr<SurfacePoolCAWrapperForGL> GetWrapperForGL(SurfacePoolCA* aPool,
                                                      gl::GLContext* aGL);
    void DestroyGLResourcesForContext(gl::GLContext* aGL);

    CFTypeRefPtr<IOSurfaceRef> ObtainSurfaceFromPool(const gfx::IntSize& aSize,
                                                     gl::GLContext* aGL);
    void ReturnSurfaceToPool(CFTypeRefPtr<IOSurfaceRef> aSurface);
    uint64_t CollectPendingSurfaces(uint64_t aCheckGenerationsUpTo);
    void EnforcePoolSizeLimit();
    Maybe<GLuint> GetFramebufferForSurface(CFTypeRefPtr<IOSurfaceRef> aSurface,
                                           gl::GLContext* aGL,
                                           bool aNeedsDepthBuffer);
    void OnWrapperDestroyed(gl::GLContext* aGL,
                            SurfacePoolCAWrapperForGL* aWrapper);
    uint64_t EstimateTotalMemory();

    uint64_t mCollectionGeneration = 0;

   protected:
    struct GLResourcesForSurface {
      RefPtr<gl::GLContext> mGLContext;            // non-null
      UniquePtr<gl::MozFramebuffer> mFramebuffer;  // non-null
    };

    struct SurfacePoolEntry {
      gfx::IntSize mSize;
      CFTypeRefPtr<IOSurfaceRef> mIOSurface;  // non-null
      Maybe<GLResourcesForSurface> mGLResources;
    };

    struct PendingSurfaceEntry {
      SurfacePoolEntry mEntry;
      // The value of LockedPool::mCollectionGeneration at the time
      // IOSurfaceIsInUse was last called for mEntry.mIOSurface.
      uint64_t mPreviousCheckGeneration;
      // The number of times an IOSurfaceIsInUse check has been performed.
      uint64_t mCheckCount;
    };

    template <typename F>
    void MutateEntryStorage(const char* aMutationType,
                            const gfx::IntSize& aSize, F aFn);

    template <typename F>
    void ForEachEntry(F aFn);

    bool CanRecycleSurfaceForRequest(const SurfacePoolEntry& aEntry,
                                     const gfx::IntSize& aSize,
                                     gl::GLContext* aGL);

    RefPtr<gl::DepthAndStencilBuffer> GetDepthBufferForSharing(
        gl::GLContext* aGL, const gfx::IntSize& aSize);
    UniquePtr<gl::MozFramebuffer> CreateFramebufferForTexture(
        gl::GLContext* aGL, const gfx::IntSize& aSize, GLuint aTexture,
        bool aNeedsDepthBuffer);

    // Every IOSurface that is managed by the pool is wrapped in a
    // SurfacePoolEntry object. Every entry is stored in one of three buckets at
    // any given time: mInUseEntries, mPendingEntries, or mAvailableEntries. All
    // mutations to these buckets are performed via calls to
    // MutateEntryStorage(). Entries can move between the buckets in the
    // following ways:
    //
    //                                [new]
    //                                  | Create
    //                                  v
    //    +----------------------------------------------------------------+
    //    |                       mInUseEntries                            |
    //    +------+------------------------------+--------------------------+
    //           |            ^                 | Start waiting for
    //           |            | Recycle         v
    //           |            |              +-----------------------------+
    //           |            |              |       mPendingEntries       |
    //           |            |              +--+--------------------+-----+
    //           | Retain     |                 | Stop waiting for   |
    //           v            |                 v                    |
    //    +-------------------+-------------------------+            |
    //    |              mAvailableEntries              |            |
    //    +-----------------------------+---------------+            |
    //                                  | Evict                      | Eject
    //                                  v                            v
    //                             [destroyed]                  [destroyed]
    //
    // Each arrow corresponds to one invocation of MutateEntryStorage() with the
    // arrow's label passed as the aMutationType string parameter.

    // Stores the entries for surfaces that are in use by NativeLayerCA, i.e. an
    // entry is inside mInUseEntries between calls to ObtainSurfaceFromPool()
    // and ReturnSurfaceToPool().
    std::unordered_map<CFTypeRefPtr<IOSurfaceRef>, SurfacePoolEntry>
        mInUseEntries;

    // Stores entries which are no longer in use by NativeLayerCA but are still
    // in use by the window server, i.e. for which
    // IOSurfaceIsInUse(pendingSurfaceEntry.mEntry.mIOSurface.get()) still
    // returns true. These entries are checked once per frame inside
    // CollectPendingSurfaces(), and returned to mAvailableEntries once the
    // window server is done.
    nsTArray<PendingSurfaceEntry> mPendingEntries;

    // Stores entries which are available for recycling. These entries are not
    // in use by a NativeLayerCA or by the window server.
    nsTArray<SurfacePoolEntry> mAvailableEntries;

    // Keeps weak references to SurfacePoolCAWrapperForGL instances.
    // For each GLContext* value (including nullptr), only one wrapper can
    // exist at any time. The wrapper keeps a strong reference to us and
    // notifies us when it gets destroyed. At that point we can call
    // DestroyGLResourcesForContext because we know no other SurfaceHandles for
    // that context exist.
    std::unordered_map<gl::GLContext*, SurfacePoolCAWrapperForGL*> mWrappers;
    size_t mPoolSizeLimit = 0;

    struct DepthBufferEntry {
      RefPtr<gl::GLContext> mGLContext;
      gfx::IntSize mSize;
      WeakPtr<gl::DepthAndStencilBuffer> mBuffer;
    };

    nsTArray<DepthBufferEntry> mDepthBuffers;
  };

  DataMutex<LockedPool> mPool;
};

// One process-wide instance per (SurfacePoolCA*, GLContext*) pair.
// Keeps the SurfacePool alive, and the SurfacePool has a weak reference to the
// wrapper so that it can ensure that there's only one wrapper for it per
// GLContext* at any time.
struct SurfacePoolCAWrapperForGL {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SurfacePoolCAWrapperForGL);

  const RefPtr<SurfacePoolCA> mPool;  // non-null
  const RefPtr<gl::GLContext> mGL;    // can be null

  SurfacePoolCAWrapperForGL(SurfacePoolCA* aPool, gl::GLContext* aGL)
      : mPool(aPool), mGL(aGL) {}

 protected:
  ~SurfacePoolCAWrapperForGL() { mPool->OnWrapperDestroyed(mGL, this); }
};

// A surface pool handle that is stored on NativeLayerCA and keeps the
// SurfacePool alive.
class SurfacePoolHandleCA final : public SurfacePoolHandle {
 public:
  SurfacePoolHandleCA* AsSurfacePoolHandleCA() override { return this; }
  const auto& gl() { return mPoolWrapper->mGL; }
  CFTypeRefPtr<IOSurfaceRef> ObtainSurfaceFromPool(const gfx::IntSize& aSize);
  void ReturnSurfaceToPool(CFTypeRefPtr<IOSurfaceRef> aSurface);
  Maybe<GLuint> GetFramebufferForSurface(CFTypeRefPtr<IOSurfaceRef> aSurface,
                                         bool aNeedsDepthBuffer);
  RefPtr<SurfacePool> Pool() override { return mPoolWrapper->mPool; }
  void OnBeginFrame() override;
  void OnEndFrame() override;

 private:
  friend class SurfacePoolCA;
  SurfacePoolHandleCA(RefPtr<SurfacePoolCAWrapperForGL>&& aPoolWrapper,
                      uint64_t aCurrentCollectionGeneration);
  ~SurfacePoolHandleCA() override;

  const RefPtr<SurfacePoolCAWrapperForGL> mPoolWrapper;
  DataMutex<uint64_t> mPreviousFrameCollectionGeneration;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_SurfacePoolCA_h
