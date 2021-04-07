/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorNative.h"

#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/CompositionRecorder.h"
#include "mozilla/layers/NativeLayer.h"
#include "mozilla/layers/SurfacePool.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/widget/CompositorWidget.h"
#include "RenderCompositorRecordedFrame.h"

namespace mozilla {
namespace wr {

RenderCompositorNative::RenderCompositorNative(
    RefPtr<widget::CompositorWidget>&& aWidget, gl::GLContext* aGL)
    : RenderCompositor(std::move(aWidget)),
      mNativeLayerRoot(GetWidget()->GetNativeLayerRoot()) {
#ifdef XP_MACOSX
  auto pool = RenderThread::Get()->SharedSurfacePool();
  if (pool) {
    mSurfacePoolHandle = pool->GetHandleForGL(aGL);
  }
#endif
  MOZ_RELEASE_ASSERT(mSurfacePoolHandle);
}

RenderCompositorNative::~RenderCompositorNative() {
  mProfilerScreenshotGrabber.Destroy();
  mNativeLayerRoot->SetLayers({});
  mNativeLayerForEntireWindow = nullptr;
  mNativeLayerRootSnapshotter = nullptr;
  mNativeLayerRoot = nullptr;
}

bool RenderCompositorNative::BeginFrame() {
  if (!MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }

  gfx::IntSize bufferSize = GetBufferSize().ToUnknownSize();
  if (!ShouldUseNativeCompositor()) {
    if (mNativeLayerForEntireWindow &&
        mNativeLayerForEntireWindow->GetSize() != bufferSize) {
      mNativeLayerRoot->RemoveLayer(mNativeLayerForEntireWindow);
      mNativeLayerForEntireWindow = nullptr;
    }
    if (!mNativeLayerForEntireWindow) {
      mNativeLayerForEntireWindow =
          mNativeLayerRoot->CreateLayer(bufferSize, false, mSurfacePoolHandle);
      mNativeLayerRoot->AppendLayer(mNativeLayerForEntireWindow);
    }
  }

  gfx::IntRect bounds({}, bufferSize);
  if (!InitDefaultFramebuffer(bounds)) {
    return false;
  }

  return true;
}

RenderedFrameId RenderCompositorNative::EndFrame(
    const nsTArray<DeviceIntRect>& aDirtyRects) {
  RenderedFrameId frameId = GetNextRenderFrameId();

  DoSwap();

  if (mNativeLayerForEntireWindow) {
    mNativeLayerForEntireWindow->NotifySurfaceReady();
    mNativeLayerRoot->CommitToScreen();
  }

  return frameId;
}

void RenderCompositorNative::Pause() {}

bool RenderCompositorNative::Resume() { return true; }

inline layers::WebRenderCompositor RenderCompositorNative::CompositorType()
    const {
  if (gfx::gfxVars::UseWebRenderCompositor()) {
    return layers::WebRenderCompositor::CORE_ANIMATION;
  }
  return layers::WebRenderCompositor::DRAW;
}

LayoutDeviceIntSize RenderCompositorNative::GetBufferSize() {
  return mWidget->GetClientSize();
}

bool RenderCompositorNative::ShouldUseNativeCompositor() {
  return gfx::gfxVars::UseWebRenderCompositor();
}

bool RenderCompositorNative::MaybeReadback(
    const gfx::IntSize& aReadbackSize, const wr::ImageFormat& aReadbackFormat,
    const Range<uint8_t>& aReadbackBuffer, bool* aNeedsYFlip) {
  if (!ShouldUseNativeCompositor()) {
    return false;
  }

  MOZ_RELEASE_ASSERT(aReadbackFormat == wr::ImageFormat::BGRA8);
  if (!mNativeLayerRootSnapshotter) {
    mNativeLayerRootSnapshotter = mNativeLayerRoot->CreateSnapshotter();

    if (!mNativeLayerRootSnapshotter) {
      return false;
    }
  }
  bool success = mNativeLayerRootSnapshotter->ReadbackPixels(
      aReadbackSize, gfx::SurfaceFormat::B8G8R8A8, aReadbackBuffer);

  // ReadbackPixels might have changed the current context. Make sure GL is
  // current again.
  MakeCurrent();

  if (aNeedsYFlip) {
    *aNeedsYFlip = true;
  }

  return success;
}

bool RenderCompositorNative::MaybeRecordFrame(
    layers::CompositionRecorder& aRecorder) {
  if (!ShouldUseNativeCompositor()) {
    return false;
  }

  if (!mNativeLayerRootSnapshotter) {
    mNativeLayerRootSnapshotter = mNativeLayerRoot->CreateSnapshotter();
  }

  if (!mNativeLayerRootSnapshotter) {
    return true;
  }

  gfx::IntSize size = GetBufferSize().ToUnknownSize();
  RefPtr<layers::profiler_screenshots::RenderSource> snapshot =
      mNativeLayerRootSnapshotter->GetWindowContents(size);
  if (!snapshot) {
    return true;
  }

  RefPtr<layers::profiler_screenshots::AsyncReadbackBuffer> buffer =
      mNativeLayerRootSnapshotter->CreateAsyncReadbackBuffer(size);
  buffer->CopyFrom(snapshot);

  RefPtr<layers::RecordedFrame> frame =
      new RenderCompositorRecordedFrame(TimeStamp::Now(), std::move(buffer));
  aRecorder.RecordFrame(frame);

  // GetWindowContents might have changed the current context. Make sure our
  // context is current again.
  MakeCurrent();
  return true;
}

bool RenderCompositorNative::MaybeGrabScreenshot(
    const gfx::IntSize& aWindowSize) {
  if (!ShouldUseNativeCompositor()) {
    return false;
  }

  if (!mNativeLayerRootSnapshotter) {
    mNativeLayerRootSnapshotter = mNativeLayerRoot->CreateSnapshotter();
  }

  if (mNativeLayerRootSnapshotter) {
    mProfilerScreenshotGrabber.MaybeGrabScreenshot(*mNativeLayerRootSnapshotter,
                                                   aWindowSize);

    // MaybeGrabScreenshot might have changed the current context. Make sure our
    // context is current again.
    MakeCurrent();
  }

  return true;
}

bool RenderCompositorNative::MaybeProcessScreenshotQueue() {
  if (!ShouldUseNativeCompositor()) {
    return false;
  }

  mProfilerScreenshotGrabber.MaybeProcessQueue();

  // MaybeProcessQueue might have changed the current context. Make sure our
  // context is current again.
  MakeCurrent();

  return true;
}

uint32_t RenderCompositorNative::GetMaxUpdateRects() {
  if (ShouldUseNativeCompositor() &&
      StaticPrefs::gfx_webrender_compositor_max_update_rects_AtStartup() > 0) {
    return 1;
  }
  return 0;
}

void RenderCompositorNative::CompositorBeginFrame() {
  mAddedLayers.Clear();
  mAddedTilePixelCount = 0;
  mAddedClippedPixelCount = 0;
  mBeginFrameTimeStamp = TimeStamp::NowUnfuzzed();
  mSurfacePoolHandle->OnBeginFrame();
}

void RenderCompositorNative::CompositorEndFrame() {
  if (profiler_thread_is_being_profiled()) {
    auto bufferSize = GetBufferSize();
    [[maybe_unused]] uint64_t windowPixelCount =
        uint64_t(bufferSize.width) * bufferSize.height;
    int nativeLayerCount = 0;
    for (const auto& it : mSurfaces) {
      nativeLayerCount += int(it.second.mNativeLayers.size());
    }
    PROFILER_MARKER_TEXT(
        "WR OS Compositor frame", GRAPHICS,
        MarkerTiming::IntervalUntilNowFrom(mBeginFrameTimeStamp),
        nsPrintfCString("%d%% painting, %d%% overdraw, %d used "
                        "layers (%d%% memory) + %d unused layers (%d%% memory)",
                        int(mDrawnPixelCount * 100 / windowPixelCount),
                        int(mAddedClippedPixelCount * 100 / windowPixelCount),
                        int(mAddedLayers.Length()),
                        int(mAddedTilePixelCount * 100 / windowPixelCount),
                        int(nativeLayerCount - mAddedLayers.Length()),
                        int((mTotalTilePixelCount - mAddedTilePixelCount) *
                            100 / windowPixelCount)));
  }
  mDrawnPixelCount = 0;

  DoFlush();

  mNativeLayerRoot->SetLayers(mAddedLayers);
  mNativeLayerRoot->CommitToScreen();
  mSurfacePoolHandle->OnEndFrame();
}

void RenderCompositorNative::BindNativeLayer(wr::NativeTileId aId,
                                             const gfx::IntRect& aDirtyRect) {
  MOZ_RELEASE_ASSERT(!mCurrentlyBoundNativeLayer);

  auto surfaceCursor = mSurfaces.find(aId.surface_id);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface& surface = surfaceCursor->second;

  auto layerCursor = surface.mNativeLayers.find(TileKey(aId.x, aId.y));
  MOZ_RELEASE_ASSERT(layerCursor != surface.mNativeLayers.end());
  RefPtr<layers::NativeLayer> layer = layerCursor->second;

  mCurrentlyBoundNativeLayer = layer;

  mDrawnPixelCount += aDirtyRect.Area();
}

void RenderCompositorNative::UnbindNativeLayer() {
  MOZ_RELEASE_ASSERT(mCurrentlyBoundNativeLayer);

  mCurrentlyBoundNativeLayer->NotifySurfaceReady();
  mCurrentlyBoundNativeLayer = nullptr;
}

void RenderCompositorNative::CreateSurface(wr::NativeSurfaceId aId,
                                           wr::DeviceIntPoint aVirtualOffset,
                                           wr::DeviceIntSize aTileSize,
                                           bool aIsOpaque) {
  MOZ_RELEASE_ASSERT(mSurfaces.find(aId) == mSurfaces.end());
  mSurfaces.insert({aId, Surface{aTileSize, aIsOpaque}});
}

void RenderCompositorNative::CreateExternalSurface(wr::NativeSurfaceId aId,
                                                   bool aIsOpaque) {
  MOZ_RELEASE_ASSERT(mSurfaces.find(aId) == mSurfaces.end());

  RefPtr<layers::NativeLayer> layer =
      mNativeLayerRoot->CreateLayerForExternalTexture(aIsOpaque);

  Surface surface{DeviceIntSize{}, aIsOpaque};
  surface.mIsExternal = true;
  surface.mNativeLayers.insert({TileKey(0, 0), layer});

  mSurfaces.insert({aId, std::move(surface)});
}

void RenderCompositorNative::AttachExternalImage(
    wr::NativeSurfaceId aId, wr::ExternalImageId aExternalImage) {
  RenderTextureHost* image =
      RenderThread::Get()->GetRenderTexture(aExternalImage);
  MOZ_RELEASE_ASSERT(image);

  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());

