/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/NativeLayerWayland.h"

#include <dlfcn.h>
#include <utility>
#include <algorithm>

#include "gfxUtils.h"
#include "GLContextProvider.h"
#include "GLBlitHelper.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/SurfacePoolWayland.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla::layers {

using gfx::BackendType;
using gfx::DrawTarget;
using gfx::IntPoint;
using gfx::IntRect;
using gfx::IntRegion;
using gfx::IntSize;
using gfx::Matrix4x4;
using gfx::Point;
using gfx::Rect;
using gfx::SamplingFilter;
using gfx::Size;

static const struct wl_callback_listener sFrameListenerNativeLayerWayland = {
    NativeLayerWayland::FrameCallbackHandler};

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

/* static */
already_AddRefed<NativeLayerRootWayland>
NativeLayerRootWayland::CreateForMozContainer(MozContainer* aContainer) {
  RefPtr<NativeLayerRootWayland> layerRoot =
      new NativeLayerRootWayland(aContainer);
  return layerRoot.forget();
}

NativeLayerRootWayland::NativeLayerRootWayland(MozContainer* aContainer)
    : mMutex("NativeLayerRootWayland"), mContainer(aContainer) {
  g_object_ref(mContainer);
}

NativeLayerRootWayland::~NativeLayerRootWayland() {
  g_object_unref(mContainer);
}

already_AddRefed<NativeLayer> NativeLayerRootWayland::CreateLayer(
    const IntSize& aSize, bool aIsOpaque,
    SurfacePoolHandle* aSurfacePoolHandle) {
  RefPtr<NativeLayer> layer = new NativeLayerWayland(
      aSize, aIsOpaque, aSurfacePoolHandle->AsSurfacePoolHandleWayland());
  return layer.forget();
}

already_AddRefed<NativeLayer>
NativeLayerRootWayland::CreateLayerForExternalTexture(bool aIsOpaque) {
  RefPtr<NativeLayer> layer = new NativeLayerWayland(aIsOpaque);
  return layer.forget();
}

void NativeLayerRootWayland::AppendLayer(NativeLayer* aLayer) {
  MOZ_RELEASE_ASSERT(false);
  MutexAutoLock lock(mMutex);

  RefPtr<NativeLayerWayland> layerWayland = aLayer->AsNativeLayerWayland();
  MOZ_RELEASE_ASSERT(layerWayland);

  mSublayers.AppendElement(layerWayland);
}

void NativeLayerRootWayland::RemoveLayer(NativeLayer* aLayer) {
  MOZ_RELEASE_ASSERT(false);
  MutexAutoLock lock(mMutex);

  RefPtr<NativeLayerWayland> layerWayland = aLayer->AsNativeLayerWayland();
  MOZ_RELEASE_ASSERT(layerWayland);

  mSublayers.RemoveElement(layerWayland);
}

void NativeLayerRootWayland::SetLayers(
    const nsTArray<RefPtr<NativeLayer>>& aLayers) {
  MutexAutoLock lock(mMutex);

  nsTArray<RefPtr<NativeLayerWayland>> newSublayers(aLayers.Length());
  for (const RefPtr<NativeLayer>& sublayer : aLayers) {
    RefPtr<NativeLayerWayland> layer = sublayer->AsNativeLayerWayland();
    newSublayers.AppendElement(layer);
  }

  if (newSublayers != mSublayers) {
    for (const RefPtr<NativeLayerWayland>& layer : mSublayers) {
      if (!newSublayers.Contains(layer)) {
        mOldSublayers.AppendElement(layer);
      }
    }
    mSublayers = std::move(newSublayers);
    mNewLayers = true;
  }
}

bool NativeLayerRootWayland::CommitToScreen() {
  MutexAutoLock lock(mMutex);
  return CommitToScreen(lock);
}

