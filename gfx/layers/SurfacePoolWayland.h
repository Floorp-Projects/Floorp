/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_SurfacePoolWayland_h
#define mozilla_layers_SurfacePoolWayland_h

#include <wayland-egl.h>

#include "GLContext.h"
#include "MozFramebuffer.h"
#include "mozilla/layers/SurfacePool.h"
#include "mozilla/widget/DMABufSurface.h"
#include "mozilla/widget/nsWaylandDisplay.h"
#include "mozilla/widget/WaylandShmBuffer.h"

namespace mozilla::layers {

using gfx::DrawTarget;
using gfx::IntPoint;
using gfx::IntRegion;
using gfx::IntSize;
using gfx::Rect;
using gl::GLContext;
using widget::nsWaylandDisplay;
using widget::WaylandShmBuffer;

typedef void (*CallbackFunc)(void* aData, uint32_t aTime);

class CallbackMultiplexHelper {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CallbackMultiplexHelper);

  explicit CallbackMultiplexHelper(CallbackFunc aCallbackFunc,
                                   void* aCallbackData);

  void Callback(uint32_t aTime);
  bool IsActive() { return mActive; }

 private:
  ~CallbackMultiplexHelper() = default;

  void RunCallback(uint32_t aTime);

  bool mActive = true;
  CallbackFunc mCallbackFunc = nullptr;
  void* mCallbackData = nullptr;
};

class NativeSurfaceWayland {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NativeSurfaceWayland);

  static RefPtr<NativeSurfaceWayland> Create(const IntSize& aSize,
                                             GLContext* aGL);

  virtual Maybe<GLuint> GetNextFramebuffer() { return Nothing(); };
  virtual RefPtr<DrawTarget> GetNextDrawTarget() { return nullptr; };

  virtual void Commit(const IntRegion& aInvalidRegion) = 0;
  virtual void NotifySurfaceReady(){};
  virtual void DestroyGLResources(){};

  void CreateSubsurface(wl_surface* aParentSurface);
  void ClearSubsurface();
  bool HasSubsurface() { return !!mWlSubsurface; }

  virtual void SetBufferTransformFlipped(bool aFlipped);
  void SetPosition(int aX, int aY);
  void SetViewportSourceRect(const Rect aSourceRect);
  void SetViewportDestinationSize(int aWidth, int aHeight);

  void RequestFrameCallback(
      const RefPtr<CallbackMultiplexHelper>& aMultiplexHelper);
  static void FrameCallbackHandler(void* aData, wl_callback* aCallback,
                                   uint32_t aTime);

  wl_surface* mWlSurface = nullptr;
  wl_subsurface* mWlSubsurface = nullptr;

 protected:
  explicit NativeSurfaceWayland(const IntSize& aSize);
  virtual ~NativeSurfaceWayland();

  void ClearSubsurface(const MutexAutoLock& aProofOfLock);
  void FrameCallbackHandler(wl_callback* aCallback, uint32_t aTime);

  Mutex mMutex;
  const IntSize mSize;
  wl_callback* mCallback = nullptr;
  wp_viewport* mViewport = nullptr;
  bool mBufferTransformFlipped = false;
  IntPoint mPosition = IntPoint(0, 0);
  Rect mViewportSourceRect = Rect(-1, -1, -1, -1);
  IntSize mViewportDestinationSize = IntSize(-1, -1);
  nsTArray<RefPtr<CallbackMultiplexHelper>> mCallbackMultiplexHelpers;
};

class NativeSurfaceWaylandSHM final : public NativeSurfaceWayland {
 public:
  RefPtr<DrawTarget> GetNextDrawTarget() override;
  void Commit(const IntRegion& aInvalidRegion) override;

  static void BufferReleaseCallbackHandler(void* aData, wl_buffer* aBuffer);

 private:
  friend RefPtr<NativeSurfaceWayland> NativeSurfaceWayland::Create(
      const IntSize& aSize, GLContext* aGL);

  explicit NativeSurfaceWaylandSHM(const IntSize& aSize);

  RefPtr<WaylandShmBuffer> ObtainBufferFromPool(
      const MutexAutoLock& aProofOfLock);
  void ReturnBufferToPool(const MutexAutoLock& aProofOfLock,
                          const RefPtr<WaylandShmBuffer>& aBuffer);
  void EnforcePoolSizeLimit(const MutexAutoLock& aProofOfLock);
  void BufferReleaseCallbackHandler(wl_buffer* aBuffer);

  nsTArray<RefPtr<WaylandShmBuffer>> mInUseBuffers;
  nsTArray<RefPtr<WaylandShmBuffer>> mAvailableBuffers;
  RefPtr<WaylandShmBuffer> mCurrentBuffer;
};

class WaylandDMABUFBuffer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WaylandDMABUFBuffer);

  static RefPtr<WaylandDMABUFBuffer> Create(const LayoutDeviceIntSize& aSize,
                                            GLContext* aGL);

  void DestroyGLResources();

  RefPtr<DMABufSurface> GetDMABufSurface() { return mDMABufSurface; };
  wl_buffer* GetWlBuffer() { return mDMABufSurface->GetWlBuffer(); };
  bool IsMatchingSize(const LayoutDeviceIntSize& aSize) {
    return aSize == mSize;
  }

  gl::MozFramebuffer* GetFramebuffer() { return mFB.get(); };
  void SetFramebuffer(UniquePtr<gl::MozFramebuffer> aFB) {
    mFB = std::move(aFB);
  };

 private:
  explicit WaylandDMABUFBuffer(const LayoutDeviceIntSize& aSize);
  ~WaylandDMABUFBuffer() = default;

  const LayoutDeviceIntSize mSize;
  RefPtr<DMABufSurfaceRGBA> mDMABufSurface;
  UniquePtr<gl::MozFramebuffer> mFB;
};