  Surface& surface = surfaceCursor->second;
  MOZ_RELEASE_ASSERT(surface.mNativeLayers.size() == 1);
  MOZ_RELEASE_ASSERT(surface.mIsExternal);
  surface.mNativeLayers.begin()->second->AttachExternalImage(image);
}

void RenderCompositorNative::DestroySurface(NativeSurfaceId aId) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());

  Surface& surface = surfaceCursor->second;
  if (!surface.mIsExternal) {
    for (const auto& iter : surface.mNativeLayers) {
      mTotalTilePixelCount -= gfx::IntRect({}, iter.second->GetSize()).Area();
    }
  }

  mSurfaces.erase(surfaceCursor);
}

void RenderCompositorNative::CreateTile(wr::NativeSurfaceId aId, int aX,
                                        int aY) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface& surface = surfaceCursor->second;
  MOZ_RELEASE_ASSERT(!surface.mIsExternal);

  RefPtr<layers::NativeLayer> layer = mNativeLayerRoot->CreateLayer(
      surface.TileSize(), surface.mIsOpaque, mSurfacePoolHandle);
  surface.mNativeLayers.insert({TileKey(aX, aY), layer});
  mTotalTilePixelCount += gfx::IntRect({}, layer->GetSize()).Area();
}

void RenderCompositorNative::DestroyTile(wr::NativeSurfaceId aId, int aX,
                                         int aY) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface& surface = surfaceCursor->second;
  MOZ_RELEASE_ASSERT(!surface.mIsExternal);

  auto layerCursor = surface.mNativeLayers.find(TileKey(aX, aY));
  MOZ_RELEASE_ASSERT(layerCursor != surface.mNativeLayers.end());
  RefPtr<layers::NativeLayer> layer = std::move(layerCursor->second);
  surface.mNativeLayers.erase(layerCursor);
  mTotalTilePixelCount -= gfx::IntRect({}, layer->GetSize()).Area();

  // If the layer is currently present in mNativeLayerRoot, it will be destroyed
  // once CompositorEndFrame() replaces mNativeLayerRoot's layers and drops that
  // reference. So until that happens, the layer still needs to hold on to its
  // front buffer. However, we can tell it to drop its back buffers now, because
  // we know that we will never draw to it again.
  // Dropping the back buffers now puts them back in the surface pool, so those
  // surfaces can be immediately re-used for drawing in other layers in the
  // current frame.
  layer->DiscardBackbuffers();
}