bool NativeLayerRootWayland::CommitToScreen(const MutexAutoLock& aProofOfLock) {
  mFrameInProcess = false;

  wl_surface* containerSurface = moz_container_wayland_surface_lock(mContainer);
  if (!containerSurface) {
    if (!mCallbackRequested) {
      RefPtr<NativeLayerRootWayland> self(this);
      moz_container_wayland_add_initial_draw_callback(
          mContainer, [self]() -> void {
            MutexAutoLock lock(self->mMutex);
            if (!self->mFrameInProcess) {
              self->CommitToScreen(lock);
            }
            self->mCallbackRequested = false;
          });
      mCallbackRequested = true;
    }
    return true;
  }

  wl_surface* previousSurface = nullptr;
  for (RefPtr<NativeLayerWayland>& layer : mSublayers) {
    layer->EnsureParentSurface(containerSurface);

    if (mNewLayers) {
      wl_subsurface_place_above(layer->mWlSubsurface, previousSurface
                                                          ? previousSurface
                                                          : containerSurface);
      previousSurface = layer->mWlSurface;
    }

    MOZ_RELEASE_ASSERT(layer->mTransform.Is2D());
    auto transform2D = layer->mTransform.As2D();

    Rect surfaceRectClipped =
        Rect(0, 0, (float)layer->mSize.width, (float)layer->mSize.height);
    surfaceRectClipped =
        surfaceRectClipped.Intersect(Rect(layer->mDisplayRect));

    transform2D.PostTranslate((float)layer->mPosition.x,
                              (float)layer->mPosition.y);
    surfaceRectClipped = transform2D.TransformBounds(surfaceRectClipped);

    if (layer->mClipRect) {
      surfaceRectClipped =
          surfaceRectClipped.Intersect(Rect(layer->mClipRect.value()));
    }

    if (roundf(surfaceRectClipped.width) > 0 &&
        roundf(surfaceRectClipped.height) > 0) {
      layer->SetBufferTransformFlipped(transform2D.HasNegativeScaling());

      double bufferScale = moz_container_wayland_get_scale(mContainer);
      layer->SetSubsurfacePosition(floor(surfaceRectClipped.x / bufferScale),
                                   floor(surfaceRectClipped.y / bufferScale));
      layer->SetViewportDestinationSize(
          ceil(surfaceRectClipped.width / bufferScale),
          ceil(surfaceRectClipped.height / bufferScale));

      auto transform2DInversed = transform2D.Inverse();
      Rect bufferClip = transform2DInversed.TransformBounds(surfaceRectClipped);
      layer->SetViewportSourceRect(bufferClip);

      layer->Commit();
    } else {
      layer->Unmap();
    }
  }

  if (mNewLayers) {
    for (RefPtr<NativeLayerWayland>& layer : mOldSublayers) {
      layer->Unmap();
    }
    mOldSublayers.Clear();

    nsCOMPtr<nsIRunnable> updateLayersRunnable = NewRunnableMethod<>(
        "layers::NativeLayerRootWayland::UpdateLayersOnMainThread", this,
        &NativeLayerRootWayland::UpdateLayersOnMainThread);
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThreadQueue(
        updateLayersRunnable.forget(), EventQueuePriority::Normal));
    mNewLayers = false;
  }

  if (containerSurface != mWlSurface) {
    if (!mShmBuffer) {
      mShmBuffer = widget::WaylandBufferSHM::Create(LayoutDeviceIntSize(1, 1));
      mShmBuffer->Clear();
    }
    mShmBuffer->AttachAndCommit(containerSurface);
    mWlSurface = containerSurface;
  } else {
    wl_surface_commit(containerSurface);
  }

  moz_container_wayland_surface_unlock(mContainer, &containerSurface);
  wl_display_flush(widget::WaylandDisplayGet()->GetDisplay());
  return true;
}

void NativeLayerRootWayland::RequestFrameCallback(CallbackFunc aCallbackFunc,
                                                  void* aCallbackData) {
  MutexAutoLock lock(mMutex);

  mCallbackMultiplexHelper =
      new CallbackMultiplexHelper(aCallbackFunc, aCallbackData);

  for (const RefPtr<NativeLayerWayland>& layer : mSublayersOnMainThread) {
    layer->RequestFrameCallback(mCallbackMultiplexHelper);
  }

  wl_surface* wlSurface = moz_container_wayland_surface_lock(mContainer);
  if (wlSurface) {
    wl_surface_commit(wlSurface);
    wl_display_flush(widget::WaylandDisplayGet()->GetDisplay());
    moz_container_wayland_surface_unlock(mContainer, &wlSurface);
  }
}

static void sAfterFrameClockAfterPaint(
    GdkFrameClock* aClock, NativeLayerRootWayland* aNativeLayerRoot) {
  aNativeLayerRoot->AfterFrameClockAfterPaint();
}

void NativeLayerRootWayland::AfterFrameClockAfterPaint() {
  MutexAutoLock lock(mMutex);
  wl_surface* containerSurface = moz_container_wayland_surface_lock(mContainer);

  for (const RefPtr<NativeLayerWayland>& layer : mSublayersOnMainThread) {
    wl_surface_commit(layer->mWlSurface);
  }
  if (containerSurface) {
    wl_surface_commit(containerSurface);
    moz_container_wayland_surface_unlock(mContainer, &containerSurface);
  }
}

