/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nullptr; c-basic-offset: 2
 * -*- This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/SurfacePoolWayland.h"

#include "GLContextEGL.h"

namespace mozilla::layers {

using gfx::IntRect;
using gl::GLContextEGL;

#define BACK_BUFFER_NUM 3

RefPtr<NativeSurfaceWayland> NativeSurfaceWayland::Create(const IntSize& aSize,
                                                          GLContext* aGL) {
  if (aGL) {
    RefPtr<NativeSurfaceWaylandEGL> surfaceEGL =
        new NativeSurfaceWaylandEGL(widget::WaylandDisplayGet(), aGL);

    surfaceEGL->mEGLWindow =
        wl_egl_window_create(surfaceEGL->mWlSurface, aSize.width, aSize.height);
    if (!surfaceEGL->mEGLWindow) {
      return nullptr;
    }

    GLContextEGL* egl = GLContextEGL::Cast(aGL);
    surfaceEGL->mEGLSurface = egl->mEgl->fCreateWindowSurface(
        egl->mConfig, surfaceEGL->mEGLWindow, nullptr);
    if (surfaceEGL->mEGLSurface == EGL_NO_SURFACE) {
      return nullptr;
    }

    return surfaceEGL;
  }

  return new NativeSurfaceWaylandSHM(widget::WaylandDisplayGet(), aSize);
}

NativeSurfaceWayland::NativeSurfaceWayland(
    const RefPtr<nsWaylandDisplay>& aWaylandDisplay)
    : mWaylandDisplay(aWaylandDisplay) {
  wl_compositor* compositor = mWaylandDisplay->GetCompositor();
  mWlSurface = wl_compositor_create_surface(compositor);

  wl_region* region = wl_compositor_create_region(compositor);
  wl_surface_set_input_region(mWlSurface, region);
  wl_region_destroy(region);

  wp_viewporter* viewporter = mWaylandDisplay->GetViewporter();
  MOZ_ASSERT(viewporter);
  mViewport = wp_viewporter_get_viewport(viewporter, mWlSurface);
}

NativeSurfaceWayland::~NativeSurfaceWayland() {
  g_clear_pointer(&mViewport, wp_viewport_destroy);
  g_clear_pointer(&mWlSubsurface, wl_subsurface_destroy);
  g_clear_pointer(&mWlSurface, wl_surface_destroy);
}

NativeSurfaceWaylandEGL::NativeSurfaceWaylandEGL(
    const RefPtr<nsWaylandDisplay>& aWaylandDisplay, GLContext* aGL)
    : NativeSurfaceWayland(aWaylandDisplay),
      mGL(aGL),
      mEGLWindow(nullptr),
      mEGLSurface(nullptr) {
  wl_surface_set_buffer_transform(mWlSurface, WL_OUTPUT_TRANSFORM_FLIPPED_180);
}

NativeSurfaceWaylandEGL::~NativeSurfaceWaylandEGL() { DestroyGLResources(); }

Maybe<GLuint> NativeSurfaceWaylandEGL::GetAsFramebuffer() {
  GLContextEGL* gl = GLContextEGL::Cast(mGL);

  gl->SetEGLSurfaceOverride(mEGLSurface);
  gl->MakeCurrent();

  return Some(0);
}

void NativeSurfaceWaylandEGL::Commit(const IntRegion& aInvalidRegion) {
  if (aInvalidRegion.IsEmpty()) {
    wl_surface_commit(mWlSurface);
    return;
  }

  GLContextEGL* gl = GLContextEGL::Cast(mGL);
  auto egl = gl->mEgl;

  gl->SetEGLSurfaceOverride(mEGLSurface);
  gl->MakeCurrent();

  gl->mEgl->fSwapInterval(0);
  gl->mEgl->fSwapBuffers(mEGLSurface);

  gl->SetEGLSurfaceOverride(nullptr);
  gl->MakeCurrent();
}

void NativeSurfaceWaylandEGL::NotifySurfaceReady() {
  GLContextEGL* gl = GLContextEGL::Cast(mGL);
  gl->SetEGLSurfaceOverride(nullptr);
  gl->MakeCurrent();
}

void NativeSurfaceWaylandEGL::DestroyGLResources() {
  if (mEGLSurface != EGL_NO_SURFACE) {
    GLContextEGL* egl = GLContextEGL::Cast(mGL);
    egl->mEgl->fDestroySurface(mEGLSurface);
    mEGLSurface = EGL_NO_SURFACE;
    g_clear_pointer(&mEGLWindow, wl_egl_window_destroy);
  }
}

NativeSurfaceWaylandSHM::NativeSurfaceWaylandSHM(
    const RefPtr<nsWaylandDisplay>& aWaylandDisplay, const IntSize& aSize)
    : NativeSurfaceWayland(aWaylandDisplay), mSize(aSize) {}

RefPtr<DrawTarget> NativeSurfaceWaylandSHM::GetAsDrawTarget() {
  if (!mCurrentBuffer) {
    mCurrentBuffer = ObtainBufferFromPool();
  }
  return mCurrentBuffer->Lock();
}

void NativeSurfaceWaylandSHM::Commit(const IntRegion& aInvalidRegion) {
  if (aInvalidRegion.IsEmpty()) {
    if (mCurrentBuffer) {
      ReturnBufferToPool(mCurrentBuffer);
      mCurrentBuffer = nullptr;
    }
    wl_surface_commit(mWlSurface);
    return;
  }

  for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
    IntRect r = iter.Get();
    wl_surface_damage_buffer(mWlSurface, r.x, r.y, r.width, r.height);
  }

