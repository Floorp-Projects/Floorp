/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/NativeLayerWayland.h"

#include <dlfcn.h>
#include <utility>
#include <algorithm>

#include "gfxUtils.h"
#include "GLContextProvider.h"
#include "mozilla/layers/SurfacePoolWayland.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla::layers {

using gfx::Point;
using gfx::Rect;
using gfx::Size;

/* static */
already_AddRefed<NativeLayerRootWayland>
NativeLayerRootWayland::CreateForMozContainer(MozContainer* aContainer) {
  RefPtr<NativeLayerRootWayland> layerRoot =
      new NativeLayerRootWayland(aContainer);
  return layerRoot.forget();
}

NativeLayerRootWayland::NativeLayerRootWayland(MozContainer* aContainer)
    : mMutex("NativeLayerRootWayland"), mContainer(aContainer) {}

void NativeLayerRootWayland::EnsureSurfaceInitialized() {
  if (mShmBuffer) {
    return;
  }

  wl_surface* wlSurface = moz_container_wayland_surface_lock(mContainer);
  if (wlSurface) {
    mShmBuffer = widget::WaylandShmBuffer::Create(widget::WaylandDisplayGet(),
                                                  LayoutDeviceIntSize(1, 1));
    mShmBuffer->Clear();

    mShmBuffer->AttachAndCommit(wlSurface);
    moz_container_wayland_surface_unlock(mContainer, &wlSurface);
  }
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

bool NativeLayerRootWayland::EnsureShowLayer(
    const RefPtr<NativeLayerWayland>& aLayer) {
  if (aLayer->mIsShown) {
    return true;
  }

  RefPtr<NativeSurfaceWayland> nativeSurface = aLayer->mNativeSurface;
  if (!nativeSurface->HasSubsurface()) {
    wl_surface* containerSurface =
        moz_container_wayland_surface_lock(mContainer);
    if (!containerSurface) {
      return false;
    }

    nativeSurface->CreateSubsurface(containerSurface);
    moz_container_wayland_surface_unlock(mContainer, &containerSurface);
  }

  aLayer->mIsShown = true;
  return true;
}

void NativeLayerRootWayland::EnsureHideLayer(
    const RefPtr<NativeLayerWayland>& aLayer) {
  if (!aLayer->mIsShown) {
    return;
  }

  RefPtr<NativeSurfaceWayland> nativeSurface = aLayer->mNativeSurface;

  nativeSurface->SetPosition(20, 20);
  nativeSurface->SetViewportSourceRect(Rect(0, 0, 1, 1));
  nativeSurface->SetViewportDestinationSize(1, 1);
  wl_surface_commit(nativeSurface->mWlSurface);

  wl_surface* wlSurface = moz_container_wayland_surface_lock(mContainer);
  if (wlSurface) {
    wl_subsurface_place_below(nativeSurface->mWlSubsurface, wlSurface);
    moz_container_wayland_surface_unlock(mContainer, &wlSurface);
  }

  aLayer->mIsShown = false;
}

void NativeLayerRootWayland::UnmapLayer(
    const RefPtr<NativeLayerWayland>& aLayer) {
  aLayer->mNativeSurface->ClearSubsurface();
  aLayer->mIsShown = false;
}

void NativeLayerRootWayland::SetLayers(
    const nsTArray<RefPtr<NativeLayer>>& aLayers) {
  MutexAutoLock lock(mMutex);

  // Ideally, we'd just be able to do mSublayers = std::move(aLayers).
  // However, aLayers has a different type: it carries NativeLayer objects,
  // whereas mSublayers carries NativeLayerWayland objects, so we have to
  // downcast all the elements first. There's one other reason to look at all
  // the elements in aLayers first: We need to make sure any new layers know
  // about our current backing scale.

  nsTArray<RefPtr<NativeLayerWayland>> newSublayers(aLayers.Length());
  for (const RefPtr<NativeLayer>& layer : aLayers) {
    RefPtr<NativeLayerWayland> layerWayland = layer->AsNativeLayerWayland();
    MOZ_RELEASE_ASSERT(layerWayland);
    newSublayers.AppendElement(std::move(layerWayland));
  }

  nsTArray<RefPtr<NativeLayerWayland>> newVisibleSublayers(aLayers.Length());
  for (const RefPtr<NativeLayerWayland>& layer : newSublayers) {
    RefPtr<NativeSurfaceWayland> nativeSurface = layer->mNativeSurface;

    MOZ_RELEASE_ASSERT(layer->mTransform.Is2D());
    auto transform2D = layer->mTransform.As2D();

    Rect surfaceRectClipped =
        Rect(0, 0, (float)layer->mSize.width, (float)layer->mSize.height);
    surfaceRectClipped = surfaceRectClipped.Intersect(Rect(layer->mValidRect));

    transform2D.PostTranslate((float)layer->mPosition.x,
                              (float)layer->mPosition.y);
    surfaceRectClipped = transform2D.TransformBounds(surfaceRectClipped);

    if (layer->mClipRect) {
      surfaceRectClipped =
          surfaceRectClipped.Intersect(Rect(layer->mClipRect.value()));
    }

    if (roundf(surfaceRectClipped.width) > 0 &&
        roundf(surfaceRectClipped.height) > 0) {
      if (!EnsureShowLayer(layer)) {
        continue;
      }
      newVisibleSublayers.AppendElement(layer);
    } else {
      EnsureHideLayer(layer);
      continue;
    }

    nativeSurface->SetBufferTransformFlipped(transform2D.HasNegativeScaling());

    double bufferScale = moz_container_wayland_get_scale(mContainer);
    nativeSurface->SetPosition(floor(surfaceRectClipped.x / bufferScale),
                               floor(surfaceRectClipped.y / bufferScale));
    nativeSurface->SetViewportDestinationSize(
        ceil(surfaceRectClipped.width / bufferScale),
        ceil(surfaceRectClipped.height / bufferScale));

    auto transform2DInversed = transform2D.Inverse();
    Rect bufferClip = transform2DInversed.TransformBounds(surfaceRectClipped);
    nativeSurface->SetViewportSourceRect(bufferClip);
  }

  if (newVisibleSublayers != mSublayers) {
    for (const RefPtr<NativeLayerWayland>& layer : mSublayers) {
      if (!newVisibleSublayers.Contains(layer)) {
        EnsureHideLayer(layer);
      }
    }

    wl_surface* previousSurface = nullptr;
    for (const RefPtr<NativeLayerWayland>& layer : newVisibleSublayers) {
      RefPtr<NativeSurfaceWayland> nativeSurface = layer->mNativeSurface;
      if (previousSurface) {
        wl_subsurface_place_above(nativeSurface->mWlSubsurface,
                                  previousSurface);
        previousSurface = nativeSurface->mWlSurface;
      } else {
        wl_surface* wlSurface = moz_container_wayland_surface_lock(mContainer);
        if (wlSurface) {
          wl_subsurface_place_above(nativeSurface->mWlSubsurface, wlSurface);
          moz_container_wayland_surface_unlock(mContainer, &wlSurface);
        }
        previousSurface = nativeSurface->mWlSurface;
      }
    }
    mSublayers = std::move(newVisibleSublayers);
  }

  nsCOMPtr<nsIRunnable> updateLayersRunnable = NewRunnableMethod<>(
      "layers::NativeLayerRootWayland::UpdateLayersOnMainThread", this,
      &NativeLayerRootWayland::UpdateLayersOnMainThread);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThreadQueue(
      updateLayersRunnable.forget(), EventQueuePriority::Normal));
}