void NativeLayerRootWayland::UpdateLayersOnMainThread() {
  AssertIsOnMainThread();
  MutexAutoLock lock(mMutex);

  static auto sGdkWaylandWindowAddCallbackSurface =
      (void (*)(GdkWindow*, struct wl_surface*))dlsym(
          RTLD_DEFAULT, "gdk_wayland_window_add_frame_callback_surface");
  static auto sGdkWaylandWindowRemoveCallbackSurface =
      (void (*)(GdkWindow*, struct wl_surface*))dlsym(
          RTLD_DEFAULT, "gdk_wayland_window_remove_frame_callback_surface");

  wl_surface* containerSurface = moz_container_wayland_surface_lock(mContainer);
  GdkWindow* gdkWindow = gtk_widget_get_window(GTK_WIDGET(mContainer));

  mSublayersOnMainThread.RemoveElementsBy([&](const auto& layer) {
    if (!mSublayers.Contains(layer)) {
      if (layer->IsOpaque() &&
          StaticPrefs::widget_wayland_opaque_region_enabled_AtStartup() &&
          sGdkWaylandWindowAddCallbackSurface && gdkWindow) {
        sGdkWaylandWindowRemoveCallbackSurface(gdkWindow, layer->mWlSurface);

        wl_compositor* compositor =
            widget::WaylandDisplayGet()->GetCompositor();
        wl_region* region = wl_compositor_create_region(compositor);
        wl_surface_set_opaque_region(layer->mWlSurface, region);
        wl_region_destroy(region);
        wl_surface_commit(layer->mWlSurface);
      }
      return true;
    }
    return false;
  });

  for (const RefPtr<NativeLayerWayland>& layer : mSublayers) {
    if (!mSublayersOnMainThread.Contains(layer)) {
      if (layer->IsOpaque() &&
          StaticPrefs::widget_wayland_opaque_region_enabled_AtStartup() &&
          sGdkWaylandWindowRemoveCallbackSurface && gdkWindow) {
        sGdkWaylandWindowAddCallbackSurface(gdkWindow, layer->mWlSurface);

        wl_compositor* compositor =
            widget::WaylandDisplayGet()->GetCompositor();
        wl_region* region = wl_compositor_create_region(compositor);
        wl_region_add(region, 0, 0, INT32_MAX, INT32_MAX);
        wl_surface_set_opaque_region(layer->mWlSurface, region);
        wl_region_destroy(region);
        wl_surface_commit(layer->mWlSurface);
      }
      if (mCallbackMultiplexHelper && mCallbackMultiplexHelper->IsActive()) {
        layer->RequestFrameCallback(mCallbackMultiplexHelper);
      }
      mSublayersOnMainThread.AppendElement(layer);
    }
  }

  if (containerSurface) {
    wl_surface_commit(containerSurface);
    moz_container_wayland_surface_unlock(mContainer, &containerSurface);
  }

  if (!mGdkAfterPaintId && gdkWindow) {
    GdkFrameClock* frame_clock = gdk_window_get_frame_clock(gdkWindow);
    mGdkAfterPaintId =
        g_signal_connect_after(frame_clock, "after-paint",
                               G_CALLBACK(sAfterFrameClockAfterPaint), this);
  }
}

void NativeLayerRootWayland::PauseCompositor() {
  MutexAutoLock lock(mMutex);

  for (RefPtr<NativeLayerWayland>& layer : mSublayers) {
    layer->EnsureParentSurface(nullptr);
  }
}

NativeLayerWayland::NativeLayerWayland(
    const IntSize& aSize, bool aIsOpaque,
    SurfacePoolHandleWayland* aSurfacePoolHandle)
    : mMutex("NativeLayerWayland"),
      mSurfacePoolHandle(aSurfacePoolHandle),
      mSize(aSize),
      mIsOpaque(aIsOpaque) {
  MOZ_RELEASE_ASSERT(mSurfacePoolHandle,
                     "Need a non-null surface pool handle.");

  RefPtr<widget::nsWaylandDisplay> waylandDisplay = widget::WaylandDisplayGet();
  wl_compositor* compositor = waylandDisplay->GetCompositor();
  mWlSurface = wl_compositor_create_surface(compositor);

  wl_region* region = wl_compositor_create_region(compositor);
  wl_surface_set_input_region(mWlSurface, region);
  wl_region_destroy(region);

  wp_viewporter* viewporter = waylandDisplay->GetViewporter();
  mViewport = wp_viewporter_get_viewport(viewporter, mWlSurface);
}

