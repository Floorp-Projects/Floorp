/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RendererOGL.h"

#include "base/task.h"
#include "GLContext.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/ProfilerScreenshots.h"
#include "mozilla/webrender/RenderCompositor.h"
#include "mozilla/webrender/RenderTextureHost.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace wr {

wr::WrExternalImage wr_renderer_lock_external_image(
    void* aObj, wr::ExternalImageId aId, uint8_t aChannelIndex,
    wr::ImageRendering aRendering) {
  RendererOGL* renderer = reinterpret_cast<RendererOGL*>(aObj);
  RenderTextureHost* texture = renderer->GetRenderTexture(aId);
  MOZ_ASSERT(texture);
  if (!texture) {
    gfxCriticalNoteOnce << "Failed to lock ExternalImage for extId:"
                        << AsUint64(aId);
    return InvalidToWrExternalImage();
  }
  return texture->Lock(aChannelIndex, renderer->gl(), aRendering);
}

void wr_renderer_unlock_external_image(void* aObj, wr::ExternalImageId aId,
                                       uint8_t aChannelIndex) {
  RendererOGL* renderer = reinterpret_cast<RendererOGL*>(aObj);
  RenderTextureHost* texture = renderer->GetRenderTexture(aId);
  MOZ_ASSERT(texture);
  if (!texture) {
    return;
  }
  texture->Unlock();
}

RendererOGL::RendererOGL(RefPtr<RenderThread>&& aThread,
                         UniquePtr<RenderCompositor> aCompositor,
                         wr::WindowId aWindowId, wr::Renderer* aRenderer,
                         layers::CompositorBridgeParent* aBridge,
                         void* aSoftwareContext)
    : mThread(aThread),
      mCompositor(std::move(aCompositor)),
      mRenderer(aRenderer),
      mBridge(aBridge),
      mWindowId(aWindowId),
      mSoftwareContext(aSoftwareContext),
      mDisableNativeCompositor(false) {
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(mCompositor);
  MOZ_ASSERT(mRenderer);
  MOZ_ASSERT(mBridge);
  MOZ_COUNT_CTOR(RendererOGL);
}

RendererOGL::~RendererOGL() {
  MOZ_COUNT_DTOR(RendererOGL);
  if (mSoftwareContext) {
    wr_swgl_make_current(mSoftwareContext);
  }
  if (!mCompositor->MakeCurrent()) {
    gfxCriticalNote
        << "Failed to make render context current during destroying.";
    // Leak resources!
  } else {
    wr_renderer_delete(mRenderer);
  }
  if (mSoftwareContext) {
    wr_swgl_destroy_context(mSoftwareContext);
  }
}

wr::WrExternalImageHandler RendererOGL::GetExternalImageHandler() {
  return wr::WrExternalImageHandler{
      this,
  };
}

void RendererOGL::Update() {
  mCompositor->Update();
  if (mCompositor->MakeCurrent()) {
    wr_renderer_update(mRenderer);
  }
}

static void DoNotifyWebRenderContextPurge(
    layers::CompositorBridgeParent* aBridge) {
  aBridge->NotifyWebRenderContextPurge();
}

static void DoWebRenderDisableNativeCompositor(
    layers::CompositorBridgeParent* aBridge) {
  aBridge->NotifyWebRenderDisableNativeCompositor();
}

RenderedFrameId RendererOGL::UpdateAndRender(
    const Maybe<gfx::IntSize>& aReadbackSize,
    const Maybe<wr::ImageFormat>& aReadbackFormat,
    const Maybe<Range<uint8_t>>& aReadbackBuffer, RendererStats* aOutStats) {
  mozilla::widget::WidgetRenderingContext widgetContext;

#if defined(XP_MACOSX)
  widgetContext.mGL = mCompositor->gl();
#endif

  if (!mCompositor->GetWidget()->PreRender(&widgetContext)) {
    // XXX This could cause oom in webrender since pending_texture_updates is
    // not handled. It needs to be addressed.
    return RenderedFrameId();
    ;
  }
  // XXX set clear color if MOZ_WIDGET_ANDROID is defined.

  if (!mCompositor->BeginFrame()) {
    if (mCompositor->IsContextLost()) {
      RenderThread::Get()->HandleDeviceReset("BeginFrame", /* aNotify */ true);
    }
    mCompositor->GetWidget()->PostRender(&widgetContext);
    return RenderedFrameId();
  }

  auto size = mCompositor->GetBufferSize();

  if (mSoftwareContext) {
    wr_swgl_make_current(mSoftwareContext);
    wr_swgl_init_default_framebuffer(mSoftwareContext, size.width, size.height);
  }

  wr_renderer_update(mRenderer);

  bool fullRender = mCompositor->RequestFullRender();
  // When we're rendering to an external target, we want to render everything.
  if (mCompositor->UsePartialPresent() &&
      (aReadbackBuffer.isSome() || layers::ProfilerScreenshots::IsEnabled())) {
    fullRender = true;
  }
  if (fullRender) {
    wr_renderer_force_redraw(mRenderer);
  }

  nsTArray<DeviceIntRect> dirtyRects;
  if (!wr_renderer_render(mRenderer, size.width, size.height, aOutStats,
                          &dirtyRects)) {
    RenderThread::Get()->HandleWebRenderError(WebRenderError::RENDER);
    mCompositor->GetWidget()->PostRender(&widgetContext);
    return RenderedFrameId();
  }

  if (aReadbackBuffer.isSome()) {
    MOZ_ASSERT(aReadbackSize.isSome());
    MOZ_ASSERT(aReadbackFormat.isSome());
    if (!mCompositor->MaybeReadback(aReadbackSize.ref(), aReadbackFormat.ref(),
                                    aReadbackBuffer.ref())) {
      wr_renderer_readback(mRenderer, aReadbackSize.ref().width,
                           aReadbackSize.ref().height, aReadbackFormat.ref(),
                           &aReadbackBuffer.ref()[0],
                           aReadbackBuffer.ref().length());
    }
  }

  mScreenshotGrabber.MaybeGrabScreenshot(this, size.ToUnknownSize());

  RenderedFrameId frameId = mCompositor->EndFrame(dirtyRects);

  mCompositor->GetWidget()->PostRender(&widgetContext);

#if defined(ENABLE_FRAME_LATENCY_LOG)
  if (mFrameStartTime) {
    uint32_t latencyMs =
        round((TimeStamp::Now() - mFrameStartTime).ToMilliseconds());
    printf_stderr("generate frame latencyMs latencyMs %d\n", latencyMs);
  }
  // Clear frame start time
  mFrameStartTime = TimeStamp();
#endif

  mScreenshotGrabber.MaybeProcessQueue(this);

  // TODO: Flush pending actions such as texture deletions/unlocks and
  //       textureHosts recycling.

  return frameId;
}

