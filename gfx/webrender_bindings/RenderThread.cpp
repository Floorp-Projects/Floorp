/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/task.h"
#include "GeckoProfiler.h"
#include "gfxPlatform.h"
#include "GLContext.h"
#include "RenderThread.h"
#include "nsThreadUtils.h"
#include "transport/runnable_utils.h"
#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/WebRenderBridgeParent.h"
#include "mozilla/layers/SharedSurfacesParent.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/webrender/RendererOGL.h"
#include "mozilla/webrender/RenderTextureHost.h"
#include "mozilla/widget/CompositorWidget.h"
#include "OGLShaderProgram.h"

#ifdef XP_WIN
#  include "GLContextEGL.h"
#  include "GLLibraryEGL.h"
#  include "mozilla/widget/WinCompositorWindowThread.h"
#  include "mozilla/gfx/DeviceManagerDx.h"
//#  include "nsWindowsHelpers.h"
//#  include <d3d11.h>
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "GLLibraryEGL.h"
#  include "mozilla/webrender/RenderAndroidSurfaceTextureHost.h"
#endif

#ifdef MOZ_WIDGET_GTK
#  include "mozilla/WidgetUtilsGtk.h"
#endif

#ifdef MOZ_WAYLAND
#  include "GLLibraryEGL.h"
#endif

using namespace mozilla;

static already_AddRefed<gl::GLContext> CreateGLContext(nsACString& aError);

MOZ_DEFINE_MALLOC_SIZE_OF(WebRenderRendererMallocSizeOf)

namespace mozilla::wr {

static StaticRefPtr<RenderThread> sRenderThread;

RenderThread::RenderThread(base::Thread* aThread)
    : mThread(aThread),
      mThreadPool(false),
      mThreadPoolLP(true),
      mSingletonGLIsForHardwareWebRender(true),
      mWindowInfos("RenderThread.mWindowInfos"),
      mRenderTextureMapLock("RenderThread.mRenderTextureMapLock"),
      mHasShutdown(false),
      mHandlingDeviceReset(false),
      mHandlingWebRenderError(false) {}

RenderThread::~RenderThread() {
  MOZ_ASSERT(mRenderTexturesDeferred.empty());
  delete mThread;
}

// static
RenderThread* RenderThread::Get() { return sRenderThread; }

// static
void RenderThread::Start() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sRenderThread);

  base::Thread* thread = new base::Thread("Renderer");

  base::Thread::Options options;
  // TODO(nical): The compositor thread has a bunch of specific options, see
  // which ones make sense here.
  if (!thread->StartWithOptions(options)) {
    delete thread;
    return;
  }

  sRenderThread = new RenderThread(thread);
#ifdef XP_WIN
  widget::WinCompositorWindowThread::Start();
#endif
  layers::SharedSurfacesParent::Initialize();

  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<RenderThread>(sRenderThread.get()), &RenderThread::InitDeviceTask);
  sRenderThread->Loop()->PostTask(runnable.forget());
}

// static
void RenderThread::ShutDown() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sRenderThread);

  {
    MutexAutoLock lock(sRenderThread->mRenderTextureMapLock);
    sRenderThread->mHasShutdown = true;
  }

  layers::SynchronousTask task("RenderThread");
  RefPtr<Runnable> runnable =
      WrapRunnable(RefPtr<RenderThread>(sRenderThread.get()),
                   &RenderThread::ShutDownTask, &task);
  sRenderThread->Loop()->PostTask(runnable.forget());
  task.Wait();

  layers::SharedSurfacesParent::Shutdown();

  sRenderThread = nullptr;
#ifdef XP_WIN
  if (widget::WinCompositorWindowThread::Get()) {
    widget::WinCompositorWindowThread::ShutDown();
  }
#endif
}

extern void ClearAllBlobImageResources();

void RenderThread::ShutDownTask(layers::SynchronousTask* aTask) {
  layers::AutoCompleteTask complete(aTask);
  MOZ_ASSERT(IsInRenderThread());

  // Let go of our handle to the (internally ref-counted) thread pool.
  mThreadPool.Release();
  mThreadPoolLP.Release();

  // Releasing on the render thread will allow us to avoid dispatching to remove
  // remaining textures from the texture map.
  layers::SharedSurfacesParent::ShutdownRenderThread();

  ClearAllBlobImageResources();
  ClearSingletonGL();
  ClearSharedSurfacePool();
}

// static
MessageLoop* RenderThread::Loop() {
  return sRenderThread ? sRenderThread->mThread->message_loop() : nullptr;
}

// static
bool RenderThread::IsInRenderThread() {
  return sRenderThread &&
         sRenderThread->mThread->thread_id() == PlatformThread::CurrentId();
}

void RenderThread::DoAccumulateMemoryReport(
    MemoryReport aReport,
    const RefPtr<MemoryReportPromise::Private>& aPromise) {
  MOZ_ASSERT(IsInRenderThread());

  for (auto& r : mRenderers) {
    r.second->AccumulateMemoryReport(&aReport);
  }

  // Note memory used by the shader cache, which is shared across all WR
  // instances.
  MOZ_ASSERT(aReport.shader_cache == 0);
  if (mProgramCache) {
    aReport.shader_cache = wr_program_cache_report_memory(
        mProgramCache->Raw(), &WebRenderRendererMallocSizeOf);
  }

  size_t renderTextureMemory = 0;
  {
    MutexAutoLock lock(mRenderTextureMapLock);
    for (const auto& entry : mRenderTextures) {
      renderTextureMemory += entry.second->Bytes();
    }
  }
  aReport.render_texture_hosts = renderTextureMemory;

  aPromise->Resolve(aReport, __func__);
}