static void sAfterFrameClockAfterPaint(
    GdkFrameClock* aClock, NativeLayerRootWayland* aNativeLayerRoot) {
  aNativeLayerRoot->AfterFrameClockAfterPaint();
}

void NativeLayerRootWayland::AfterFrameClockAfterPaint() {
  MutexAutoLock lock(mMutex);
  wl_surface* containerSurface = moz_container_wayland_surface_lock(mContainer);

  for (const RefPtr<NativeLayerWayland>& layer : mSublayersOnMainThread) {
    wl_surface_commit(layer->mNativeSurface->mWlSurface);
  }
  if (containerSurface) {
    wl_surface_commit(containerSurface);
    moz_container_wayland_surface_unlock(mContainer, &containerSurface);
  }

  wl_display_flush(widget::WaylandDisplayGet()->GetDisplay());
}

void NativeLayerRootWayland::UpdateLayersOnMainThread() {
  AssertIsOnMainThread();
  MutexAutoLock lock(mMutex);

  if (!mCompositorRunning) {
    return;
  }

  static auto sGdkWaylandWindowAddCallbackSurface =
      (void (*)(GdkWindow*, struct wl_surface*))dlsym(
          RTLD_DEFAULT, "gdk_wayland_window_add_frame_callback_surface");
  static auto sGdkWaylandWindowRemoveCallbackSurface =
      (void (*)(GdkWindow*, struct wl_surface*))dlsym(
          RTLD_DEFAULT, "gdk_wayland_window_remove_frame_callback_surface");

  GdkWindow* gdkWindow = gtk_widget_get_window(GTK_WIDGET(mContainer));
  wl_surface* containerSurface = moz_container_wayland_surface_lock(mContainer);

  mSublayersOnMainThread.RemoveElementsBy([&](const auto& layer) {
    if (!mSublayers.Contains(layer)) {
      if (layer->IsOpaque() &&
          StaticPrefs::widget_wayland_opaque_region_enabled_AtStartup() &&
          sGdkWaylandWindowAddCallbackSurface && gdkWindow) {
        wl_surface* layerSurface = layer->mNativeSurface->mWlSurface;

        sGdkWaylandWindowRemoveCallbackSurface(gdkWindow, layerSurface);

        wl_compositor* compositor =
            widget::WaylandDisplayGet()->GetCompositor();
        wl_region* region = wl_compositor_create_region(compositor);
        wl_surface_set_opaque_region(layerSurface, region);
        wl_region_destroy(region);
        wl_surface_commit(layerSurface);
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
        wl_surface* layerSurface = layer->mNativeSurface->mWlSurface;

        sGdkWaylandWindowAddCallbackSurface(gdkWindow, layerSurface);

        wl_compositor* compositor =
            widget::WaylandDisplayGet()->GetCompositor();
        wl_region* region = wl_compositor_create_region(compositor);
        wl_region_add(region, 0, 0, INT32_MAX, INT32_MAX);
        wl_surface_set_opaque_region(layerSurface, region);
        wl_region_destroy(region);
        wl_surface_commit(layerSurface);
      }
      if (mCallbackMultiplexHelper && mCallbackMultiplexHelper->IsActive()) {
        layer->mNativeSurface->RequestFrameCallback(mCallbackMultiplexHelper);
      }
      mSublayersOnMainThread.AppendElement(layer);
    }
  }

  if (containerSurface) {
    wl_surface_commit(containerSurface);
    moz_container_wayland_surface_unlock(mContainer, &containerSurface);
  }

  GdkFrameClock* frame_clock = gdk_window_get_frame_clock(gdkWindow);
  if (!mGdkAfterPaintId) {
    mGdkAfterPaintId =
        g_signal_connect_after(frame_clock, "after-paint",
                               G_CALLBACK(sAfterFrameClockAfterPaint), this);
  }
}

void NativeLayerRootWayland::RequestFrameCallback(CallbackFunc aCallbackFunc,
                                                  void* aCallbackData) {
  MutexAutoLock lock(mMutex);

  mCallbackMultiplexHelper =
      new CallbackMultiplexHelper(aCallbackFunc, aCallbackData);

  for (const RefPtr<NativeLayerWayland>& layer : mSublayersOnMainThread) {
    layer->mNativeSurface->RequestFrameCallback(mCallbackMultiplexHelper);
  }

  wl_surface* wlSurface = moz_container_wayland_surface_lock(mContainer);
  if (wlSurface) {
    wl_surface_commit(wlSurface);
    wl_display_flush(widget::WaylandDisplayGet()->GetDisplay());
    moz_container_wayland_surface_unlock(mContainer, &wlSurface);
  }
}

bool NativeLayerRootWayland::CommitToScreen() {
  MutexAutoLock lock(mMutex);

  wl_surface* wlSurface = moz_container_wayland_surface_lock(mContainer);
  for (RefPtr<NativeLayerWayland>& layer : mSublayers) {
    layer->mNativeSurface->Commit(layer->mDirtyRegion);
    layer->mDirtyRegion.SetEmpty();
  }

  if (wlSurface) {
    wl_surface_commit(wlSurface);
    moz_container_wayland_surface_unlock(mContainer, &wlSurface);
  }

  wl_display_flush(widget::WaylandDisplayGet()->GetDisplay());

  EnsureSurfaceInitialized();
  return true;
}

void NativeLayerRootWayland::PauseCompositor() {
  MutexAutoLock lock(mMutex);

  for (RefPtr<NativeLayerWayland>& layer : mSublayers) {
    UnmapLayer(layer);
  }

  mCompositorRunning = false;
  mShmBuffer = nullptr;
}

bool NativeLayerRootWayland::ResumeCompositor() {
  MutexAutoLock lock(mMutex);

  mCompositorRunning = true;
  return true;
}

UniquePtr<NativeLayerRootSnapshotter>
NativeLayerRootWayland::CreateSnapshotter() {
  MutexAutoLock lock(mMutex);
  return nullptr;
}

NativeLayerWayland::NativeLayerWayland(
    const IntSize& aSize, bool aIsOpaque,
    SurfacePoolHandleWayland* aSurfacePoolHandle)
    : mMutex("NativeLayerWayland"),
      mSurfacePoolHandle(aSurfacePoolHandle),
      mSize(aSize),
      mIsOpaque(aIsOpaque),
      mNativeSurface(nullptr) {
  MOZ_RELEASE_ASSERT(mSurfacePoolHandle,
                     "Need a non-null surface pool handle.");
}

NativeLayerWayland::NativeLayerWayland(bool aIsOpaque)
    : mMutex("NativeLayerWayland"),
      mSurfacePoolHandle(nullptr),
      mIsOpaque(aIsOpaque) {
  MOZ_RELEASE_ASSERT(false);  // external image
}

NativeLayerWayland::~NativeLayerWayland() {
  if (mNativeSurface) {
    mSurfacePoolHandle->ReturnSurfaceToPool(mNativeSurface);
  }
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

  mValidRect = IntRect(aDisplayRect);
  mDirtyRegion = IntRegion(aUpdateRegion);

  if (!mNativeSurface) {
    mNativeSurface = mSurfacePoolHandle->ObtainSurfaceFromPool(mSize);
  }

  return mNativeSurface->GetAsDrawTarget();
}

Maybe<GLuint> NativeLayerWayland::NextSurfaceAsFramebuffer(
    const IntRect& aDisplayRect, const IntRegion& aUpdateRegion,
    bool aNeedsDepth) {
  MutexAutoLock lock(mMutex);

  mValidRect = IntRect(aDisplayRect);
  mDirtyRegion = IntRegion(aUpdateRegion);

  if (!mNativeSurface) {
    mNativeSurface = mSurfacePoolHandle->ObtainSurfaceFromPool(mSize);
  }

  return mNativeSurface ? mNativeSurface->GetAsFramebuffer() : Nothing();
}

void NativeLayerWayland::NotifySurfaceReady() {
  MutexAutoLock lock(mMutex);

  mNativeSurface->NotifySurfaceReady();
}

void NativeLayerWayland::DiscardBackbuffers() {}

}  // namespace mozilla::layers