gfx::SamplingFilter ToSamplingFilter(wr::ImageRendering aImageRendering) {
  if (aImageRendering == wr::ImageRendering::Auto) {
    return gfx::SamplingFilter::LINEAR;
  }
  return gfx::SamplingFilter::POINT;
}

void RenderCompositorNative::AddSurface(
    wr::NativeSurfaceId aId, const wr::CompositorSurfaceTransform& aTransform,
    wr::DeviceIntRect aClipRect, wr::ImageRendering aImageRendering) {
  MOZ_RELEASE_ASSERT(!mCurrentlyBoundNativeLayer);

  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  const Surface& surface = surfaceCursor->second;

  gfx::Matrix4x4 transform(
      aTransform.m11, aTransform.m12, aTransform.m13, aTransform.m14,
      aTransform.m21, aTransform.m22, aTransform.m23, aTransform.m24,
      aTransform.m31, aTransform.m32, aTransform.m33, aTransform.m34,
      aTransform.m41, aTransform.m42, aTransform.m43, aTransform.m44);

  for (auto it = surface.mNativeLayers.begin();
       it != surface.mNativeLayers.end(); ++it) {
    RefPtr<layers::NativeLayer> layer = it->second;
    gfx::IntSize layerSize = layer->GetSize();
    gfx::IntPoint layerPosition(surface.mTileSize.width * it->first.mX,
                                surface.mTileSize.height * it->first.mY);
    layer->SetPosition(layerPosition);
    gfx::IntRect clipRect(aClipRect.origin.x, aClipRect.origin.y,
                          aClipRect.size.width, aClipRect.size.height);
    layer->SetClipRect(Some(clipRect));
    layer->SetTransform(transform);
    layer->SetSamplingFilter(ToSamplingFilter(aImageRendering));
    mAddedLayers.AppendElement(layer);

    if (!surface.mIsExternal) {
      mAddedTilePixelCount += layerSize.width * layerSize.height;
    }
    gfx::Rect r = transform.TransformBounds(
        gfx::Rect(layer->CurrentSurfaceDisplayRect()));
    gfx::IntRect visibleRect =
        clipRect.Intersect(RoundedToInt(r) + layerPosition);
    mAddedClippedPixelCount += visibleRect.Area();
  }
}