// static
RefPtr<MemoryReportPromise> RenderThread::AccumulateMemoryReport(
    MemoryReport aInitial) {
  RefPtr<MemoryReportPromise::Private> p =
      new MemoryReportPromise::Private(__func__);
  MOZ_ASSERT(!IsInRenderThread());
  if (!Get() || !Get()->Loop()) {
    // This happens when the GPU process fails to start and we fall back to the
    // basic compositor in the parent process. We could assert against this if
    // we made the webrender detection code in gfxPlatform.cpp smarter. See bug
    // 1494430 comment 12.
    NS_WARNING("No render thread, returning empty memory report");
    p->Resolve(aInitial, __func__);
    return p;
  }

  Get()->Loop()->PostTask(
      NewRunnableMethod<MemoryReport, RefPtr<MemoryReportPromise::Private>>(
          "wr::RenderThread::DoAccumulateMemoryReport", Get(),
          &RenderThread::DoAccumulateMemoryReport, aInitial, p));

  return p;
}

void RenderThread::AddRenderer(wr::WindowId aWindowId,
                               UniquePtr<RendererOGL> aRenderer) {
  MOZ_ASSERT(IsInRenderThread());

  if (mHasShutdown) {
    return;
  }

  mRenderers[aWindowId] = std::move(aRenderer);

  auto windows = mWindowInfos.Lock();
  windows->emplace(AsUint64(aWindowId), new WindowInfo());
}

void RenderThread::RemoveRenderer(wr::WindowId aWindowId) {
  MOZ_ASSERT(IsInRenderThread());

  if (mHasShutdown) {
    return;
  }

  mRenderers.erase(aWindowId);

  if (mRenderers.empty()) {
    mHandlingDeviceReset = false;
    mHandlingWebRenderError = false;
  }

  auto windows = mWindowInfos.Lock();
  auto it = windows->find(AsUint64(aWindowId));
  MOZ_ASSERT(it != windows->end());
  WindowInfo* toDelete = it->second;
  windows->erase(it);
  delete toDelete;
}

RendererOGL* RenderThread::GetRenderer(wr::WindowId aWindowId) {
  MOZ_ASSERT(IsInRenderThread());

  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());

  if (it == mRenderers.end()) {
    return nullptr;
  }

  return it->second.get();
}

size_t RenderThread::RendererCount() {
  MOZ_ASSERT(IsInRenderThread());
  return mRenderers.size();
}

void RenderThread::BeginRecordingForWindow(wr::WindowId aWindowId,
                                           const TimeStamp& aRecordingStart,
                                           wr::PipelineId aRootPipelineId) {
  MOZ_ASSERT(IsInRenderThread());
  RendererOGL* renderer = GetRenderer(aWindowId);
  MOZ_ASSERT(renderer);

  renderer->BeginRecording(aRecordingStart, aRootPipelineId);
}

void RenderThread::WriteCollectedFramesForWindow(wr::WindowId aWindowId) {
  MOZ_ASSERT(IsInRenderThread());

  RendererOGL* renderer = GetRenderer(aWindowId);
  MOZ_ASSERT(renderer);
  renderer->WriteCollectedFrames();
}

Maybe<layers::CollectedFrames> RenderThread::GetCollectedFramesForWindow(
    wr::WindowId aWindowId) {
  MOZ_ASSERT(IsInRenderThread());

  RendererOGL* renderer = GetRenderer(aWindowId);
  MOZ_ASSERT(renderer);
  return renderer->GetCollectedFrames();
}

void RenderThread::HandleFrameOneDoc(wr::WindowId aWindowId, bool aRender) {
  if (mHasShutdown) {
    return;
  }

  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod<wr::WindowId, bool>(
        "wr::RenderThread::HandleFrameOneDoc", this,
        &RenderThread::HandleFrameOneDoc, aWindowId, aRender));
    return;
  }

  if (IsDestroyed(aWindowId)) {
    return;
  }

  if (mHandlingDeviceReset) {
    return;
  }

  bool render = false;
  PendingFrameInfo frame;
  {  // scope lock
    auto windows = mWindowInfos.Lock();
    auto it = windows->find(AsUint64(aWindowId));
    if (it == windows->end()) {
      MOZ_ASSERT(false);
      return;
    }

    WindowInfo* info = it->second;
    PendingFrameInfo& frameInfo = info->mPendingFrames.front();
    frameInfo.mFrameNeedsRender |= aRender;
    render = frameInfo.mFrameNeedsRender;

    frame = frameInfo;
  }

  // It is for ensuring that PrepareForUse() is called before
  // RenderTextureHost::Lock().
  HandleRenderTextureOps();

  UpdateAndRender(aWindowId, frame.mStartId, frame.mStartTime, render,
                  /* aReadbackSize */ Nothing(),
                  /* aReadbackFormat */ Nothing(),
                  /* aReadbackBuffer */ Nothing());

  {  // scope lock
    auto windows = mWindowInfos.Lock();
    auto it = windows->find(AsUint64(aWindowId));
    if (it == windows->end()) {
      MOZ_ASSERT(false);
      return;
    }
    WindowInfo* info = it->second;
    info->mPendingFrames.pop();
  }

  // The start time is from WebRenderBridgeParent::CompositeToTarget. From that
  // point until now (when the frame is finally pushed to the screen) is
  // equivalent to the COMPOSITE_TIME metric in the non-WR codepath.
  mozilla::Telemetry::AccumulateTimeDelta(mozilla::Telemetry::COMPOSITE_TIME,
                                          frame.mStartTime);
}