bool RendererOGL::EnsureAsyncScreenshot() {
  if (mCompositor->SupportAsyncScreenshot()) {
    return true;
  }
  if (!mDisableNativeCompositor) {
    layers::CompositorThread()->Dispatch(
        NewRunnableFunction("DoWebRenderDisableNativeCompositorRunnable",
                            &DoWebRenderDisableNativeCompositor, mBridge));

    mDisableNativeCompositor = true;
    gfxCriticalNote << "Disable native compositor for async screenshot";
  }
  return false;
}

void RendererOGL::CheckGraphicsResetStatus() {
  if (!mCompositor || !mCompositor->gl()) {
    return;
  }

  gl::GLContext* gl = mCompositor->gl();
  if (gl->IsSupported(gl::GLFeature::robustness)) {
    GLenum resetStatus = gl->fGetGraphicsResetStatus();
    if (resetStatus == LOCAL_GL_PURGED_CONTEXT_RESET_NV) {
      layers::CompositorThread()->Dispatch(
          NewRunnableFunction("DoNotifyWebRenderContextPurgeRunnable",
                              &DoNotifyWebRenderContextPurge, mBridge));
    }
  }
}

void RendererOGL::WaitForGPU() {
  if (!mCompositor->WaitForGPU()) {
    if (mCompositor->IsContextLost()) {
      RenderThread::Get()->HandleDeviceReset("WaitForGPU", /* aNotify */ true);
    }
  }
}

RenderedFrameId RendererOGL::GetLastCompletedFrameId() {
  return mCompositor->GetLastCompletedFrameId();
}

RenderedFrameId RendererOGL::UpdateFrameId() {
  return mCompositor->UpdateFrameId();
}

void RendererOGL::Pause() { mCompositor->Pause(); }

bool RendererOGL::Resume() { return mCompositor->Resume(); }

layers::SyncObjectHost* RendererOGL::GetSyncObject() const {
  return mCompositor->GetSyncObject();
}

gl::GLContext* RendererOGL::gl() const { return mCompositor->gl(); }

void RendererOGL::SetFrameStartTime(const TimeStamp& aTime) {
  if (mFrameStartTime) {
    // frame start time is already set. This could happen when multiple
    // generate frame requests are merged by webrender.
    return;
  }
  mFrameStartTime = aTime;
}

RefPtr<WebRenderPipelineInfo> RendererOGL::FlushPipelineInfo() {
  RefPtr<WebRenderPipelineInfo> info = new WebRenderPipelineInfo();
  wr_renderer_flush_pipeline_info(mRenderer, &info->Raw());
  return info;
}

RenderTextureHost* RendererOGL::GetRenderTexture(
    wr::ExternalImageId aExternalImageId) {
  return mThread->GetRenderTexture(aExternalImageId);
}

void RendererOGL::AccumulateMemoryReport(MemoryReport* aReport) {
  wr_renderer_accumulate_memory_report(GetRenderer(), aReport);

  LayoutDeviceIntSize size = mCompositor->GetBufferSize();

  // Assume BGRA8 for the format since it's not exposed anywhere,
  // and all compositor backends should be using that.
  uintptr_t swapChainSize = size.width * size.height *
                            BytesPerPixel(gfx::SurfaceFormat::B8G8R8A8) *
                            (mCompositor->UseTripleBuffering() ? 3 : 2);
  aReport->swap_chain += swapChainSize;
}

}  // namespace wr
}  // namespace mozilla
