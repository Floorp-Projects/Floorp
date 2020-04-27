/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorOGL.h"

#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/SurfacePool.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/widget/CompositorWidget.h"

#ifdef MOZ_GECKO_PROFILER
#  include "ProfilerMarkerPayload.h"
#endif

namespace mozilla {
namespace wr {

/* static */
UniquePtr<RenderCompositor> RenderCompositorOGL::Create(
    RefPtr<widget::CompositorWidget>&& aWidget) {
  RefPtr<gl::GLContext> gl = RenderThread::Get()->SharedGL();
  if (!gl) {
    gl = gl::GLContextProvider::CreateForCompositorWidget(
        aWidget, /* aWebRender */ true, /* aForceAccelerated */ true);
    RenderThread::MaybeEnableGLDebugMessage(gl);
  }
  if (!gl || !gl->MakeCurrent()) {
    gfxCriticalNote << "Failed GL context creation for WebRender: "
                    << gfx::hexa(gl.get());
    return nullptr;
  }
  return MakeUnique<RenderCompositorOGL>(std::move(gl), std::move(aWidget));
}

RenderCompositorOGL::RenderCompositorOGL(
    RefPtr<gl::GLContext>&& aGL, RefPtr<widget::CompositorWidget>&& aWidget)
    : RenderCompositor(std::move(aWidget)),
      mGL(aGL),
      mNativeLayerRoot(GetWidget()->GetNativeLayerRoot()),
      mPreviousFrameDoneSync(nullptr),
      mThisFrameDoneSync(nullptr) {
  MOZ_ASSERT(mGL);
  if (mNativeLayerRoot) {
#ifdef XP_MACOSX
    auto pool = RenderThread::Get()->SharedSurfacePool();
    if (pool) {
      mSurfacePoolHandle = pool->GetHandleForGL(mGL);
    }
#endif
    MOZ_RELEASE_ASSERT(mSurfacePoolHandle);
  }
}

RenderCompositorOGL::~RenderCompositorOGL() {
  if (mNativeLayerRoot) {
    mNativeLayerRoot->SetLayers({});
    mNativeLayerForEntireWindow = nullptr;
    mNativeLayerRootSnapshotter = nullptr;
    mNativeLayerRoot = nullptr;
  }

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

bool RenderCompositorOGL::BeginFrame() {
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }

  gfx::IntSize bufferSize = GetBufferSize().ToUnknownSize();
  if (mNativeLayerRoot && !ShouldUseNativeCompositor()) {
    if (mNativeLayerForEntireWindow &&
        mNativeLayerForEntireWindow->GetSize() != bufferSize) {
      mNativeLayerRoot->RemoveLayer(mNativeLayerForEntireWindow);
      mNativeLayerForEntireWindow = nullptr;
    }
    if (!mNativeLayerForEntireWindow) {
      mNativeLayerForEntireWindow =
          mNativeLayerRoot->CreateLayer(bufferSize, false, mSurfacePoolHandle);
      mNativeLayerForEntireWindow->SetSurfaceIsFlipped(true);
      mNativeLayerRoot->AppendLayer(mNativeLayerForEntireWindow);
    }
  }
  if (mNativeLayerForEntireWindow) {
    gfx::IntRect bounds({}, bufferSize);
    Maybe<GLuint> fbo =
        mNativeLayerForEntireWindow->NextSurfaceAsFramebuffer(bounds, true);
    if (!fbo) {
      return false;
    }
    mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, *fbo);
  } else {
    mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mGL->GetDefaultFramebuffer());
  }

  return true;
}

RenderedFrameId RenderCompositorOGL::EndFrame(
    const nsTArray<DeviceIntRect>& aDirtyRects) {
  RenderedFrameId frameId = GetNextRenderFrameId();
  InsertFrameDoneSync();

  if (!mNativeLayerRoot) {
    mGL->SwapBuffers();
    return frameId;
  }

  if (mNativeLayerForEntireWindow) {
    mGL->fFlush();
    mNativeLayerForEntireWindow->NotifySurfaceReady();
    mNativeLayerRoot->CommitToScreen();
  }

  return frameId;
}