void RenderThread::SetClearColor(wr::WindowId aWindowId, wr::ColorF aColor) {
  if (mHasShutdown) {
    return;
  }

  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod<wr::WindowId, wr::ColorF>(
        "wr::RenderThread::SetClearColor", this, &RenderThread::SetClearColor,
        aWindowId, aColor));
    return;
  }

  if (IsDestroyed(aWindowId)) {
    return;
  }

  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());
  if (it != mRenderers.end()) {
    wr_renderer_set_clear_color(it->second->GetRenderer(), aColor);
  }
}

void RenderThread::SetProfilerUI(wr::WindowId aWindowId, const nsCString& aUI) {
  if (mHasShutdown) {
    return;
  }

  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod<wr::WindowId, nsCString>(
        "wr::RenderThread::SetProfilerUI", this, &RenderThread::SetProfilerUI,
        aWindowId, aUI));
    return;
  }

  auto it = mRenderers.find(aWindowId);
  if (it != mRenderers.end()) {
    it->second->SetProfilerUI(aUI);
  }
}

void RenderThread::RunEvent(wr::WindowId aWindowId,
                            UniquePtr<RendererEvent> aEvent) {
  if (!IsInRenderThread()) {
    Loop()->PostTask(
        NewRunnableMethod<wr::WindowId, UniquePtr<RendererEvent>&&>(
            "wr::RenderThread::RunEvent", this, &RenderThread::RunEvent,
            aWindowId, std::move(aEvent)));
    return;
  }

  aEvent->Run(*this, aWindowId);
  aEvent = nullptr;
}

static void NotifyDidRender(layers::CompositorBridgeParent* aBridge,
                            const RefPtr<const WebRenderPipelineInfo>& aInfo,
                            VsyncId aCompositeStartId,
                            TimeStamp aCompositeStart, TimeStamp aRenderStart,
                            TimeStamp aEnd, bool aRender,
                            RendererStats aStats) {
  if (aRender && aBridge->GetWrBridge()) {
    // We call this here to mimic the behavior in LayerManagerComposite, as to
    // not change what Talos measures. That is, we do not record an empty frame
    // as a frame.
    aBridge->GetWrBridge()->RecordFrame();
  }

  aBridge->NotifyDidRender(aCompositeStartId, aCompositeStart, aRenderStart,
                           aEnd, &aStats);

  for (const auto& epoch : aInfo->Raw().epochs) {
    aBridge->NotifyPipelineRendered(epoch.pipeline_id, epoch.epoch,
                                    aCompositeStartId, aCompositeStart,
                                    aRenderStart, aEnd, &aStats);
  }

  if (aBridge->GetWrBridge()) {
    aBridge->GetWrBridge()->CompositeIfNeeded();
  }
}

static void NotifyDidStartRender(layers::CompositorBridgeParent* aBridge) {
  // Starting a render will change mIsRendering, and potentially
  // change whether we can allow the bridge to intiate another frame.
  if (aBridge->GetWrBridge()) {
    aBridge->GetWrBridge()->CompositeIfNeeded();
  }
}

void RenderThread::UpdateAndRender(
    wr::WindowId aWindowId, const VsyncId& aStartId,
    const TimeStamp& aStartTime, bool aRender,
    const Maybe<gfx::IntSize>& aReadbackSize,
    const Maybe<wr::ImageFormat>& aReadbackFormat,
    const Maybe<Range<uint8_t>>& aReadbackBuffer, bool* aNeedsYFlip) {
  AUTO_PROFILER_TRACING_MARKER("Paint", "Composite", GRAPHICS);
  AUTO_PROFILER_LABEL("RenderThread::UpdateAndRender", GRAPHICS);
  MOZ_ASSERT(IsInRenderThread());
  MOZ_ASSERT(aRender || aReadbackBuffer.isNothing());

  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());
  if (it == mRenderers.end()) {
    return;
  }

  TimeStamp start = TimeStamp::Now();

  auto& renderer = it->second;

  if (renderer->IsPaused()) {
    aRender = false;
  }

  layers::CompositorThread()->Dispatch(
      NewRunnableFunction("NotifyDidStartRenderRunnable", &NotifyDidStartRender,
                          renderer->GetCompositorBridge()));

  wr::RenderedFrameId latestFrameId;
  RendererStats stats = {0};
  if (aRender) {
    latestFrameId = renderer->UpdateAndRender(
        aReadbackSize, aReadbackFormat, aReadbackBuffer, aNeedsYFlip, &stats);
  } else {
    renderer->Update();
  }
  // Check graphics reset status even when rendering is skipped.
  renderer->CheckGraphicsResetStatus("PostUpdate", /* aForce */ false);

  TimeStamp end = TimeStamp::Now();
  RefPtr<const WebRenderPipelineInfo> info = renderer->FlushPipelineInfo();

  layers::CompositorThread()->Dispatch(
      NewRunnableFunction("NotifyDidRenderRunnable", &NotifyDidRender,
                          renderer->GetCompositorBridge(), info, aStartId,
                          aStartTime, start, end, aRender, stats));

  if (latestFrameId.IsValid()) {
    renderer->MaybeRecordFrame(info);
  }

  ipc::FileDescriptor fenceFd;

  if (latestFrameId.IsValid()) {
    fenceFd = renderer->GetAndResetReleaseFence();

    // Wait for GPU after posting NotifyDidRender, since the wait is not
    // necessary for the NotifyDidRender.
    // The wait is necessary for Textures recycling of AsyncImagePipelineManager
    // and for avoiding GPU queue is filled with too much tasks.
    // WaitForGPU's implementation is different for each platform.
    renderer->WaitForGPU();
  } else {
    // Update frame id for NotifyPipelinesUpdated() when rendering does not
    // happen, either because rendering was not requested or the frame was
    // canceled. Rendering can sometimes be canceled if UpdateAndRender is
    // called when the window is not yet ready (not mapped or 0 size).
    latestFrameId = renderer->UpdateFrameId();
  }

  RenderedFrameId lastCompletedFrameId = renderer->GetLastCompletedFrameId();

  RefPtr<layers::AsyncImagePipelineManager> pipelineMgr =
      renderer->GetCompositorBridge()->GetAsyncImagePipelineManager();
  // pipelineMgr should always be non-null here because it is only nulled out
  // after the WebRenderAPI instance for the CompositorBridgeParent is
  // destroyed, and that destruction blocks until the renderer thread has
  // removed the relevant renderer. And after that happens we should never reach
  // this code at all; it would bail out at the mRenderers.find check above.
  MOZ_ASSERT(pipelineMgr);
  pipelineMgr->NotifyPipelinesUpdated(info, latestFrameId, lastCompletedFrameId,
                                      std::move(fenceFd));
}

