/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nullptr; c-basic-offset: 2
 * -*- This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/SurfacePoolWayland.h"

#include "GLBlitHelper.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"

namespace mozilla::layers {

using gfx::IntRect;
using gl::DepthAndStencilBuffer;
using gl::MozFramebuffer;

#define BACK_BUFFER_NUM 3

static const struct wl_callback_listener sFrameListenerNativeSurfaceWayland = {
    NativeSurfaceWayland::FrameCallbackHandler};

CallbackMultiplexHelper::CallbackMultiplexHelper(CallbackFunc aCallbackFunc,
                                                 void* aCallbackData)
    : mCallbackFunc(aCallbackFunc), mCallbackData(aCallbackData) {}

void CallbackMultiplexHelper::Callback(uint32_t aTime) {
  if (!mActive) {
    return;
  }
  mActive = false;

  // This is likely the first of a batch of frame callbacks being processed and
  // may trigger the setup of a successive one. In order to avoid complexity,
  // defer calling the callback function until we had a chance to process
  // all pending frame callbacks.

  AddRef();
  nsCOMPtr<nsIRunnable> runnable = NewRunnableMethod<uint32_t>(
      "layers::CallbackMultiplexHelper::RunCallback", this,
      &CallbackMultiplexHelper::RunCallback, aTime);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThreadQueue(
      runnable.forget(), EventQueuePriority::Vsync));
}

void CallbackMultiplexHelper::RunCallback(uint32_t aTime) {
  mCallbackFunc(mCallbackData, aTime);
  Release();
}

RefPtr<NativeSurfaceWayland> NativeSurfaceWayland::Create(const IntSize& aSize,
                                                          GLContext* aGL) {
  if (aGL) {
    return new NativeSurfaceWaylandDMABUF(aSize, aGL);
  }

  return new NativeSurfaceWaylandSHM(aSize);
}

NativeSurfaceWayland::NativeSurfaceWayland(const IntSize& aSize)
    : mMutex("NativeSurfaceWayland"), mSize(aSize) {
  RefPtr<nsWaylandDisplay> waylandDisplay = widget::WaylandDisplayGet();
  wl_compositor* compositor = waylandDisplay->GetCompositor();
  mWlSurface = wl_compositor_create_surface(compositor);

  wl_region* region = wl_compositor_create_region(compositor);
  wl_surface_set_input_region(mWlSurface, region);
  wl_region_destroy(region);

  wp_viewporter* viewporter = waylandDisplay->GetViewporter();
  MOZ_ASSERT(viewporter);
  mViewport = wp_viewporter_get_viewport(viewporter, mWlSurface);
}

NativeSurfaceWayland::~NativeSurfaceWayland() {
  MutexAutoLock lock(mMutex);
  g_clear_pointer(&mCallback, wl_callback_destroy);
  g_clear_pointer(&mViewport, wp_viewport_destroy);
  g_clear_pointer(&mWlSubsurface, wl_subsurface_destroy);
  g_clear_pointer(&mWlSurface, wl_surface_destroy);
}

void NativeSurfaceWayland::CreateSubsurface(wl_surface* aParentSurface) {
  MutexAutoLock lock(mMutex);

  if (mWlSubsurface) {
    ClearSubsurface(lock);
  }

  MOZ_ASSERT(aParentSurface);
  wl_subcompositor* subcompositor =
      widget::WaylandDisplayGet()->GetSubcompositor();
  mWlSubsurface = wl_subcompositor_get_subsurface(subcompositor, mWlSurface,
                                                  aParentSurface);
}

void NativeSurfaceWayland::ClearSubsurface() {
  MutexAutoLock lock(mMutex);
  ClearSubsurface(lock);
}

void NativeSurfaceWayland::ClearSubsurface(const MutexAutoLock& aProofOfLock) {
  g_clear_pointer(&mWlSubsurface, wl_subsurface_destroy);
  mPosition = IntPoint(0, 0);
}

void NativeSurfaceWayland::SetBufferTransformFlipped(bool aFlipped) {
  MutexAutoLock lock(mMutex);

  if (aFlipped == mBufferTransformFlipped) {
    return;
  }

  mBufferTransformFlipped = aFlipped;
  if (mBufferTransformFlipped) {
    wl_surface_set_buffer_transform(mWlSurface,
                                    WL_OUTPUT_TRANSFORM_FLIPPED_180);
  } else {
    wl_surface_set_buffer_transform(mWlSurface, WL_OUTPUT_TRANSFORM_NORMAL);
  }
}