void RenderCompositorOGL::InsertFrameDoneSync() {
#ifdef XP_MACOSX
  // Only do this on macOS.
  // On other platforms, SwapBuffers automatically applies back-pressure.
  if (mThisFrameDoneSync) {
    mGL->fDeleteSync(mThisFrameDoneSync);
  }
  mThisFrameDoneSync = mGL->fFenceSync(LOCAL_GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#endif
}

bool RenderCompositorOGL::WaitForGPU() {
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

void RenderCompositorOGL::Pause() {}

bool RenderCompositorOGL::Resume() { return true; }

LayoutDeviceIntSize RenderCompositorOGL::GetBufferSize() {
  return mWidget->GetClientSize();
}

bool RenderCompositorOGL::ShouldUseNativeCompositor() {
  return mNativeLayerRoot && gfx::gfxVars::UseWebRenderCompositor();
}

bool RenderCompositorOGL::MaybeReadback(const gfx::IntSize& aReadbackSize,
                                        const wr::ImageFormat& aReadbackFormat,
                                        const Range<uint8_t>& aReadbackBuffer) {
  if (!ShouldUseNativeCompositor()) {
    return false;
  }

  MOZ_RELEASE_ASSERT(aReadbackFormat == wr::ImageFormat::BGRA8);
  if (!mNativeLayerRootSnapshotter) {
    mNativeLayerRootSnapshotter = mNativeLayerRoot->CreateSnapshotter();
  }
  bool success = mNativeLayerRootSnapshotter->ReadbackPixels(
      aReadbackSize, gfx::SurfaceFormat::B8G8R8A8, aReadbackBuffer);

  // ReadbackPixels might have changed the current context. Make sure mGL is
  // current again.
  mGL->MakeCurrent();

  return success;
}

uint32_t RenderCompositorOGL::GetMaxUpdateRects() {
  if (ShouldUseNativeCompositor() &&
      StaticPrefs::gfx_webrender_compositor_max_update_rects_AtStartup() > 0) {
    return 1;
  }
  return 0;
}

void RenderCompositorOGL::CompositorBeginFrame() {
  mAddedLayers.Clear();
  mAddedPixelCount = 0;
  mAddedClippedPixelCount = 0;
  mBeginFrameTimeStamp = TimeStamp::NowUnfuzzed();
  mSurfacePoolHandle->OnBeginFrame();
}

void RenderCompositorOGL::CompositorEndFrame() {
#ifdef MOZ_GECKO_PROFILER
  if (profiler_thread_is_being_profiled()) {
    auto bufferSize = GetBufferSize();
    uint64_t windowPixelCount = uint64_t(bufferSize.width) * bufferSize.height;
    int nativeLayerCount = 0;
    for (const auto& it : mSurfaces) {
      nativeLayerCount += int(it.second.mNativeLayers.size());
    }
    profiler_add_text_marker(
        "WR OS Compositor frame",
        nsPrintfCString("%d%% painting, %d%% overdraw, %d used "
                        "layers (%d%% memory) + %d unused layers (%d%% memory)",
                        int(mDrawnPixelCount * 100 / windowPixelCount),
                        int(mAddedClippedPixelCount * 100 / windowPixelCount),
                        int(mAddedLayers.Length()),
                        int(mAddedPixelCount * 100 / windowPixelCount),
                        int(nativeLayerCount - mAddedLayers.Length()),
                        int((mTotalPixelCount - mAddedPixelCount) * 100 /
                            windowPixelCount)),
        JS::ProfilingCategoryPair::GRAPHICS, mBeginFrameTimeStamp,
        TimeStamp::NowUnfuzzed());
  }
#endif
  mDrawnPixelCount = 0;

  mNativeLayerRoot->SetLayers(mAddedLayers);
  mGL->fFlush();
  mNativeLayerRoot->CommitToScreen();
  mSurfacePoolHandle->OnEndFrame();
}

void RenderCompositorOGL::Bind(wr::NativeTileId aId,
                               wr::DeviceIntPoint* aOffset, uint32_t* aFboId,
                               wr::DeviceIntRect aDirtyRect,
                               wr::DeviceIntRect aValidRect) {
  MOZ_RELEASE_ASSERT(!mCurrentlyBoundNativeLayer);

  auto surfaceCursor = mSurfaces.find(aId.surface_id);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface& surface = surfaceCursor->second;

  auto layerCursor = surface.mNativeLayers.find(TileKey(aId.x, aId.y));
  MOZ_RELEASE_ASSERT(layerCursor != surface.mNativeLayers.end());
  RefPtr<layers::NativeLayer> layer = layerCursor->second;

  gfx::IntRect validRect(aValidRect.origin.x, aValidRect.origin.y,
                         aValidRect.size.width, aValidRect.size.height);
  layer->SetValidRect(validRect);

  gfx::IntRect dirtyRect(aDirtyRect.origin.x, aDirtyRect.origin.y,
                         aDirtyRect.size.width, aDirtyRect.size.height);

  Maybe<GLuint> fbo = layer->NextSurfaceAsFramebuffer(dirtyRect, true);
  MOZ_RELEASE_ASSERT(fbo);  // TODO: make fallible
  mCurrentlyBoundNativeLayer = layer;

  *aFboId = *fbo;
  *aOffset = wr::DeviceIntPoint{0, 0};

  mDrawnPixelCount += dirtyRect.Area();
}

void RenderCompositorOGL::Unbind() {
  MOZ_RELEASE_ASSERT(mCurrentlyBoundNativeLayer);

  mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
  mCurrentlyBoundNativeLayer->NotifySurfaceReady();
  mCurrentlyBoundNativeLayer = nullptr;
}

void RenderCompositorOGL::CreateSurface(wr::NativeSurfaceId aId,
                                        wr::DeviceIntPoint aVirtualOffset,
                                        wr::DeviceIntSize aTileSize,
                                        bool aIsOpaque) {
  MOZ_RELEASE_ASSERT(mSurfaces.find(aId) == mSurfaces.end());
  mSurfaces.insert({aId, Surface{aTileSize, aIsOpaque}});
}

void RenderCompositorOGL::DestroySurface(NativeSurfaceId aId) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());

  Surface& surface = surfaceCursor->second;
  for (const auto& iter : surface.mNativeLayers) {
    mTotalPixelCount -= gfx::IntRect({}, iter.second->GetSize()).Area();
  }

  mSurfaces.erase(surfaceCursor);
}