NativeLayerWayland::NativeLayerWayland(bool aIsOpaque)
    : mMutex("NativeLayerWayland"),
      mSurfacePoolHandle(nullptr),
      mIsOpaque(aIsOpaque) {
  MOZ_RELEASE_ASSERT(false);  // external image
}

NativeLayerWayland::~NativeLayerWayland() {
  MutexAutoLock lock(mMutex);

  if (mInProgressBuffer) {
    mSurfacePoolHandle->ReturnBufferToPool(mInProgressBuffer);
    mInProgressBuffer = nullptr;
  }
  if (mFrontBuffer) {
    mSurfacePoolHandle->ReturnBufferToPool(mFrontBuffer);
    mFrontBuffer = nullptr;
  }
  g_clear_pointer(&mCallback, wl_callback_destroy);
  g_clear_pointer(&mViewport, wp_viewport_destroy);
  g_clear_pointer(&mWlSubsurface, wl_subsurface_destroy);
  g_clear_pointer(&mWlSurface, wl_surface_destroy);
}

void NativeLayerWayland::AttachExternalImage(
    wr::RenderTextureHost* aExternalImage) {
  MOZ_RELEASE_ASSERT(false);
}

void NativeLayerWayland::SetSurfaceIsFlipped(bool aIsFlipped) {
  MutexAutoLock lock(mMutex);

  if (aIsFlipped != mSurfaceIsFlipped) {
    mSurfaceIsFlipped = aIsFlipped;
  }
}

bool NativeLayerWayland::SurfaceIsFlipped() {
  MutexAutoLock lock(mMutex);

  return mSurfaceIsFlipped;
}

IntSize NativeLayerWayland::GetSize() {
  MutexAutoLock lock(mMutex);
  return mSize;
}

void NativeLayerWayland::SetPosition(const IntPoint& aPosition) {
  MutexAutoLock lock(mMutex);

  if (aPosition != mPosition) {
    mPosition = aPosition;
  }
}

IntPoint NativeLayerWayland::GetPosition() {
  MutexAutoLock lock(mMutex);
  return mPosition;
}

void NativeLayerWayland::SetTransform(const Matrix4x4& aTransform) {
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(aTransform.IsRectilinear());

  if (aTransform != mTransform) {
    mTransform = aTransform;
  }
}

void NativeLayerWayland::SetSamplingFilter(SamplingFilter aSamplingFilter) {
  MutexAutoLock lock(mMutex);

  if (aSamplingFilter != mSamplingFilter) {
    mSamplingFilter = aSamplingFilter;
  }
}

Matrix4x4 NativeLayerWayland::GetTransform() {
  MutexAutoLock lock(mMutex);
  return mTransform;
}

IntRect NativeLayerWayland::GetRect() {
  MutexAutoLock lock(mMutex);
  return IntRect(mPosition, mSize);
}

bool NativeLayerWayland::IsOpaque() {
  MutexAutoLock lock(mMutex);
  return mIsOpaque;
}

void NativeLayerWayland::SetClipRect(const Maybe<IntRect>& aClipRect) {
  MutexAutoLock lock(mMutex);

  if (aClipRect != mClipRect) {
    mClipRect = aClipRect;
  }
}

Maybe<IntRect> NativeLayerWayland::ClipRect() {
  MutexAutoLock lock(mMutex);
  return mClipRect;
}

IntRect NativeLayerWayland::CurrentSurfaceDisplayRect() {
  MutexAutoLock lock(mMutex);
  return mDisplayRect;
}

RefPtr<DrawTarget> NativeLayerWayland::NextSurfaceAsDrawTarget(
    const IntRect& aDisplayRect, const IntRegion& aUpdateRegion,
    BackendType aBackendType) {
  MutexAutoLock lock(mMutex);

  mDisplayRect = IntRect(aDisplayRect);
  mDirtyRegion = IntRegion(aUpdateRegion);

  MOZ_ASSERT(!mInProgressBuffer);
  if (mFrontBuffer && !mFrontBuffer->IsAttached()) {
    // the Wayland compositor released the buffer early, we can reuse it
    mInProgressBuffer = std::move(mFrontBuffer);
  } else {
    mInProgressBuffer = mSurfacePoolHandle->ObtainBufferFromPool(mSize);
    if (mFrontBuffer) {
      HandlePartialUpdate(lock);
      mSurfacePoolHandle->ReturnBufferToPool(mFrontBuffer);
    }
  }
  mFrontBuffer = nullptr;

  if (!mInProgressBuffer) {
    gfxCriticalError() << "Failed to obtain buffer";
    wr::RenderThread::Get()->HandleWebRenderError(
        wr::WebRenderError::NEW_SURFACE);
    return nullptr;
  }

  return mInProgressBuffer->Lock();
}

