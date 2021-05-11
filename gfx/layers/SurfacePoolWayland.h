/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_SurfacePoolWayland_h
#define mozilla_layers_SurfacePoolWayland_h

#include <wayland-egl.h>

#include "mozilla/layers/SurfacePool.h"
#include "mozilla/widget/nsWaylandDisplay.h"

namespace mozilla {

namespace layers {

class NativeSurfaceWayland {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NativeSurfaceWayland);

  EGLSurface GetEGLSurface() { return mEGLSurface; }

  struct wl_surface* mWlSurface = nullptr;
  struct wl_subsurface* mWlSubsurface = nullptr;
  struct wp_viewport* mViewport = nullptr;

 private:
  friend class SurfacePoolWayland;
  NativeSurfaceWayland(const gfx::IntSize& aSize, gl::GLContext* aGL);
  ~NativeSurfaceWayland();

  gl::GLContext* mGL = nullptr;
  struct wl_egl_window* mEGLWindow = nullptr;
  EGLSurface mEGLSurface = nullptr;
};

class SurfacePoolWayland final : public SurfacePool {
 public:
  // Get a handle for a new window. aGL can be nullptr.
  RefPtr<SurfacePoolHandle> GetHandleForGL(gl::GLContext* aGL) override;

  // Destroy all GL resources associated with aGL managed by this pool.
  void DestroyGLResourcesForContext(gl::GLContext* aGL) override;

 private:
  friend class SurfacePoolHandleWayland;
  friend RefPtr<SurfacePool> SurfacePool::Create(size_t aPoolSizeLimit);

  explicit SurfacePoolWayland(size_t aPoolSizeLimit);

  RefPtr<NativeSurfaceWayland> ObtainSurfaceFromPool(const gfx::IntSize& aSize,
                                                     gl::GLContext* aGL);
  void ReturnSurfaceToPool(const RefPtr<NativeSurfaceWayland>& aSurface);
  void EnforcePoolSizeLimit();

  struct SurfacePoolEntry {
    gfx::IntSize mSize;
    RefPtr<NativeSurfaceWayland> mNativeSurface;  // non-null
  };

  bool CanRecycleSurfaceForRequest(const SurfacePoolEntry& aEntry,
                                   const gfx::IntSize& aSize,
                                   gl::GLContext* aGL);

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

  RefPtr<NativeSurfaceWayland> ObtainSurfaceFromPool(const gfx::IntSize& aSize);
  void ReturnSurfaceToPool(const RefPtr<NativeSurfaceWayland>& aSurface);
  const auto& gl() { return mGL; }

  RefPtr<SurfacePool> Pool() override { return mPool; }
  void OnBeginFrame() override;
  void OnEndFrame() override;

 private:
  friend class SurfacePoolWayland;
  SurfacePoolHandleWayland(RefPtr<SurfacePoolWayland> aPool,
                           gl::GLContext* aGL);

  const RefPtr<SurfacePoolWayland> mPool;
  const RefPtr<gl::GLContext> mGL;
};

}  // namespace layers
}  // namespace mozilla

#endif