void RenderThread::Pause(wr::WindowId aWindowId) {
  MOZ_ASSERT(IsInRenderThread());

  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());
  if (it == mRenderers.end()) {
    return;
  }
  auto& renderer = it->second;
  renderer->Pause();
}

bool RenderThread::Resume(wr::WindowId aWindowId) {
  MOZ_ASSERT(IsInRenderThread());

  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());
  if (it == mRenderers.end()) {
    return false;
  }
  auto& renderer = it->second;
  return renderer->Resume();
}

bool RenderThread::TooManyPendingFrames(wr::WindowId aWindowId) {
  const int64_t maxFrameCount = 1;

  // Too many pending frames if pending frames exit more than maxFrameCount
  // or if RenderBackend is still processing a frame.

  auto windows = mWindowInfos.Lock();
  auto it = windows->find(AsUint64(aWindowId));
  if (it == windows->end()) {
    MOZ_ASSERT(false);
    return true;
  }
  WindowInfo* info = it->second;

  if (info->PendingCount() > maxFrameCount) {
    return true;
  }
  // If there is no ongoing frame build, we accept a new frame.
  return info->mPendingFrameBuild > 0;
}

bool RenderThread::IsDestroyed(wr::WindowId aWindowId) {
  auto windows = mWindowInfos.Lock();
  auto it = windows->find(AsUint64(aWindowId));
  if (it == windows->end()) {
    return true;
  }

  return it->second->mIsDestroyed;
}

void RenderThread::SetDestroyed(wr::WindowId aWindowId) {
  auto windows = mWindowInfos.Lock();
  auto it = windows->find(AsUint64(aWindowId));
  if (it == windows->end()) {
    MOZ_ASSERT(false);
    return;
  }
  it->second->mIsDestroyed = true;
}

void RenderThread::IncPendingFrameCount(wr::WindowId aWindowId,
                                        const VsyncId& aStartId,
                                        const TimeStamp& aStartTime) {
  auto windows = mWindowInfos.Lock();
  auto it = windows->find(AsUint64(aWindowId));
  if (it == windows->end()) {
    MOZ_ASSERT(false);
    return;
  }
  it->second->mPendingFrameBuild++;
  it->second->mPendingFrames.push(
      PendingFrameInfo{aStartTime, aStartId, false});
}

void RenderThread::DecPendingFrameBuildCount(wr::WindowId aWindowId) {
  auto windows = mWindowInfos.Lock();
  auto it = windows->find(AsUint64(aWindowId));
  if (it == windows->end()) {
    MOZ_ASSERT(false);
    return;
  }
  WindowInfo* info = it->second;
  MOZ_RELEASE_ASSERT(info->mPendingFrameBuild >= 1);
  info->mPendingFrameBuild--;
}

void RenderThread::RegisterExternalImage(
    uint64_t aExternalImageId, already_AddRefed<RenderTextureHost> aTexture) {
  MutexAutoLock lock(mRenderTextureMapLock);

  if (mHasShutdown) {
    return;
  }
  MOZ_ASSERT(mRenderTextures.find(aExternalImageId) == mRenderTextures.end());
  RefPtr<RenderTextureHost> texture = aTexture;
  if (texture->SyncObjectNeeded()) {
    mSyncObjectNeededRenderTextures.emplace(aExternalImageId, texture);
  }
  mRenderTextures.emplace(aExternalImageId, texture);
}

void RenderThread::UnregisterExternalImage(uint64_t aExternalImageId) {
  MutexAutoLock lock(mRenderTextureMapLock);
  if (mHasShutdown) {
    return;
  }
  auto it = mRenderTextures.find(aExternalImageId);
  if (it == mRenderTextures.end()) {
    return;
  }

  auto& texture = it->second;
  if (texture->SyncObjectNeeded()) {
    MOZ_RELEASE_ASSERT(
        mSyncObjectNeededRenderTextures.erase(aExternalImageId) == 1);
  }

  if (!IsInRenderThread()) {
    // The RenderTextureHost should be released in render thread. So, post the
    // deletion task here.
    // The shmem and raw buffer are owned by compositor ipc channel. It's
    // possible that RenderTextureHost is still exist after the shmem/raw buffer
    // deletion. Then the buffer in RenderTextureHost becomes invalid. It's fine
    // for this situation. Gecko will only release the buffer if WR doesn't need
    // it. So, no one will access the invalid buffer in RenderTextureHost.
    RefPtr<RenderTextureHost> texture = it->second;
    mRenderTextures.erase(it);
    mRenderTexturesDeferred.emplace_back(std::move(texture));
    Loop()->PostTask(NewRunnableMethod(
        "RenderThread::DeferredRenderTextureHostDestroy", this,
        &RenderThread::DeferredRenderTextureHostDestroy));
  } else {
    mRenderTextures.erase(it);
  }
}