Maybe<GLuint> NativeLayerWayland::NextSurfaceAsFramebuffer(
    const IntRect& aDisplayRect, const IntRegion& aUpdateRegion,
    bool aNeedsDepth) {
  MutexAutoLock lock(mMutex);

  mDisplayRect = IntRect(aDisplayRect);
  mDirtyRegion = IntRegion(aUpdateRegion);

  MOZ_ASSERT(!mInProgressBuffer);
  if (mFrontBuffer && !mFrontBuffer->IsAttached()) {
    // the Wayland compositor released the buffer early, we can reuse it
    mInProgressBuffer = std::move(mFrontBuffer);
    mFrontBuffer = nullptr;
  } else {
    mInProgressBuffer = mSurfacePoolHandle->ObtainBufferFromPool(mSize);
  }

  if (!mInProgressBuffer) {
    gfxCriticalError() << "Failed to obtain buffer";
    wr::RenderThread::Get()->HandleWebRenderError(
        wr::WebRenderError::NEW_SURFACE);
    return Nothing();
  }

  // get the framebuffer before handling partial damage so we don't accidently
  // create one without depth buffer
  Maybe<GLuint> fbo = mSurfacePoolHandle->GetFramebufferForBuffer(
      mInProgressBuffer, aNeedsDepth);
  MOZ_RELEASE_ASSERT(fbo, "GetFramebufferForBuffer failed.");

  if (mFrontBuffer) {
    HandlePartialUpdate(lock);
    mSurfacePoolHandle->ReturnBufferToPool(mFrontBuffer);
    mFrontBuffer = nullptr;
  }

  return fbo;
}

void NativeLayerWayland::HandlePartialUpdate(
    const MutexAutoLock& aProofOfLock) {
  IntRegion copyRegion = IntRegion(mDisplayRect);
  copyRegion.SubOut(mDirtyRegion);

  if (!copyRegion.IsEmpty()) {
    if (mSurfacePoolHandle->gl()) {
      mSurfacePoolHandle->gl()->MakeCurrent();
      for (auto iter = copyRegion.RectIter(); !iter.Done(); iter.Next()) {
        gfx::IntRect r = iter.Get();
        Maybe<GLuint> sourceFB =
            mSurfacePoolHandle->GetFramebufferForBuffer(mFrontBuffer, false);
        Maybe<GLuint> destFB = mSurfacePoolHandle->GetFramebufferForBuffer(
            mInProgressBuffer, false);
        MOZ_RELEASE_ASSERT(sourceFB && destFB);
        mSurfacePoolHandle->gl()->BlitHelper()->BlitFramebufferToFramebuffer(
            sourceFB.value(), destFB.value(), r, r, LOCAL_GL_NEAREST);
      }
    } else {
      RefPtr<gfx::DataSourceSurface> dataSourceSurface =
          gfx::CreateDataSourceSurfaceFromData(
              mSize, mFrontBuffer->GetSurfaceFormat(),
              (const uint8_t*)mFrontBuffer->GetImageData(),
              mSize.width * BytesPerPixel(mFrontBuffer->GetSurfaceFormat()));
      RefPtr<DrawTarget> dt = mInProgressBuffer->Lock();

      for (auto iter = copyRegion.RectIter(); !iter.Done(); iter.Next()) {
        IntRect r = iter.Get();
        dt->CopySurface(dataSourceSurface, r, IntPoint(r.x, r.y));
      }
    }
  }
}

void NativeLayerWayland::NotifySurfaceReady() {
  MOZ_ASSERT(!mFrontBuffer);
  MOZ_ASSERT(mInProgressBuffer);
  mFrontBuffer = mInProgressBuffer;
  mInProgressBuffer = nullptr;
}

void NativeLayerWayland::DiscardBackbuffers() {}