void RenderCompositorOGL::CreateTile(wr::NativeSurfaceId aId, int aX, int aY) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface& surface = surfaceCursor->second;

  RefPtr<layers::NativeLayer> layer = mNativeLayerRoot->CreateLayer(
      surface.TileSize(), surface.mIsOpaque, mSurfacePoolHandle);
  surface.mNativeLayers.insert({TileKey(aX, aY), layer});
  mTotalPixelCount += gfx::IntRect({}, layer->GetSize()).Area();
}

void RenderCompositorOGL::DestroyTile(wr::NativeSurfaceId aId, int aX, int aY) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface& surface = surfaceCursor->second;

  auto layerCursor = surface.mNativeLayers.find(TileKey(aX, aY));
  MOZ_RELEASE_ASSERT(layerCursor != surface.mNativeLayers.end());
  RefPtr<layers::NativeLayer> layer = std::move(layerCursor->second);
  surface.mNativeLayers.erase(layerCursor);
  mTotalPixelCount -= gfx::IntRect({}, layer->GetSize()).Area();

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

void RenderCompositorOGL::AddSurface(wr::NativeSurfaceId aId,
                                     wr::DeviceIntPoint aPosition,
                                     wr::DeviceIntRect aClipRect) {
  MOZ_RELEASE_ASSERT(!mCurrentlyBoundNativeLayer);

  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  const Surface& surface = surfaceCursor->second;

  for (auto it = surface.mNativeLayers.begin();
       it != surface.mNativeLayers.end(); ++it) {
    RefPtr<layers::NativeLayer> layer = it->second;
    gfx::IntSize layerSize = layer->GetSize();
    gfx::IntRect layerRect(
        aPosition.x + surface.mTileSize.width * it->first.mX,
        aPosition.y + surface.mTileSize.height * it->first.mY, layerSize.width,
        layerSize.height);
    gfx::IntRect validRect = layer->GetValidRect() + layerRect.TopLeft();
    gfx::IntRect clipRect(aClipRect.origin.x, aClipRect.origin.y,
                          aClipRect.size.width, aClipRect.size.height);
    gfx::IntRect realClip = clipRect.Intersect(validRect);
    layer->SetPosition(layerRect.TopLeft());
    layer->SetClipRect(Some(realClip));
    mAddedLayers.AppendElement(layer);

    mAddedPixelCount += layerRect.Area();
    mAddedClippedPixelCount += realClip.Intersect(layerRect).Area();
  }
}

CompositorCapabilities RenderCompositorOGL::GetCompositorCapabilities() {
  CompositorCapabilities caps;

  // CoreAnimation doesn't use virtual surfaces
  caps.virtual_surface_size = 0;

  return caps;
}

}  // namespace wr
}  // namespace mozilla