void NativeSurfaceWayland::SetPosition(int aX, int aY) {
  MutexAutoLock lock(mMutex);

  if ((aX == mPosition.x && aY == mPosition.y) || !mWlSubsurface) {
    return;
  }

  mPosition.x = aX;
  mPosition.y = aY;
  wl_subsurface_set_position(mWlSubsurface, mPosition.x, mPosition.y);
}

void NativeSurfaceWayland::SetViewportSourceRect(const Rect aSourceRect) {
  MutexAutoLock lock(mMutex);

  if (aSourceRect == mViewportSourceRect) {
    return;
  }

  mViewportSourceRect = aSourceRect;
  wp_viewport_set_source(mViewport, wl_fixed_from_double(mViewportSourceRect.x),
                         wl_fixed_from_double(mViewportSourceRect.y),
                         wl_fixed_from_double(mViewportSourceRect.width),
                         wl_fixed_from_double(mViewportSourceRect.height));
}

void NativeSurfaceWayland::SetViewportDestinationSize(int aWidth, int aHeight) {
  MutexAutoLock lock(mMutex);

  if (aWidth == mViewportDestinationSize.width &&
      aHeight == mViewportDestinationSize.height) {
    return;
  }

  mViewportDestinationSize.width = aWidth;
  mViewportDestinationSize.height = aHeight;
  wp_viewport_set_destination(mViewport, mViewportDestinationSize.width,
                              mViewportDestinationSize.height);
}

void NativeSurfaceWayland::RequestFrameCallback(
    const RefPtr<CallbackMultiplexHelper>& aMultiplexHelper) {
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(aMultiplexHelper->IsActive());

  // Avoid piling up old helpers if this surface does not receive callbacks
  // for a longer time
  mCallbackMultiplexHelpers.RemoveElementsBy(
      [&](const auto& object) { return !object->IsActive(); });

  mCallbackMultiplexHelpers.AppendElement(aMultiplexHelper);
  if (!mCallback) {
    mCallback = wl_surface_frame(mWlSurface);
    wl_callback_add_listener(mCallback, &sFrameListenerNativeSurfaceWayland,
                             this);
    wl_surface_commit(mWlSurface);
  }
}

void NativeSurfaceWayland::FrameCallbackHandler(wl_callback* aCallback,
                                                uint32_t aTime) {
  MutexAutoLock lock(mMutex);

  MOZ_RELEASE_ASSERT(aCallback == mCallback);
  g_clear_pointer(&mCallback, wl_callback_destroy);

  for (const RefPtr<CallbackMultiplexHelper>& callbackMultiplexHelper :
       mCallbackMultiplexHelpers) {
    callbackMultiplexHelper->Callback(aTime);
  }
  mCallbackMultiplexHelpers.Clear();
}

/* static */
void NativeSurfaceWayland::FrameCallbackHandler(void* aData,
                                                wl_callback* aCallback,
                                                uint32_t aTime) {
  auto surface = reinterpret_cast<NativeSurfaceWayland*>(aData);
  surface->FrameCallbackHandler(aCallback, aTime);
}

NativeSurfaceWaylandSHM::NativeSurfaceWaylandSHM(const IntSize& aSize)
    : NativeSurfaceWayland(aSize) {}

RefPtr<DrawTarget> NativeSurfaceWaylandSHM::GetNextDrawTarget() {
  MutexAutoLock lock(mMutex);
  if (!mCurrentBuffer) {
    mCurrentBuffer = ObtainBufferFromPool(lock);
  }
  return mCurrentBuffer->Lock();
}

void NativeSurfaceWaylandSHM::Commit(const IntRegion& aInvalidRegion,
                                     const IntRect& aValidRect) {
  MutexAutoLock lock(mMutex);

  if (aInvalidRegion.IsEmpty()) {
    if (mCurrentBuffer) {
      ReturnBufferToPool(lock, mCurrentBuffer);
      mCurrentBuffer = nullptr;
    }
    wl_surface_commit(mWlSurface);
    return;
  }

  HandlePartialUpdate(lock, aInvalidRegion, aValidRect);

  for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
    IntRect r = iter.Get();
    wl_surface_damage_buffer(mWlSurface, r.x, r.y, r.width, r.height);
  }

  MOZ_ASSERT(mCurrentBuffer);
  mCurrentBuffer->AttachAndCommit(mWlSurface);
  mCurrentBuffer = nullptr;

  EnforcePoolSizeLimit(lock);
}