/* static */
UniquePtr<RenderCompositor> RenderCompositorNativeOGL::Create(
    RefPtr<widget::CompositorWidget>&& aWidget, nsACString& aError) {
  RefPtr<gl::GLContext> gl = RenderThread::Get()->SingletonGL();
  if (!gl) {
    gl = gl::GLContextProvider::CreateForCompositorWidget(
        aWidget, /* aHardwareWebRender */ true, /* aForceAccelerated */ true);
    RenderThread::MaybeEnableGLDebugMessage(gl);
  }
  if (!gl || !gl->MakeCurrent()) {
    gfxCriticalNote << "Failed GL context creation for WebRender: "
                    << gfx::hexa(gl.get());
    return nullptr;
  }
  return MakeUnique<RenderCompositorNativeOGL>(std::move(aWidget),
                                               std::move(gl));
}

RenderCompositorNativeOGL::RenderCompositorNativeOGL(
    RefPtr<widget::CompositorWidget>&& aWidget, RefPtr<gl::GLContext>&& aGL)
    : RenderCompositorNative(std::move(aWidget), aGL), mGL(aGL) {
  MOZ_ASSERT(mGL);
}

RenderCompositorNativeOGL::~RenderCompositorNativeOGL() {
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote
        << "Failed to make render context current during destroying.";
    // Leak resources!
    mPreviousFrameDoneSync = nullptr;
    mThisFrameDoneSync = nullptr;
    return;
  }

  if (mPreviousFrameDoneSync) {
    mGL->fDeleteSync(mPreviousFrameDoneSync);
  }
  if (mThisFrameDoneSync) {
    mGL->fDeleteSync(mThisFrameDoneSync);
  }
}