void RenderThread::PrepareForUse(uint64_t aExternalImageId) {
  AddRenderTextureOp(RenderTextureOp::PrepareForUse, aExternalImageId);
}

void RenderThread::NotifyNotUsed(uint64_t aExternalImageId) {
  AddRenderTextureOp(RenderTextureOp::NotifyNotUsed, aExternalImageId);
}

void RenderThread::NotifyForUse(uint64_t aExternalImageId) {
  AddRenderTextureOp(RenderTextureOp::NotifyForUse, aExternalImageId);
}

void RenderThread::AddRenderTextureOp(RenderTextureOp aOp,
                                      uint64_t aExternalImageId) {
  MOZ_ASSERT(!IsInRenderThread());

  MutexAutoLock lock(mRenderTextureMapLock);

  auto it = mRenderTextures.find(aExternalImageId);
  MOZ_ASSERT(it != mRenderTextures.end());
  if (it == mRenderTextures.end()) {
    return;
  }

  RefPtr<RenderTextureHost> texture = it->second;
  mRenderTextureOps.emplace_back(aOp, std::move(texture));
  Loop()->PostTask(NewRunnableMethod("RenderThread::HandleRenderTextureOps",
                                     this,
                                     &RenderThread::HandleRenderTextureOps));
}

void RenderThread::HandleRenderTextureOps() {
  MOZ_ASSERT(IsInRenderThread());

  std::list<std::pair<RenderTextureOp, RefPtr<RenderTextureHost>>>
      renderTextureOps;
  {
    MutexAutoLock lock(mRenderTextureMapLock);
    mRenderTextureOps.swap(renderTextureOps);
  }

  for (auto& it : renderTextureOps) {
    switch (it.first) {
      case RenderTextureOp::PrepareForUse:
        it.second->PrepareForUse();
        break;
      case RenderTextureOp::NotifyForUse:
        it.second->NotifyForUse();
        break;
      case RenderTextureOp::NotifyNotUsed:
        it.second->NotifyNotUsed();
        break;
    }
  }
}

void RenderThread::UnregisterExternalImageDuringShutdown(
    uint64_t aExternalImageId) {
  MOZ_ASSERT(IsInRenderThread());
  MutexAutoLock lock(mRenderTextureMapLock);
  MOZ_ASSERT(mHasShutdown);
  MOZ_ASSERT(mRenderTextures.find(aExternalImageId) != mRenderTextures.end());
  mRenderTextures.erase(aExternalImageId);
}

bool RenderThread::SyncObjectNeeded() {
  MOZ_ASSERT(IsInRenderThread());
  MutexAutoLock lock(mRenderTextureMapLock);
  return !mSyncObjectNeededRenderTextures.empty();
}

void RenderThread::DeferredRenderTextureHostDestroy() {
  MutexAutoLock lock(mRenderTextureMapLock);
  mRenderTexturesDeferred.clear();
}

RenderTextureHost* RenderThread::GetRenderTexture(
    wr::ExternalImageId aExternalImageId) {
  MOZ_ASSERT(IsInRenderThread());

  MutexAutoLock lock(mRenderTextureMapLock);
  auto it = mRenderTextures.find(AsUint64(aExternalImageId));
  MOZ_ASSERT(it != mRenderTextures.end());
  if (it == mRenderTextures.end()) {
    return nullptr;
  }
  return it->second;
}

void RenderThread::InitDeviceTask() {
  MOZ_ASSERT(IsInRenderThread());
  MOZ_ASSERT(!mSingletonGL);

  if (gfx::gfxVars::UseSoftwareWebRender()) {
    // Ensure we don't instantiate any shared GL context when SW-WR is used.
    return;
  }

  nsAutoCString err;
  CreateSingletonGL(err);
  if (gfx::gfxVars::UseWebRenderProgramBinaryDisk()) {
    mProgramCache = MakeUnique<WebRenderProgramCache>(ThreadPool().Raw());
  }
  // Query the shared GL context to force the
  // lazy initialization to happen now.
  SingletonGL();
}

#ifndef XP_WIN
static DeviceResetReason GLenumToResetReason(GLenum aReason) {
  switch (aReason) {
    case LOCAL_GL_NO_ERROR:
      return DeviceResetReason::FORCED_RESET;
    case LOCAL_GL_INNOCENT_CONTEXT_RESET_ARB:
      return DeviceResetReason::DRIVER_ERROR;
    case LOCAL_GL_PURGED_CONTEXT_RESET_NV:
      return DeviceResetReason::NVIDIA_VIDEO;
    case LOCAL_GL_GUILTY_CONTEXT_RESET_ARB:
      return DeviceResetReason::RESET;
    case LOCAL_GL_UNKNOWN_CONTEXT_RESET_ARB:
      return DeviceResetReason::UNKNOWN;
    case LOCAL_GL_OUT_OF_MEMORY:
      return DeviceResetReason::OUT_OF_MEMORY;
    default:
      return DeviceResetReason::OTHER;
  }
}
#endif