void NativeSurfaceWaylandSHM::HandlePartialUpdate(
    const MutexAutoLock& aProofOfLock, const IntRegion& aInvalidRegion,
    const IntRect& aValidRect) {
  if (!mPreviousBuffer || mPreviousBuffer == mCurrentBuffer) {
    mPreviousBuffer = mCurrentBuffer;
    return;
  }

  IntRegion copyRegion = IntRegion(aValidRect);
  copyRegion.SubOut(aInvalidRegion);

  if (!copyRegion.IsEmpty()) {
    RefPtr<gfx::DataSourceSurface> dataSourceSurface =
        gfx::CreateDataSourceSurfaceFromData(
            mSize, mPreviousBuffer->GetSurfaceFormat(),
            (const uint8_t*)mPreviousBuffer->GetShmPool()->GetImageData(),
            mSize.width * BytesPerPixel(mPreviousBuffer->GetSurfaceFormat()));
    RefPtr<DrawTarget> dt = mCurrentBuffer->Lock();

    for (auto iter = copyRegion.RectIter(); !iter.Done(); iter.Next()) {
      IntRect r = iter.Get();
      dt->CopySurface(dataSourceSurface, r, IntPoint(r.x, r.y));
    }
  }

  mPreviousBuffer = mCurrentBuffer;
}

RefPtr<WaylandShmBuffer> NativeSurfaceWaylandSHM::ObtainBufferFromPool(
    const MutexAutoLock& aProofOfLock) {
  if (!mAvailableBuffers.IsEmpty()) {
    RefPtr<WaylandShmBuffer> buffer = mAvailableBuffers.PopLastElement();
    mInUseBuffers.AppendElement(buffer);
    return buffer;
  }

  RefPtr<nsWaylandDisplay> waylandDisplay = widget::WaylandDisplayGet();
  RefPtr<WaylandShmBuffer> buffer = WaylandShmBuffer::Create(
      waylandDisplay, LayoutDeviceIntSize::FromUnknownSize(mSize));

  buffer->SetBufferReleaseFunc(
      &NativeSurfaceWaylandSHM::BufferReleaseCallbackHandler);
  buffer->SetBufferReleaseData(this);

  mInUseBuffers.AppendElement(buffer);

  return buffer;
}

void NativeSurfaceWaylandSHM::ReturnBufferToPool(
    const MutexAutoLock& aProofOfLock,
    const RefPtr<WaylandShmBuffer>& aBuffer) {
  for (const RefPtr<WaylandShmBuffer>& buffer : mInUseBuffers) {
    if (buffer == aBuffer) {
      mAvailableBuffers.AppendElement(buffer);
      mInUseBuffers.RemoveElement(buffer);
      return;
    }
  }
  MOZ_RELEASE_ASSERT(false, "Returned buffer not in use");
}

void NativeSurfaceWaylandSHM::EnforcePoolSizeLimit(
    const MutexAutoLock& aProofOfLock) {
  // Enforce the pool size limit, removing least-recently-used entries as
  // necessary.
  while (mAvailableBuffers.Length() > BACK_BUFFER_NUM) {
    mAvailableBuffers.RemoveElementAt(0);
  }

  NS_WARNING_ASSERTION(mInUseBuffers.Length() < 10, "We are leaking buffers");
}

void NativeSurfaceWaylandSHM::BufferReleaseCallbackHandler(wl_buffer* aBuffer) {
  MutexAutoLock lock(mMutex);
  for (const RefPtr<WaylandShmBuffer>& buffer : mInUseBuffers) {
    if (buffer->GetWlBuffer() == aBuffer) {
      ReturnBufferToPool(lock, buffer);
      break;
    }
  }
}

/* static */
void NativeSurfaceWaylandSHM::BufferReleaseCallbackHandler(void* aData,
                                                           wl_buffer* aBuffer) {
  auto surface = reinterpret_cast<NativeSurfaceWaylandSHM*>(aData);
  surface->BufferReleaseCallbackHandler(aBuffer);
}

/* static */
RefPtr<WaylandDMABUFBuffer> WaylandDMABUFBuffer::Create(
    const LayoutDeviceIntSize& aSize, GLContext* aGL) {
  RefPtr<WaylandDMABUFBuffer> buffer = new WaylandDMABUFBuffer(aSize);

  const auto flags =
      static_cast<DMABufSurfaceFlags>(DMABUF_TEXTURE | DMABUF_ALPHA);
  buffer->mDMABufSurface =
      DMABufSurfaceRGBA::CreateDMABufSurface(aSize.width, aSize.height, flags);
  if (!buffer->mDMABufSurface || !buffer->mDMABufSurface->CreateTexture(aGL)) {
    return nullptr;
  }

  if (!buffer->mDMABufSurface->CreateWlBuffer()) {
    return nullptr;
  }

  return buffer;
}

