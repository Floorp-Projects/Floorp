/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_SurfacePool_h
#define mozilla_layers_SurfacePool_h

#include "mozilla/Maybe.h"
#include "mozilla/ThreadSafeWeakPtr.h"

#include "GLTypes.h"
#include "nsISupportsImpl.h"
#include "nsRegion.h"

namespace mozilla {

namespace gl {
class GLContext;
}  // namespace gl

namespace layers {

class SurfacePoolHandle;

// A pool of surfaces for NativeLayers. Manages GL resources. Since GLContexts
// are bound to their creator thread, a pool should not be shared across
// threads. Call Create() to create an instance. Call GetHandleForGL() to obtain
// a handle that can be passed to NativeLayerRoot::CreateLayer.
class SurfacePool {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SurfacePool);

#if defined(XP_MACOSX) || defined(MOZ_WAYLAND)
  static RefPtr<SurfacePool> Create(size_t aPoolSizeLimit);
#endif

  // aGL can be nullptr.
  virtual RefPtr<SurfacePoolHandle> GetHandleForGL(gl::GLContext* aGL) = 0;
  virtual void DestroyGLResourcesForContext(gl::GLContext* aGL) = 0;

 protected:
  virtual ~SurfacePool() = default;
};

class SurfacePoolHandleCA;
class SurfacePoolHandleWayland;

// A handle to the process-wide surface pool. Users should create one handle per
// OS window, and call OnBeginFrame() and OnEndFrame() on the handle at
// appropriate times. OnBeginFrame() and OnEndFrame() should be called on the
// thread that the surface pool was created on.
// These handles are stored on NativeLayers that are created with them and keep
// the SurfacePool alive.
class SurfacePoolHandle {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SurfacePoolHandle);
  virtual SurfacePoolHandleCA* AsSurfacePoolHandleCA() { return nullptr; }
  virtual SurfacePoolHandleWayland* AsSurfacePoolHandleWayland() {
    return nullptr;
  }

  virtual RefPtr<SurfacePool> Pool() = 0;

  // Should be called every frame, in order to do rate-limited cleanup tasks.
  virtual void OnBeginFrame() = 0;
  virtual void OnEndFrame() = 0;

 protected:
  virtual ~SurfacePoolHandle() = default;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_SurfacePool_h