void RenderThread::HandleDeviceReset(const char* aWhere, GLenum aReason) {
  MOZ_ASSERT(IsInRenderThread());

  if (mHandlingDeviceReset) {
    return;
  }

#ifndef XP_WIN
  // On Windows, see DeviceManagerDx::MaybeResetAndReacquireDevices.
  gfx::GPUProcessManager::RecordDeviceReset(GLenumToResetReason(aReason));
#endif

  {
    MutexAutoLock lock(mRenderTextureMapLock);
    mRenderTexturesDeferred.clear();
    for (const auto& entry : mRenderTextures) {
      entry.second->ClearCachedResources();
    }
  }

  mHandlingDeviceReset = aReason != LOCAL_GL_NO_ERROR;
  if (mHandlingDeviceReset) {
    // All RenderCompositors will be destroyed by the GPUProcessManager in
    // either OnRemoteProcessDeviceReset via the GPUChild, or
    // OnInProcessDeviceReset here directly.
    gfxCriticalNote << "GFX: RenderThread detected a device reset in "
                    << aWhere;
    if (XRE_IsGPUProcess()) {
      gfx::GPUParent::GetSingleton()->NotifyDeviceReset();
    } else {
#ifndef XP_WIN
      // FIXME(aosmond): Do we need to do this on Windows? nsWindow::OnPaint
      // seems to do its own detection for the parent process.
      bool guilty = aReason == LOCAL_GL_GUILTY_CONTEXT_RESET_ARB;
      NS_DispatchToMainThread(NS_NewRunnableFunction(
          "gfx::GPUProcessManager::OnInProcessDeviceReset", [guilty]() -> void {
            gfx::GPUProcessManager::Get()->OnInProcessDeviceReset(guilty);
          }));
#endif
    }
  }
}

bool RenderThread::IsHandlingDeviceReset() {
  MOZ_ASSERT(IsInRenderThread());
  return mHandlingDeviceReset;
}

void RenderThread::SimulateDeviceReset() {
  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod("RenderThread::SimulateDeviceReset",
                                       this,
                                       &RenderThread::SimulateDeviceReset));
  } else {
    // When this function is called GPUProcessManager::SimulateDeviceReset()
    // already triggers destroying all CompositorSessions before re-creating
    // them.
    HandleDeviceReset("SimulateDeviceReset", LOCAL_GL_NO_ERROR);
  }
}

static void DoNotifyWebRenderError(WebRenderError aError) {
  layers::CompositorManagerParent::NotifyWebRenderError(aError);
}

void RenderThread::NotifyWebRenderError(WebRenderError aError) {
  MOZ_ASSERT(IsInRenderThread());

  layers::CompositorThread()->Dispatch(NewRunnableFunction(
      "DoNotifyWebRenderErrorRunnable", &DoNotifyWebRenderError, aError));
}

void RenderThread::HandleWebRenderError(WebRenderError aError) {
  if (mHandlingWebRenderError) {
    return;
  }

  NotifyWebRenderError(aError);

  {
    MutexAutoLock lock(mRenderTextureMapLock);
    mRenderTexturesDeferred.clear();
    for (const auto& entry : mRenderTextures) {
      entry.second->ClearCachedResources();
    }
  }
  mHandlingWebRenderError = true;
  // WebRender is going to be disabled by
  // GPUProcessManager::NotifyWebRenderError()
}

bool RenderThread::IsHandlingWebRenderError() {
  MOZ_ASSERT(IsInRenderThread());
  return mHandlingWebRenderError;
}

gl::GLContext* RenderThread::SingletonGL() {
  nsAutoCString err;
  auto* gl = SingletonGL(err);
  if (!err.IsEmpty()) {
    gfxCriticalNote << err.get();
  }
  return gl;
}

void RenderThread::CreateSingletonGL(nsACString& aError) {
  mSingletonGL = CreateGLContext(aError);
  mSingletonGLIsForHardwareWebRender = !gfx::gfxVars::UseSoftwareWebRender();
}

gl::GLContext* RenderThread::SingletonGL(nsACString& aError) {
  MOZ_ASSERT(IsInRenderThread());
  if (!mSingletonGL) {
    CreateSingletonGL(aError);
    mShaders = nullptr;
  }
  if (mSingletonGL && !mShaders) {
    mShaders = MakeUnique<WebRenderShaders>(mSingletonGL, mProgramCache.get());
  }

  return mSingletonGL.get();
}

gl::GLContext* RenderThread::SingletonGLForCompositorOGL() {
  MOZ_RELEASE_ASSERT(gfx::gfxVars::UseSoftwareWebRender());

  if (mSingletonGLIsForHardwareWebRender) {
    // Clear singleton GL, since GLContext is for hardware WebRender.
    ClearSingletonGL();
  }
  return SingletonGL();
}

void RenderThread::ClearSingletonGL() {
  MOZ_ASSERT(IsInRenderThread());
  if (mSurfacePool) {
    mSurfacePool->DestroyGLResourcesForContext(mSingletonGL);
  }
  if (mProgramsForCompositorOGL) {
    mProgramsForCompositorOGL->Clear();
    mProgramsForCompositorOGL = nullptr;
  }
  mShaders = nullptr;
  mSingletonGL = nullptr;
}

RefPtr<layers::ShaderProgramOGLsHolder>
RenderThread::GetProgramsForCompositorOGL() {
  if (!mSingletonGL) {
    return nullptr;
  }

  if (!mProgramsForCompositorOGL) {
    mProgramsForCompositorOGL =
        MakeAndAddRef<layers::ShaderProgramOGLsHolder>(mSingletonGL);
  }
  return mProgramsForCompositorOGL;
}

RefPtr<layers::SurfacePool> RenderThread::SharedSurfacePool() {
#ifdef XP_MACOSX
  if (!mSurfacePool) {
    size_t poolSizeLimit =
        StaticPrefs::gfx_webrender_compositor_surface_pool_size_AtStartup();
    mSurfacePool = layers::SurfacePool::Create(poolSizeLimit);
  }
#endif
  return mSurfacePool;
}