WaylandDMABUFBuffer::WaylandDMABUFBuffer(const LayoutDeviceIntSize& aSize)
    : mSize(aSize) {}

NativeSurfaceWaylandDMABUF::NativeSurfaceWaylandDMABUF(const IntSize& aSize,
                                                       GLContext* aGL)
    : NativeSurfaceWayland(aSize), mGL(aGL) {}

Maybe<GLuint> NativeSurfaceWaylandDMABUF::GetNextFramebuffer() {
  MutexAutoLock lock(mMutex);

  if (!mCurrentBuffer) {
    mCurrentBuffer = ObtainBufferFromPool(lock);
  }

  return Some(mCurrentBuffer->GetFramebuffer()->mFB);
}

void NativeSurfaceWaylandDMABUF::Commit(const IntRegion& aInvalidRegion,
                                        const IntRect& aValidRect) {
  MutexAutoLock lock(mMutex);

  if (aInvalidRegion.IsEmpty()) {
    if (mCurrentBuffer) {
      ReturnBufferToPool(lock, mCurrentBuffer);
      mCurrentBuffer = nullptr;
    }
    wl_surface_commit(mWlSurface);
    return;
  }

  HandlePartialUpdate(lock, aInvalidRegion, aValidRect);

  // We rely on implicit synchronization in the system compositor to make sure
  // all GL operation have been finished befor presenting a new frame.
  mGL->fFlush();

  for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
    IntRect r = iter.Get();
    wl_surface_damage_buffer(mWlSurface, r.x, r.y, r.width, r.height);
  }

  MOZ_ASSERT(mCurrentBuffer);
  wl_surface_attach(mWlSurface, mCurrentBuffer->GetWlBuffer(), 0, 0);
  wl_surface_commit(mWlSurface);
  mCurrentBuffer = nullptr;

  EnforcePoolSizeLimit(lock);
}

void NativeSurfaceWaylandDMABUF::HandlePartialUpdate(
    const MutexAutoLock& aProofOfLock, const IntRegion& aInvalidRegion,
    const IntRect& aValidRect) {
  if (!mPreviousBuffer || mPreviousBuffer == mCurrentBuffer) {
    mPreviousBuffer = mCurrentBuffer;
    return;
  }

  IntRegion copyRegion = IntRegion(aValidRect);
  copyRegion.SubOut(aInvalidRegion);

  if (!copyRegion.IsEmpty()) {
    mGL->MakeCurrent();
    for (auto iter = copyRegion.RectIter(); !iter.Done(); iter.Next()) {
      gfx::IntRect r = iter.Get();
      mGL->BlitHelper()->BlitFramebufferToFramebuffer(
          mPreviousBuffer->GetFramebuffer()->mFB,
          mCurrentBuffer->GetFramebuffer()->mFB, r, r, LOCAL_GL_NEAREST);
    }
  }

  mPreviousBuffer = mCurrentBuffer;
}

void NativeSurfaceWaylandDMABUF::DestroyGLResources() {
  mInUseBuffers.Clear();
  mAvailableBuffers.Clear();
  mDepthBuffers.Clear();
  mCurrentBuffer = nullptr;
  mPreviousBuffer = nullptr;
  mGL = nullptr;
}

static const struct wl_buffer_listener
    sBufferListenerNativeSurfaceWaylandDMABUF = {
        NativeSurfaceWaylandDMABUF::BufferReleaseCallbackHandler};

RefPtr<WaylandDMABUFBuffer> NativeSurfaceWaylandDMABUF::ObtainBufferFromPool(
    const MutexAutoLock& aProofOfLock) {
  if (!mAvailableBuffers.IsEmpty()) {
    RefPtr<WaylandDMABUFBuffer> buffer = mAvailableBuffers.PopLastElement();
    mInUseBuffers.AppendElement(buffer);
    return buffer;
  }

  RefPtr<WaylandDMABUFBuffer> buffer = WaylandDMABUFBuffer::Create(
      LayoutDeviceIntSize::FromUnknownSize(mSize), mGL);

  const auto tex = buffer->GetDMABufSurface()->GetTexture();
  UniquePtr<MozFramebuffer> framebuffer =
      CreateFramebufferForTexture(aProofOfLock, mGL, mSize, tex);
  buffer->SetFramebuffer(std::move(framebuffer));

  wl_buffer_add_listener(buffer->GetWlBuffer(),
                         &sBufferListenerNativeSurfaceWaylandDMABUF, this);

  mInUseBuffers.AppendElement(buffer);

  return buffer;
}