bool RenderCompositorNativeOGL::InitDefaultFramebuffer(
    const gfx::IntRect& aBounds) {
  if (mNativeLayerForEntireWindow) {
    Maybe<GLuint> fbo = mNativeLayerForEntireWindow->NextSurfaceAsFramebuffer(
        aBounds, aBounds, true);
    if (!fbo) {
      return false;
    }
    mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, *fbo);
  } else {
    mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mGL->GetDefaultFramebuffer());
  }
  return true;
}

void RenderCompositorNativeOGL::DoSwap() {
  InsertFrameDoneSync();
  if (mNativeLayerForEntireWindow) {
    mGL->fFlush();
  }
}

void RenderCompositorNativeOGL::DoFlush() { mGL->fFlush(); }

void RenderCompositorNativeOGL::InsertFrameDoneSync() {
#ifdef XP_MACOSX
  // Only do this on macOS.
  // On other platforms, SwapBuffers automatically applies back-pressure.
  if (mThisFrameDoneSync) {
    mGL->fDeleteSync(mThisFrameDoneSync);
  }
  mThisFrameDoneSync = mGL->fFenceSync(LOCAL_GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#endif
}

bool RenderCompositorNativeOGL::WaitForGPU() {
  if (mPreviousFrameDoneSync) {
    AUTO_PROFILER_LABEL("Waiting for GPU to finish previous frame", GRAPHICS);
    mGL->fClientWaitSync(mPreviousFrameDoneSync,
                         LOCAL_GL_SYNC_FLUSH_COMMANDS_BIT,
                         LOCAL_GL_TIMEOUT_IGNORED);
    mGL->fDeleteSync(mPreviousFrameDoneSync);
  }
  mPreviousFrameDoneSync = mThisFrameDoneSync;
  mThisFrameDoneSync = nullptr;

  return true;
}

void RenderCompositorNativeOGL::Bind(wr::NativeTileId aId,
                                     wr::DeviceIntPoint* aOffset,
                                     uint32_t* aFboId,
                                     wr::DeviceIntRect aDirtyRect,
                                     wr::DeviceIntRect aValidRect) {
  gfx::IntRect validRect(aValidRect.origin.x, aValidRect.origin.y,
                         aValidRect.size.width, aValidRect.size.height);
  gfx::IntRect dirtyRect(aDirtyRect.origin.x, aDirtyRect.origin.y,
                         aDirtyRect.size.width, aDirtyRect.size.height);

  BindNativeLayer(aId, dirtyRect);

  Maybe<GLuint> fbo = mCurrentlyBoundNativeLayer->NextSurfaceAsFramebuffer(
      validRect, dirtyRect, true);

  *aFboId = *fbo;
  *aOffset = wr::DeviceIntPoint{0, 0};
}

void RenderCompositorNativeOGL::Unbind() {
  mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  UnbindNativeLayer();
}

/* static */
UniquePtr<RenderCompositor> RenderCompositorNativeSWGL::Create(
    RefPtr<widget::CompositorWidget>&& aWidget, nsACString& aError) {
  void* ctx = wr_swgl_create_context();
  if (!ctx) {
    gfxCriticalNote << "Failed SWGL context creation for WebRender";
    return nullptr;
  }
  return MakeUnique<RenderCompositorNativeSWGL>(std::move(aWidget), ctx);
}

RenderCompositorNativeSWGL::RenderCompositorNativeSWGL(
    RefPtr<widget::CompositorWidget>&& aWidget, void* aContext)
    : RenderCompositorNative(std::move(aWidget)), mContext(aContext) {
  MOZ_ASSERT(mContext);
}

RenderCompositorNativeSWGL::~RenderCompositorNativeSWGL() {
  wr_swgl_destroy_context(mContext);
}

bool RenderCompositorNativeSWGL::MakeCurrent() {
  wr_swgl_make_current(mContext);
  return true;
}

bool RenderCompositorNativeSWGL::InitDefaultFramebuffer(
    const gfx::IntRect& aBounds) {
  if (mNativeLayerForEntireWindow) {
    if (!MapNativeLayer(mNativeLayerForEntireWindow, aBounds, aBounds)) {
      return false;
    }
    wr_swgl_init_default_framebuffer(mContext, aBounds.x, aBounds.y,
                                     aBounds.width, aBounds.height,
                                     mLayerStride, mLayerValidRectData);
  }
  return true;
}

void RenderCompositorNativeSWGL::CancelFrame() {
  if (mNativeLayerForEntireWindow && mLayerTarget) {
    wr_swgl_init_default_framebuffer(mContext, 0, 0, 0, 0, 0, nullptr);
    UnmapNativeLayer();
  }
}

void RenderCompositorNativeSWGL::DoSwap() {
  if (mNativeLayerForEntireWindow && mLayerTarget) {
    wr_swgl_init_default_framebuffer(mContext, 0, 0, 0, 0, 0, nullptr);
    UnmapNativeLayer();
  }
}

bool RenderCompositorNativeSWGL::MapNativeLayer(
    layers::NativeLayer* aLayer, const gfx::IntRect& aDirtyRect,
    const gfx::IntRect& aValidRect) {
  uint8_t* data = nullptr;
  gfx::IntSize size;
  int32_t stride = 0;
  gfx::SurfaceFormat format = gfx::SurfaceFormat::UNKNOWN;
  RefPtr<gfx::DrawTarget> dt = aLayer->NextSurfaceAsDrawTarget(
      aValidRect, gfx::IntRegion(aDirtyRect), gfx::BackendType::SKIA);
  if (!dt || !dt->LockBits(&data, &size, &stride, &format)) {
    return false;
  }
  MOZ_ASSERT(format == gfx::SurfaceFormat::B8G8R8A8 ||
             format == gfx::SurfaceFormat::B8G8R8X8);
  mLayerTarget = std::move(dt);
  mLayerData = data;
  mLayerValidRectData = data + aValidRect.y * stride + aValidRect.x * 4;
  mLayerStride = stride;
  return true;
}

void RenderCompositorNativeSWGL::UnmapNativeLayer() {
  MOZ_ASSERT(mLayerTarget && mLayerData);
  mLayerTarget->ReleaseBits(mLayerData);
  mLayerTarget = nullptr;
  mLayerData = nullptr;
  mLayerValidRectData = nullptr;
  mLayerStride = 0;
}

bool RenderCompositorNativeSWGL::MapTile(wr::NativeTileId aId,
                                         wr::DeviceIntRect aDirtyRect,
                                         wr::DeviceIntRect aValidRect,
                                         void** aData, int32_t* aStride) {
  if (mNativeLayerForEntireWindow) {
    return false;
  }
  gfx::IntRect dirtyRect(aDirtyRect.origin.x, aDirtyRect.origin.y,
                         aDirtyRect.size.width, aDirtyRect.size.height);
  gfx::IntRect validRect(aValidRect.origin.x, aValidRect.origin.y,
                         aValidRect.size.width, aValidRect.size.height);
  BindNativeLayer(aId, dirtyRect);
  if (!MapNativeLayer(mCurrentlyBoundNativeLayer, dirtyRect, validRect)) {
    UnbindNativeLayer();
    return false;
  }
  *aData = mLayerValidRectData;
  *aStride = mLayerStride;
  return true;
}

void RenderCompositorNativeSWGL::UnmapTile() {
  if (!mNativeLayerForEntireWindow && mCurrentlyBoundNativeLayer) {
    UnmapNativeLayer();
    UnbindNativeLayer();
  }
}

}  // namespace wr
}  // namespace mozilla