void RenderThread::ClearSharedSurfacePool() { mSurfacePool = nullptr; }

static void GLAPIENTRY DebugMessageCallback(GLenum aSource, GLenum aType,
                                            GLuint aId, GLenum aSeverity,
                                            GLsizei aLength,
                                            const GLchar* aMessage,
                                            const GLvoid* aUserParam) {
  constexpr const char* kContextLost = "Context has been lost.";

  if (StaticPrefs::gfx_webrender_gl_debug_message_critical_note_AtStartup() &&
      aSeverity == LOCAL_GL_DEBUG_SEVERITY_HIGH) {
    auto message = std::string(aMessage, aLength);
    // When content lost happned, error messages are flooded by its message.
    if (message != kContextLost) {
      gfxCriticalNote << message;
    } else {
      gfxCriticalNoteOnce << message;
    }
  }

  if (StaticPrefs::gfx_webrender_gl_debug_message_print_AtStartup()) {
    gl::GLContext* gl = (gl::GLContext*)aUserParam;
    gl->DebugCallback(aSource, aType, aId, aSeverity, aLength, aMessage);
  }
}

// static
void RenderThread::MaybeEnableGLDebugMessage(gl::GLContext* aGLContext) {
  if (!aGLContext) {
    return;
  }

  bool enableDebugMessage =
      StaticPrefs::gfx_webrender_gl_debug_message_critical_note_AtStartup() ||
      StaticPrefs::gfx_webrender_gl_debug_message_print_AtStartup();

  if (enableDebugMessage &&
      aGLContext->IsExtensionSupported(gl::GLContext::KHR_debug)) {
    aGLContext->fEnable(LOCAL_GL_DEBUG_OUTPUT);
    aGLContext->fDisable(LOCAL_GL_DEBUG_OUTPUT_SYNCHRONOUS);
    aGLContext->fDebugMessageCallback(&DebugMessageCallback, (void*)aGLContext);
    aGLContext->fDebugMessageControl(LOCAL_GL_DONT_CARE, LOCAL_GL_DONT_CARE,
                                     LOCAL_GL_DONT_CARE, 0, nullptr, true);
  }
}

WebRenderShaders::WebRenderShaders(gl::GLContext* gl,
                                   WebRenderProgramCache* programCache) {
  mGL = gl;
  mShaders =
      wr_shaders_new(gl, programCache ? programCache->Raw() : nullptr,
                     StaticPrefs::gfx_webrender_precache_shaders_AtStartup());
}

WebRenderShaders::~WebRenderShaders() {
  wr_shaders_delete(mShaders, mGL.get());
}

WebRenderThreadPool::WebRenderThreadPool(bool low_priority) {
  mThreadPool = wr_thread_pool_new(low_priority);
}

WebRenderThreadPool::~WebRenderThreadPool() { Release(); }

void WebRenderThreadPool::Release() {
  if (mThreadPool) {
    wr_thread_pool_delete(mThreadPool);
    mThreadPool = nullptr;
  }
}

WebRenderProgramCache::WebRenderProgramCache(wr::WrThreadPool* aThreadPool) {
  MOZ_ASSERT(aThreadPool);

  nsAutoString path;
  if (gfx::gfxVars::UseWebRenderProgramBinaryDisk()) {
    path.Append(gfx::gfxVars::ProfDirectory());
  }
  mProgramCache = wr_program_cache_new(&path, aThreadPool);
  if (gfx::gfxVars::UseWebRenderProgramBinaryDisk()) {
    wr_try_load_startup_shaders_from_disk(mProgramCache);
  }
}

WebRenderProgramCache::~WebRenderProgramCache() {
  wr_program_cache_delete(mProgramCache);
}

}  // namespace mozilla::wr

#ifdef XP_WIN
static already_AddRefed<gl::GLContext> CreateGLContextANGLE(
    nsACString& aError) {
  const RefPtr<ID3D11Device> d3d11Device =
      gfx::DeviceManagerDx::Get()->GetCompositorDevice();
  if (!d3d11Device) {
    aError.Assign("RcANGLE(no compositor device for EGLDisplay)"_ns);
    return nullptr;
  }

  nsCString failureId;
  const auto lib = gl::DefaultEglLibrary(&failureId);
  if (!lib) {
    aError.Assign(
        nsPrintfCString("RcANGLE(load EGL lib failed: %s)", failureId.get()));
    return nullptr;
  }

  const auto egl = lib->CreateDisplay(d3d11Device.get());
  if (!egl) {
    aError.Assign(nsPrintfCString("RcANGLE(create EGLDisplay failed: %s)",
                                  failureId.get()));
    return nullptr;
  }

  gl::CreateContextFlags flags = gl::CreateContextFlags::PREFER_ES3;

  if (StaticPrefs::gfx_webrender_prefer_robustness_AtStartup()) {
    flags |= gl::CreateContextFlags::PREFER_ROBUSTNESS;
  }

  if (egl->IsExtensionSupported(
          gl::EGLExtension::MOZ_create_context_provoking_vertex_dont_care)) {
    flags |= gl::CreateContextFlags::PROVOKING_VERTEX_DONT_CARE;
  }

  // Create GLContext with dummy EGLSurface, the EGLSurface is not used.
  // Instread we override it with EGLSurface of SwapChain's back buffer.

  const auto dummySize = mozilla::gfx::IntSize(16, 16);
  auto gl = gl::GLContextEGL::CreateEGLPBufferOffscreenContext(
      egl, {flags}, dummySize, &failureId);
  if (!gl || !gl->IsANGLE()) {
    aError.Assign(nsPrintfCString("RcANGLE(create GL context failed: %x, %s)",
                                  gl.get(), failureId.get()));
    return nullptr;
  }

  if (!gl->MakeCurrent()) {
    aError.Assign(
        nsPrintfCString("RcANGLE(make current GL context failed: %x, %x)",
                        gl.get(), gl->mEgl->mLib->fGetError()));
    return nullptr;
  }

  return gl.forget();
}
#endif

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WAYLAND)
static already_AddRefed<gl::GLContext> CreateGLContextEGL() {
  // Create GLContext with dummy EGLSurface.
  bool forHardwareWebRender = true;
  // SW-WR uses CompositorOGL in native compositor.
  if (gfx::gfxVars::UseSoftwareWebRender()) {
    forHardwareWebRender = false;
  }
  RefPtr<gl::GLContext> gl =
      gl::GLContextProviderEGL::CreateForCompositorWidget(
          nullptr, forHardwareWebRender, /* aForceAccelerated */ true);
  if (!gl || !gl->MakeCurrent()) {
    gfxCriticalNote << "Failed GL context creation for hardware WebRender: "
                    << forHardwareWebRender;
    return nullptr;
  }
  return gl.forget();
}
#endif