void NativeSurfaceWaylandDMABUF::ReturnBufferToPool(
    const MutexAutoLock& aProofOfLock,
    const RefPtr<WaylandDMABUFBuffer>& aBuffer) {
  for (const RefPtr<WaylandDMABUFBuffer>& buffer : mInUseBuffers) {
    if (buffer == aBuffer) {
      mAvailableBuffers.AppendElement(buffer);
      mInUseBuffers.RemoveElement(buffer);
      return;
    }
  }
  MOZ_RELEASE_ASSERT(false, "Returned buffer not in use");
}

void NativeSurfaceWaylandDMABUF::EnforcePoolSizeLimit(
    const MutexAutoLock& aProofOfLock) {
  // Enforce the pool size limit, removing least-recently-used entries as
  // necessary.
  while (mAvailableBuffers.Length() > BACK_BUFFER_NUM) {
    mAvailableBuffers.RemoveElementAt(0);
  }

  NS_WARNING_ASSERTION(mInUseBuffers.Length() < 10, "We are leaking buffers");
}

RefPtr<DepthAndStencilBuffer>
NativeSurfaceWaylandDMABUF::GetDepthBufferForSharing(
    const MutexAutoLock& aProofOfLock, GLContext* aGL, const IntSize& aSize) {
  // Clean out entries for which the weak pointer has become null.
  mDepthBuffers.RemoveElementsBy(
      [&](const DepthBufferEntry& entry) { return !entry.mBuffer; });

  for (const auto& entry : mDepthBuffers) {
    if (entry.mGL == aGL && entry.mSize == aSize) {
      return entry.mBuffer.get();
    }
  }
  return nullptr;
}

UniquePtr<MozFramebuffer>
NativeSurfaceWaylandDMABUF::CreateFramebufferForTexture(
    const MutexAutoLock& aProofOfLock, GLContext* aGL, const IntSize& aSize,
    GLuint aTexture) {
  // Try to find an existing depth buffer of aSize in aGL and create a
  // framebuffer that shares it.
  if (auto buffer = GetDepthBufferForSharing(aProofOfLock, aGL, aSize)) {
    return MozFramebuffer::CreateForBackingWithSharedDepthAndStencil(
        aSize, 0, LOCAL_GL_TEXTURE_2D, aTexture, buffer);
  }

  UniquePtr<MozFramebuffer> fb = MozFramebuffer::CreateForBacking(
      aGL, aSize, 0, true, LOCAL_GL_TEXTURE_2D, aTexture);
  if (fb) {
    mDepthBuffers.AppendElement(
        DepthBufferEntry{aGL, aSize, fb->GetDepthAndStencilBuffer().get()});
  }

  return fb;
}

void NativeSurfaceWaylandDMABUF::BufferReleaseCallbackHandler(
    wl_buffer* aBuffer) {
  MutexAutoLock lock(mMutex);
  for (const RefPtr<WaylandDMABUFBuffer>& buffer : mInUseBuffers) {
    if (buffer->GetWlBuffer() == aBuffer) {
      ReturnBufferToPool(lock, buffer);
      break;
    }
  }
}

/* static */
void NativeSurfaceWaylandDMABUF::BufferReleaseCallbackHandler(
    void* aData, wl_buffer* aBuffer) {
  auto surface = reinterpret_cast<NativeSurfaceWaylandDMABUF*>(aData);
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
      [aGL](const auto& entry) { return entry.mGL == aGL; });

  // std::erase_if
  for (auto entry = mInUseEntries.begin(), last = mInUseEntries.end();
       entry != last;) {
    if (entry->second.mGL == aGL) {
      entry->second.mNativeSurface->DestroyGLResources();
      entry = mInUseEntries.erase(entry);
    } else {
      ++entry;
    }
  }
}

bool SurfacePoolWayland::CanRecycleSurfaceForRequest(
    const SurfacePoolEntry& aEntry, const IntSize& aSize, GLContext* aGL) {
  if (aEntry.mSize != aSize) {
    return false;
  }
  if (aEntry.mGL != aGL) {
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
        {surface.get(), SurfacePoolEntry{aSize, surface, aGL}});
  }

  return surface;
}

void SurfacePoolWayland::ReturnSurfaceToPool(
    const RefPtr<NativeSurfaceWayland>& aSurface) {
  auto inUseEntryIter = mInUseEntries.find(aSurface);
  if (inUseEntryIter != mInUseEntries.end()) {
    mAvailableEntries.AppendElement(std::move(inUseEntryIter->second));
    mInUseEntries.erase(inUseEntryIter);
  }

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