  MOZ_ASSERT(mCurrentBuffer);
  mCurrentBuffer->AttachAndCommit(mWlSurface);
  mCurrentBuffer = nullptr;

  EnforcePoolSizeLimit();
}

RefPtr<WaylandShmBuffer> NativeSurfaceWaylandSHM::ObtainBufferFromPool() {
  if (!mAvailableBuffers.IsEmpty()) {
    RefPtr<WaylandShmBuffer> buffer = mAvailableBuffers.PopLastElement();
    mInUseBuffers.AppendElement(buffer);
    return buffer;
  }

  RefPtr<WaylandShmBuffer> buffer = WaylandShmBuffer::Create(
      mWaylandDisplay, LayoutDeviceIntSize::FromUnknownSize(mSize));
  mInUseBuffers.AppendElement(buffer);

  buffer->SetBufferReleaseFunc(
      &NativeSurfaceWaylandSHM::BufferReleaseCallbackHandler);
  buffer->SetBufferReleaseData(this);

  return buffer;
}

void NativeSurfaceWaylandSHM::ReturnBufferToPool(
    const RefPtr<WaylandShmBuffer>& aBuffer) {
  for (const RefPtr<WaylandShmBuffer>& buffer : mInUseBuffers) {
    if (buffer == aBuffer) {
      if (buffer->IsMatchingSize(LayoutDeviceIntSize::FromUnknownSize(mSize))) {
        mAvailableBuffers.AppendElement(buffer);
      }
      mInUseBuffers.RemoveElement(buffer);
      return;
    }
  }
  MOZ_RELEASE_ASSERT(false, "Returned buffer not in use");
}

void NativeSurfaceWaylandSHM::EnforcePoolSizeLimit() {
  // Enforce the pool size limit, removing least-recently-used entries as
  // necessary.
  while (mAvailableBuffers.Length() > BACK_BUFFER_NUM) {
    mAvailableBuffers.RemoveElementAt(0);
  }

  NS_WARNING_ASSERTION(mInUseBuffers.Length() < 10, "We are leaking buffers");
}

void NativeSurfaceWaylandSHM::BufferReleaseCallbackHandler(wl_buffer* aBuffer) {
  for (const RefPtr<WaylandShmBuffer>& buffer : mInUseBuffers) {
    if (buffer->GetWlBuffer() == aBuffer) {
      ReturnBufferToPool(buffer);
      break;
    }
  }
}

void NativeSurfaceWaylandSHM::BufferReleaseCallbackHandler(void* aData,
                                                           wl_buffer* aBuffer) {
  auto* surface = reinterpret_cast<NativeSurfaceWaylandSHM*>(aData);
  surface->BufferReleaseCallbackHandler(aBuffer);
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
  mAvailableEntries.RemoveElementsBy(
      [aGL](const auto& entry) { return entry.mGLContext == aGL; });

  for (auto& entry : mInUseEntries) {
    if (entry.second.mGLContext == aGL) {
      entry.second.mNativeSurface->DestroyGLResources();
      entry.second.mRecycle = false;
    }
  }
}

bool SurfacePoolWayland::CanRecycleSurfaceForRequest(
    const SurfacePoolEntry& aEntry, const IntSize& aSize, GLContext* aGL) {
  if (aEntry.mSize != aSize) {
    return false;
  }
  if (aEntry.mGLContext != aGL) {
    return false;
  }
  return true;
}

RefPtr<NativeSurfaceWayland> SurfacePoolWayland::ObtainSurfaceFromPool(
    const IntSize& aSize, GLContext* aGL) {
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

  RefPtr<NativeSurfaceWayland> surface =
      NativeSurfaceWayland::Create(aSize, aGL);
  if (surface) {
    mInUseEntries.insert(
        {surface.get(), SurfacePoolEntry{aSize, surface, aGL, true}});
  }

  return surface;
}

void SurfacePoolWayland::ReturnSurfaceToPool(
    const RefPtr<NativeSurfaceWayland>& aSurface) {
  auto inUseEntryIter = mInUseEntries.find(aSurface);
  MOZ_RELEASE_ASSERT(inUseEntryIter != mInUseEntries.end());

  if (inUseEntryIter->second.mRecycle) {
    mAvailableEntries.AppendElement(std::move(inUseEntryIter->second));
  }
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
    const IntSize& aSize) {
  return mPool->ObtainSurfaceFromPool(aSize, mGL);
}

void SurfacePoolHandleWayland::ReturnSurfaceToPool(
    const RefPtr<NativeSurfaceWayland>& aSurface) {
  mPool->ReturnSurfaceToPool(aSurface);
}

}  // namespace mozilla::layers