#ifdef XP_MACOSX
static already_AddRefed<gl::GLContext> CreateGLContextCGL() {
  nsCString failureUnused;
  return gl::GLContextProvider::CreateHeadless(
      {gl::CreateContextFlags::ALLOW_OFFLINE_RENDERER |
       gl::CreateContextFlags::FORCE_ENABLE_HARDWARE},
      &failureUnused);
}
#endif

static already_AddRefed<gl::GLContext> CreateGLContext(nsACString& aError) {
  RefPtr<gl::GLContext> gl;

#ifdef XP_WIN
  if (gfx::gfxVars::UseWebRenderANGLE()) {
    gl = CreateGLContextANGLE(aError);
  }
#elif defined(MOZ_WIDGET_ANDROID)
  gl = CreateGLContextEGL();
#elif defined(MOZ_WAYLAND)
  if (gfx::gfxVars::UseEGL()) {
    gl = CreateGLContextEGL();
  }
#elif XP_MACOSX
  gl = CreateGLContextCGL();
#endif

  wr::RenderThread::MaybeEnableGLDebugMessage(gl);

  return gl.forget();
}

extern "C" {

void wr_notifier_wake_up(mozilla::wr::WrWindowId aWindowId,
                         bool aCompositeNeeded) {
  mozilla::wr::RenderThread::Get()->IncPendingFrameCount(aWindowId, VsyncId(),
                                                         TimeStamp::Now());
  mozilla::wr::RenderThread::Get()->DecPendingFrameBuildCount(aWindowId);
  mozilla::wr::RenderThread::Get()->HandleFrameOneDoc(aWindowId,
                                                      aCompositeNeeded);
}

void wr_notifier_new_frame_ready(mozilla::wr::WrWindowId aWindowId) {
  mozilla::wr::RenderThread::Get()->DecPendingFrameBuildCount(aWindowId);
  mozilla::wr::RenderThread::Get()->HandleFrameOneDoc(aWindowId,
                                                      /* aRender */ true);
}

void wr_notifier_nop_frame_done(mozilla::wr::WrWindowId aWindowId) {
  mozilla::wr::RenderThread::Get()->DecPendingFrameBuildCount(aWindowId);
  mozilla::wr::RenderThread::Get()->HandleFrameOneDoc(aWindowId,
                                                      /* aRender */ false);
}

void wr_notifier_external_event(mozilla::wr::WrWindowId aWindowId,
                                size_t aRawEvent) {
  mozilla::UniquePtr<mozilla::wr::RendererEvent> evt(
      reinterpret_cast<mozilla::wr::RendererEvent*>(aRawEvent));
  mozilla::wr::RenderThread::Get()->RunEvent(mozilla::wr::WindowId(aWindowId),
                                             std::move(evt));
}

static void NotifyScheduleRender(mozilla::wr::WrWindowId aWindowId) {
  RefPtr<mozilla::layers::CompositorBridgeParent> cbp = mozilla::layers::
      CompositorBridgeParent::GetCompositorBridgeParentFromWindowId(aWindowId);
  if (cbp) {
    cbp->ScheduleComposition();
  }
}

void wr_schedule_render(mozilla::wr::WrWindowId aWindowId) {
  layers::CompositorThread()->Dispatch(NewRunnableFunction(
      "NotifyScheduleRender", &NotifyScheduleRender, aWindowId));
}

static void NotifyDidSceneBuild(
    mozilla::wr::WrWindowId aWindowId,
    const RefPtr<const wr::WebRenderPipelineInfo>& aInfo) {
  RefPtr<mozilla::layers::CompositorBridgeParent> cbp = mozilla::layers::
      CompositorBridgeParent::GetCompositorBridgeParentFromWindowId(aWindowId);
  if (cbp) {
    cbp->NotifyDidSceneBuild(aInfo);
  }
}

void wr_finished_scene_build(mozilla::wr::WrWindowId aWindowId,
                             mozilla::wr::WrPipelineInfo* aPipelineInfo) {
  RefPtr<wr::WebRenderPipelineInfo> info = new wr::WebRenderPipelineInfo();
  info->Raw() = std::move(*aPipelineInfo);
  layers::CompositorThread()->Dispatch(NewRunnableFunction(
      "NotifyDidSceneBuild", &NotifyDidSceneBuild, aWindowId, info));
}

}  // extern C
