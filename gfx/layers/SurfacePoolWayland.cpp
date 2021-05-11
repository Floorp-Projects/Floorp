/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nullptr; c-basic-offset: 2
 * -*- This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/SurfacePoolWayland.h"

#include "GLContextEGL.h"

namespace mozilla::layers {

using gl::GLContext;
using gl::GLContextEGL;

NativeSurfaceWayland::NativeSurfaceWayland(const gfx::IntSize& aSize,
                                           GLContext* aGL)
    : mGL(aGL) {
  wl_compositor* compositor = widget::WaylandDisplayGet()->GetCompositor();
  mWlSurface = wl_compositor_create_surface(compositor);

  wl_region* region = wl_compositor_create_region(compositor);
  wl_surface_set_input_region(mWlSurface, region);
  wl_region_destroy(region);

  wp_viewporter* viewporter = widget::WaylandDisplayGet()->GetViewporter();
  MOZ_ASSERT(viewporter);
  mViewport = wp_viewporter_get_viewport(viewporter, mWlSurface);

  mEGLWindow = wl_egl_window_create(mWlSurface, aSize.width, aSize.height);

  GLContextEGL* egl = GLContextEGL::Cast(mGL);
  mEGLSurface =
      egl->mEgl->fCreateWindowSurface(egl->mConfig, mEGLWindow, nullptr);
  MOZ_ASSERT(mEGLSurface != EGL_NO_SURFACE);
}

NativeSurfaceWayland::~NativeSurfaceWayland() {
  GLContextEGL* egl = GLContextEGL::Cast(mGL);
  egl->mEgl->fDestroySurface(mEGLSurface);
  g_clear_pointer(&mEGLWindow, wl_egl_window_destroy);
  g_clear_pointer(&mViewport, wp_viewport_destroy);
  g_clear_pointer(&mWlSurface, wl_surface_destroy);
}

/* static */ RefPtr<SurfacePool> SurfacePool::Create(size_t aPoolSizeLimit) {
  return new SurfacePoolWayland(aPoolSizeLimit);
}

SurfacePoolWayland::SurfacePoolWayland(size_t aPoolSizeLimit)
    : mPoolSizeLimit(aPoolSizeLimit) {}

RefPtr<SurfacePoolHandle> SurfacePoolWayland::GetHandleForGL(GLContext* aGL) {
  return new SurfacePoolHandleWayland(this, aGL);
}

void SurfacePoolWayland::DestroyGLResourcesForContext(GLContext* aGL) {
  // Assume a single shared GL context for now
  MOZ_ASSERT(mInUseEntries.empty());
  mAvailableEntries.Clear();
}

bool SurfacePoolWayland::CanRecycleSurfaceForRequest(
    const SurfacePoolEntry& aEntry, const gfx::IntSize& aSize, GLContext* aGL) {
  return aEntry.mSize == aSize;
}

RefPtr<NativeSurfaceWayland> SurfacePoolWayland::ObtainSurfaceFromPool(
    const gfx::IntSize& aSize, GLContext* aGL) {
  auto iterToRecycle =
      std::find_if(mAvailableEntries.begin(), mAvailableEntries.end(),
                   [&](const SurfacePoolEntry& aEntry) {
                     return CanRecycleSurfaceForRequest(aEntry, aSize, aGL);
                   });
  if (iterToRecycle != mAvailableEntries.end()) {
    RefPtr<NativeSurfaceWayland> surface = iterToRecycle->mNativeSurface;
    mInUseEntries.insert({surface.get(), std::move(*iterToRecycle)});
    mAvailableEntries.RemoveElementAt(iterToRecycle);
    return surface;
  }

  RefPtr<NativeSurfaceWayland> surface = new NativeSurfaceWayland(aSize, aGL);
  mInUseEntries.insert({surface.get(), SurfacePoolEntry{aSize, surface}});

  return surface;
}

void SurfacePoolWayland::ReturnSurfaceToPool(
    const RefPtr<NativeSurfaceWayland>& aSurface) {
  auto inUseEntryIter = mInUseEntries.find(aSurface);
  MOZ_RELEASE_ASSERT(inUseEntryIter != mInUseEntries.end());

  mAvailableEntries.AppendElement(std::move(inUseEntryIter->second));
  mInUseEntries.erase(inUseEntryIter);

  g_clear_pointer(&aSurface->mWlSubsurface, wl_subsurface_destroy);
}

void SurfacePoolWayland::EnforcePoolSizeLimit() {
  // Enforce the pool size limit, removing least-recently-used entries as
  // necessary.
  while (mAvailableEntries.Length() > mPoolSizeLimit) {
    mAvailableEntries.RemoveElementAt(0);
  }
}

SurfacePoolHandleWayland::SurfacePoolHandleWayland(
    RefPtr<SurfacePoolWayland> aPool, GLContext* aGL)
    : mPool(std::move(aPool)), mGL(aGL) {}

void SurfacePoolHandleWayland::OnBeginFrame() {}

void SurfacePoolHandleWayland::OnEndFrame() { mPool->EnforcePoolSizeLimit(); }

RefPtr<NativeSurfaceWayland> SurfacePoolHandleWayland::ObtainSurfaceFromPool(
    const gfx::IntSize& aSize) {
  return mPool->ObtainSurfaceFromPool(aSize, mGL);
}

void SurfacePoolHandleWayland::ReturnSurfaceToPool(
    const RefPtr<NativeSurfaceWayland>& aSurface) {
  mPool->ReturnSurfaceToPool(aSurface);
}

}  // namespace mozilla::layers