class NativeSurfaceWaylandDMABUF final : public NativeSurfaceWayland {
 public:
  Maybe<GLuint> GetNextFramebuffer() override;
  void Commit(const IntRegion& aInvalidRegion) override;
  void DestroyGLResources() override;

  static void BufferReleaseCallbackHandler(void* aData, wl_buffer* aBuffer);

 private:
  friend RefPtr<NativeSurfaceWayland> NativeSurfaceWayland::Create(
      const IntSize& aSize, GLContext* aGL);

  NativeSurfaceWaylandDMABUF(const IntSize& aSize, GLContext* aGL);
  ~NativeSurfaceWaylandDMABUF() = default;

  RefPtr<WaylandDMABUFBuffer> ObtainBufferFromPool(
      const MutexAutoLock& aProofOfLock);
  void ReturnBufferToPool(const MutexAutoLock& aProofOfLock,
                          const RefPtr<WaylandDMABUFBuffer>& aBuffer);
  void EnforcePoolSizeLimit(const MutexAutoLock& aProofOfLock);
  UniquePtr<gl::MozFramebuffer> CreateFramebufferForTexture(
      const MutexAutoLock& aProofOfLock, GLContext* aGL, const IntSize& aSize,
      GLuint aTexture);
  RefPtr<gl::DepthAndStencilBuffer> GetDepthBufferForSharing(
      const MutexAutoLock& aProofOfLock, GLContext* aGL, const IntSize& aSize);
  void BufferReleaseCallbackHandler(wl_buffer* aBuffer);

  RefPtr<GLContext> mGL;

  nsTArray<RefPtr<WaylandDMABUFBuffer>> mInUseBuffers;
  nsTArray<RefPtr<WaylandDMABUFBuffer>> mAvailableBuffers;
  RefPtr<WaylandDMABUFBuffer> mCurrentBuffer;

  struct DepthBufferEntry final {
    RefPtr<GLContext> mGL;
    IntSize mSize;
    WeakPtr<gl::DepthAndStencilBuffer> mBuffer;
  };

  nsTArray<DepthBufferEntry> mDepthBuffers;
};

class SurfacePoolWayland final : public SurfacePool {
 public:
  // Get a handle for a new window. aGL can be nullptr.
  RefPtr<SurfacePoolHandle> GetHandleForGL(GLContext* aGL) override;

  // Destroy all GL resources associated with aGL managed by this pool.
  void DestroyGLResourcesForContext(GLContext* aGL) override;

 private:
  friend class SurfacePoolHandleWayland;
  friend RefPtr<SurfacePool> SurfacePool::Create(size_t aPoolSizeLimit);

  explicit SurfacePoolWayland(size_t aPoolSizeLimit);

  RefPtr<NativeSurfaceWayland> ObtainSurfaceFromPool(const IntSize& aSize,
                                                     GLContext* aGL);
  void ReturnSurfaceToPool(const RefPtr<NativeSurfaceWayland>& aSurface);
  void EnforcePoolSizeLimit();

  struct SurfacePoolEntry final {
    const IntSize mSize;
    const RefPtr<NativeSurfaceWayland> mNativeSurface;  // non-null
    const RefPtr<gl::GLContext> mGL;
  };

  bool CanRecycleSurfaceForRequest(const SurfacePoolEntry& aEntry,
                                   const IntSize& aSize, GLContext* aGL);

  // Stores the entries for surfaces that are in use by NativeLayerWayland, i.e.
  // an entry is inside mInUseEntries between calls to ObtainSurfaceFromPool()
  // and ReturnSurfaceToPool().
  std::unordered_map<NativeSurfaceWayland*, SurfacePoolEntry> mInUseEntries;

  // Stores entries which are no longer in use by NativeLayerWayland but are
  // still in use by the window server, i.e. for which
  // WaylandSurfaceIsInUse(pendingSurfaceEntry.mEntry.mNativeSurface.get()) TODO
  // still returns true. These entries are checked once per frame inside
  // CollectPendingSurfaces(), and returned to mAvailableEntries once the
  // window server is done.
  // nsTArray<SurfacePoolEntry> mPendingEntries;

  // Stores entries which are available for recycling. These entries are not
  // in use by a NativeLayerWayland or by the window server.
  nsTArray<SurfacePoolEntry> mAvailableEntries;
  size_t mPoolSizeLimit;
};

// A surface pool handle that is stored on NativeLayerWayland and keeps the
// SurfacePool alive.
class SurfacePoolHandleWayland final : public SurfacePoolHandle {
 public:
  SurfacePoolHandleWayland* AsSurfacePoolHandleWayland() override {
    return this;
  }

  RefPtr<NativeSurfaceWayland> ObtainSurfaceFromPool(const IntSize& aSize);
  void ReturnSurfaceToPool(const RefPtr<NativeSurfaceWayland>& aSurface);
  const auto& gl() { return mGL; }

  RefPtr<SurfacePool> Pool() override { return mPool; }
  void OnBeginFrame() override;
  void OnEndFrame() override;

 private:
  friend class SurfacePoolWayland;
  SurfacePoolHandleWayland(RefPtr<SurfacePoolWayland> aPool, GLContext* aGL);

  const RefPtr<SurfacePoolWayland> mPool;
  const RefPtr<GLContext> mGL;
};

}  // namespace mozilla::layers

#endif
