/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RendererOGL.h"
#include "GLContext.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/webrender/RenderCompositor.h"
#include "mozilla/webrender/RenderTextureHost.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace wr {

wr::WrExternalImage wr_renderer_lock_external_image(
    void* aObj, wr::WrExternalImageId aId, uint8_t aChannelIndex,
    wr::ImageRendering aRendering) {
  RendererOGL* renderer = reinterpret_cast<RendererOGL*>(aObj);
  RenderTextureHost* texture = renderer->GetRenderTexture(aId);
  MOZ_ASSERT(texture);
  if (!texture) {
    gfxCriticalNote << "Failed to lock ExternalImage for extId:"
                    << AsUint64(aId);
    return InvalidToWrExternalImage();
  }
  return texture->Lock(aChannelIndex, renderer->gl(), aRendering);
}

void wr_renderer_unlock_external_image(void* aObj, wr::WrExternalImageId aId,
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
                         layers::CompositorBridgeParent* aBridge)
    : mThread(aThread),
      mCompositor(std::move(aCompositor)),
      mRenderer(aRenderer),
      mBridge(aBridge),
      mWindowId(aWindowId) {
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(mCompositor);
  MOZ_ASSERT(mRenderer);
  MOZ_ASSERT(mBridge);
  MOZ_COUNT_CTOR(RendererOGL);

  mNativeLayerRoot = mCompositor->GetWidget()->GetNativeLayerRoot();
  if (mNativeLayerRoot) {
    mNativeLayerForEntireWindow = mNativeLayerRoot->CreateLayer();
    mNativeLayerRoot->AppendLayer(mNativeLayerForEntireWindow);
  }
}

RendererOGL::~RendererOGL() {
  MOZ_COUNT_DTOR(RendererOGL);
  if (!mCompositor->MakeCurrent()) {
    gfxCriticalNote
        << "Failed to make render context current during destroying.";
    // Leak resources!
    return;
  }
  if (mNativeLayerRoot) {
    mNativeLayerRoot->RemoveLayer(mNativeLayerForEntireWindow);
    mNativeLayerForEntireWindow = nullptr;
    mNativeLayerRoot = nullptr;
  }
  mCompositor->GetWidget()->DoCompositorCleanup();
  wr_renderer_delete(mRenderer);
}

wr::WrExternalImageHandler RendererOGL::GetExternalImageHandler() {
  return wr::WrExternalImageHandler{
      this,
  };
}

void RendererOGL::Update() {
  if (mCompositor->MakeCurrent()) {
    wr_renderer_update(mRenderer);
  }
}

static void DoNotifyWebRenderContextPurge(
    layers::CompositorBridgeParent* aBridge) {
  aBridge->NotifyWebRenderContextPurge();
}

bool RendererOGL::UpdateAndRender(const Maybe<gfx::IntSize>& aReadbackSize,
                                  const Maybe<wr::ImageFormat>& aReadbackFormat,
                                  const Maybe<Range<uint8_t>>& aReadbackBuffer,
                                  bool aHadSlowFrame,
                                  RendererStats* aOutStats) {
  mozilla::widget::WidgetRenderingContext widgetContext;

#if defined(XP_MACOSX)
  widgetContext.mGL = mCompositor->gl();
// TODO: we don't have a notion of compositor here.
//#elif defined(MOZ_WIDGET_ANDROID)
//  widgetContext.mCompositor = mCompositor;
#endif

  if (!mCompositor->GetWidget()->PreRender(&widgetContext)) {
    // XXX This could cause oom in webrender since pending_texture_updates is
    // not handled. It needs to be addressed.
    return false;
  }
  // XXX set clear color if MOZ_WIDGET_ANDROID is defined.

  auto size = mCompositor->GetBufferSize();

  if (mNativeLayerForEntireWindow) {
    gfx::IntRect bounds(gfx::IntPoint(0, 0), size.ToUnknownSize());
    mNativeLayerForEntireWindow->SetRect(bounds);
#ifdef XP_MACOSX
    mNativeLayerForEntireWindow->SetOpaqueRegion(
        mCompositor->GetWidget()->GetOpaqueWidgetRegion().ToUnknownRegion());
#endif
  }

  if (!mCompositor->BeginFrame(mNativeLayerForEntireWindow)) {
    if (mCompositor->IsContextLost()) {
      RenderThread::Get()->HandleDeviceReset("BeginFrame", /* aNotify */ true);
    }
    mCompositor->GetWidget()->PostRender(&widgetContext);
    return false;
  }

  wr_renderer_update(mRenderer);

  if (!wr_renderer_render(mRenderer, size.width, size.height, aHadSlowFrame,
                          aOutStats)) {
    RenderThread::Get()->HandleWebRenderError(WebRenderError::RENDER);
    mCompositor->GetWidget()->PostRender(&widgetContext);
    return false;
  }

  if (aReadbackBuffer.isSome()) {
    MOZ_ASSERT(aReadbackSize.isSome());
    MOZ_ASSERT(aReadbackFormat.isSome());
    wr_renderer_readback(mRenderer, aReadbackSize.ref().width,
                         aReadbackSize.ref().height, aReadbackFormat.ref(),
                         &aReadbackBuffer.ref()[0],
                         aReadbackBuffer.ref().length());
  }

  mScreenshotGrabber.MaybeGrabScreenshot(mRenderer, size.ToUnknownSize());

  mCompositor->EndFrame();

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

  mScreenshotGrabber.MaybeProcessQueue(mRenderer);

  // TODO: Flush pending actions such as texture deletions/unlocks and
  //       textureHosts recycling.

  return true;
}

void RendererOGL::CheckGraphicsResetStatus() {
  if (!mCompositor || !mCompositor->gl()) {
    return;
  }

  gl::GLContext* gl = mCompositor->gl();
  if (gl->IsSupported(gl::GLFeature::robustness)) {
    GLenum resetStatus = gl->fGetGraphicsResetStatus();
    if (resetStatus == LOCAL_GL_PURGED_CONTEXT_RESET_NV) {
      layers::CompositorThreadHolder::Loop()->PostTask(
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
  auto info = wr_renderer_flush_pipeline_info(mRenderer);
  return new WebRenderPipelineInfo(info);
}

RenderTextureHost* RendererOGL::GetRenderTexture(
    wr::WrExternalImageId aExternalImageId) {
  return mThread->GetRenderTexture(aExternalImageId);
}

void RendererOGL::AccumulateMemoryReport(MemoryReport* aReport) {
  wr_renderer_accumulate_memory_report(GetRenderer(), aReport);

  LayoutDeviceIntSize size = mCompositor->GetBufferSize();

  // Assume BGRA8 for the format since it's not exposed anywhere,
  // and all compositor backends should be using that.
  uintptr_t swapChainSize = size.width * size.height *
                            BytesPerPixel(SurfaceFormat::B8G8R8A8) *
                            (mCompositor->UseTripleBuffering() ? 3 : 2);
  aReport->swap_chain += swapChainSize;
}

}  // namespace wr
}  // namespace mozilla