void NativeLayerWayland::Commit() {
  MutexAutoLock lock(mMutex);

  if (mDirtyRegion.IsEmpty() && mHasBufferAttached) {
    wl_surface_commit(mWlSurface);
    return;
  }

  for (auto iter = mDirtyRegion.RectIter(); !iter.Done(); iter.Next()) {
    IntRect r = iter.Get();
    wl_surface_damage_buffer(mWlSurface, r.x, r.y, r.width, r.height);
  }

  MOZ_ASSERT(mFrontBuffer);
  mFrontBuffer->AttachAndCommit(mWlSurface);
  mHasBufferAttached = true;
  mDirtyRegion.SetEmpty();
}

void NativeLayerWayland::Unmap() {
  MutexAutoLock lock(mMutex);

  if (!mHasBufferAttached) {
    return;
  }

  wl_surface_attach(mWlSurface, nullptr, 0, 0);
  wl_surface_commit(mWlSurface);
  mHasBufferAttached = false;
}

void NativeLayerWayland::EnsureParentSurface(wl_surface* aParentSurface) {
  MutexAutoLock lock(mMutex);

  if (aParentSurface != mParentWlSurface) {
    g_clear_pointer(&mWlSubsurface, wl_subsurface_destroy);
    mSubsurfacePosition = IntPoint(0, 0);

    if (aParentSurface) {
      wl_subcompositor* subcompositor =
          widget::WaylandDisplayGet()->GetSubcompositor();
      mWlSubsurface = wl_subcompositor_get_subsurface(subcompositor, mWlSurface,
                                                      aParentSurface);
    }
    mParentWlSurface = aParentSurface;
  }
}

void NativeLayerWayland::SetBufferTransformFlipped(bool aFlipped) {
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

void NativeLayerWayland::SetSubsurfacePosition(int aX, int aY) {
  MutexAutoLock lock(mMutex);

  if ((aX == mSubsurfacePosition.x && aY == mSubsurfacePosition.y) ||
      !mWlSubsurface) {
    return;
  }

  mSubsurfacePosition.x = aX;
  mSubsurfacePosition.y = aY;
  wl_subsurface_set_position(mWlSubsurface, mSubsurfacePosition.x,
                             mSubsurfacePosition.y);
}

void NativeLayerWayland::SetViewportSourceRect(const Rect aSourceRect) {
  MutexAutoLock lock(mMutex);

  if (aSourceRect == mViewportSourceRect) {
    return;
  }

  mViewportSourceRect = aSourceRect;
  MOZ_RELEASE_ASSERT(
      (mViewportSourceRect.x >= 0 && mViewportSourceRect.y >= 0 &&
       mViewportSourceRect.width > 0 && mViewportSourceRect.height > 0) ||
      (mViewportSourceRect.x == -1 && mViewportSourceRect.y == -1 &&
       mViewportSourceRect.width == -1 && mViewportSourceRect.height == -1));
  wp_viewport_set_source(mViewport, wl_fixed_from_double(mViewportSourceRect.x),
                         wl_fixed_from_double(mViewportSourceRect.y),
                         wl_fixed_from_double(mViewportSourceRect.width),
                         wl_fixed_from_double(mViewportSourceRect.height));
}

void NativeLayerWayland::SetViewportDestinationSize(int aWidth, int aHeight) {
  MutexAutoLock lock(mMutex);

  if (aWidth == mViewportDestinationSize.width &&
      aHeight == mViewportDestinationSize.height) {
    return;
  }

  mViewportDestinationSize.width = aWidth;
  mViewportDestinationSize.height = aHeight;
  MOZ_RELEASE_ASSERT((mViewportDestinationSize.width > 0 &&
                      mViewportDestinationSize.height > 0) ||
                     (mViewportDestinationSize.width == -1 &&
                      mViewportDestinationSize.height == -1));
  wp_viewport_set_destination(mViewport, mViewportDestinationSize.width,
                              mViewportDestinationSize.height);
}

void NativeLayerWayland::RequestFrameCallback(
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
    wl_callback_add_listener(mCallback, &sFrameListenerNativeLayerWayland,
                             this);
    wl_surface_commit(mWlSurface);
  }
}

void NativeLayerWayland::FrameCallbackHandler(wl_callback* aCallback,
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
void NativeLayerWayland::FrameCallbackHandler(void* aData,
                                              wl_callback* aCallback,
                                              uint32_t aTime) {
  auto surface = reinterpret_cast<NativeLayerWayland*>(aData);
  surface->FrameCallbackHandler(aCallback, aTime);
}

}  // namespace mozilla::layers
