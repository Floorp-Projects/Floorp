/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/task.h"
#include "GeckoProfiler.h"
#include "RenderThread.h"
#include "nsThreadUtils.h"
#include "mtransport/runnable_utils.h"
#include "mozilla/layers/AsyncImagePipelineManager.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/SharedSurfacesParent.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/webrender/RendererOGL.h"
#include "mozilla/webrender/RenderTextureHost.h"
#include "mozilla/widget/CompositorWidget.h"

#ifdef XP_WIN
#include "mozilla/widget/WinCompositorWindowThread.h"
#endif

namespace mozilla {
namespace wr {

static StaticRefPtr<RenderThread> sRenderThread;

RenderThread::RenderThread(base::Thread* aThread)
  : mThread(aThread)
  , mFrameCountMapLock("RenderThread.mFrameCountMapLock")
  , mRenderTextureMapLock("RenderThread.mRenderTextureMapLock")
  , mHasShutdown(false)
  , mHandlingDeviceReset(false)
{

}

RenderThread::~RenderThread()
{
  MOZ_ASSERT(mRenderTexturesDeferred.empty());
  delete mThread;
}

// static
RenderThread*
RenderThread::Get()
{
  return sRenderThread;
}

// static
void
RenderThread::Start()
{
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

  if (XRE_IsGPUProcess() &&
      gfx::gfxVars::UseWebRenderProgramBinary()) {
    MOZ_ASSERT(gfx::gfxVars::UseWebRender());
    // Initialize program cache if necessary
    RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<RenderThread>(sRenderThread.get()),
      &RenderThread::ProgramCacheTask);
    sRenderThread->Loop()->PostTask(runnable.forget());
  }
}

// static
void
RenderThread::ShutDown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sRenderThread);

  {
    MutexAutoLock lock(sRenderThread->mRenderTextureMapLock);
    sRenderThread->mHasShutdown = true;
  }

  layers::SynchronousTask task("RenderThread");
  RefPtr<Runnable> runnable = WrapRunnable(
    RefPtr<RenderThread>(sRenderThread.get()),
    &RenderThread::ShutDownTask,
    &task);
  sRenderThread->Loop()->PostTask(runnable.forget());
  task.Wait();

  sRenderThread = nullptr;
#ifdef XP_WIN
  widget::WinCompositorWindowThread::ShutDown();
#endif
}

extern void ClearAllBlobImageResources();

void
RenderThread::ShutDownTask(layers::SynchronousTask* aTask)
{
  layers::AutoCompleteTask complete(aTask);
  MOZ_ASSERT(IsInRenderThread());

  // Releasing on the render thread will allow us to avoid dispatching to remove
  // remaining textures from the texture map.
  layers::SharedSurfacesParent::Shutdown();

  ClearAllBlobImageResources();
}

// static
MessageLoop*
RenderThread::Loop()
{
  return sRenderThread ? sRenderThread->mThread->message_loop() : nullptr;
}

// static
bool
RenderThread::IsInRenderThread()
{
  return sRenderThread && sRenderThread->mThread->thread_id() == PlatformThread::CurrentId();
}

void
RenderThread::AddRenderer(wr::WindowId aWindowId, UniquePtr<RendererOGL> aRenderer)
{
  MOZ_ASSERT(IsInRenderThread());

  if (mHasShutdown) {
    return;
  }

  mRenderers[aWindowId] = std::move(aRenderer);

  MutexAutoLock lock(mFrameCountMapLock);
  mWindowInfos.emplace(AsUint64(aWindowId), new WindowInfo());
}

void
RenderThread::RemoveRenderer(wr::WindowId aWindowId)
{
  MOZ_ASSERT(IsInRenderThread());

  if (mHasShutdown) {
    return;
  }

  mRenderers.erase(aWindowId);

  if (mRenderers.size() == 0 && mHandlingDeviceReset) {
    mHandlingDeviceReset = false;
  }

  MutexAutoLock lock(mFrameCountMapLock);
  auto it = mWindowInfos.find(AsUint64(aWindowId));
  MOZ_ASSERT(it != mWindowInfos.end());
  WindowInfo* toDelete = it->second;
  mWindowInfos.erase(it);
  delete toDelete;
}

RendererOGL*
RenderThread::GetRenderer(wr::WindowId aWindowId)
{
  MOZ_ASSERT(IsInRenderThread());

  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());

  if (it == mRenderers.end()) {
    return nullptr;
  }

  return it->second.get();
}

size_t
RenderThread::RendererCount()
{
  MOZ_ASSERT(IsInRenderThread());
  return mRenderers.size();
}

void
RenderThread::NewFrameReady(wr::WindowId aWindowId)
{
  if (mHasShutdown) {
    return;
  }

  if (!IsInRenderThread()) {
    Loop()->PostTask(
      NewRunnableMethod<wr::WindowId>("wr::RenderThread::NewFrameReady",
                                      this,
                                      &RenderThread::NewFrameReady,
                                      aWindowId));
    return;
  }

  if (IsDestroyed(aWindowId)) {
    return;
  }

  if (mHandlingDeviceReset) {
    return;
  }

  TimeStamp startTime;

  { // scope lock
    MutexAutoLock lock(mFrameCountMapLock);
    auto it = mWindowInfos.find(AsUint64(aWindowId));
    MOZ_ASSERT(it != mWindowInfos.end());
    WindowInfo* info = it->second;
    MOZ_ASSERT(info->mPendingCount > 0);
    startTime = info->mStartTimes.front();
  }

  UpdateAndRender(aWindowId, startTime);
  FrameRenderingComplete(aWindowId);
}

void
RenderThread::WakeUp(wr::WindowId aWindowId)
{
  if (mHasShutdown) {
    return;
  }

  if (!IsInRenderThread()) {
    Loop()->PostTask(
      NewRunnableMethod<wr::WindowId>("wr::RenderThread::WakeUp",
                                      this,
                                      &RenderThread::WakeUp,
                                      aWindowId));
    return;
  }

  if (IsDestroyed(aWindowId)) {
    return;
  }

  if (mHandlingDeviceReset) {
    return;
  }

  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());
  if (it != mRenderers.end()) {
    it->second->Update();
  }
}

void
RenderThread::RunEvent(wr::WindowId aWindowId, UniquePtr<RendererEvent> aEvent)
{
  if (!IsInRenderThread()) {
    Loop()->PostTask(
      NewRunnableMethod<wr::WindowId, UniquePtr<RendererEvent>&&>(
        "wr::RenderThread::RunEvent",
        this,
        &RenderThread::RunEvent,
        aWindowId,
        std::move(aEvent)));
    return;
  }

  aEvent->Run(*this, aWindowId);
  aEvent = nullptr;
}

static void
NotifyDidRender(layers::CompositorBridgeParent* aBridge,
                wr::WrPipelineInfo aInfo,
                TimeStamp aStart,
                TimeStamp aEnd)
{
  for (uintptr_t i = 0; i < aInfo.epochs.length; i++) {
    aBridge->NotifyPipelineRendered(
        aInfo.epochs.data[i].pipeline_id,
        aInfo.epochs.data[i].epoch,
        aStart,
        aEnd);
  }

  wr_pipeline_info_delete(aInfo);
}

void
RenderThread::UpdateAndRender(wr::WindowId aWindowId,
                              const TimeStamp& aStartTime,
                              bool aReadback)
{
  AUTO_PROFILER_TRACING("Paint", "Composite");
  MOZ_ASSERT(IsInRenderThread());

  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());
  if (it == mRenderers.end()) {
    return;
  }

  auto& renderer = it->second;

  bool ret = renderer->UpdateAndRender(aReadback);
  if (!ret) {
    // Render did not happen, do not call NotifyDidRender.
    return;
  }

  TimeStamp end = TimeStamp::Now();

  auto info = renderer->FlushPipelineInfo();
  RefPtr<layers::AsyncImagePipelineManager> pipelineMgr =
      renderer->GetCompositorBridge()->GetAsyncImagePipelineManager();
  // pipelineMgr should always be non-null here because it is only nulled out
  // after the WebRenderAPI instance for the CompositorBridgeParent is
  // destroyed, and that destruction blocks until the renderer thread has
  // removed the relevant renderer. And after that happens we should never reach
  // this code at all; it would bail out at the mRenderers.find check above.
  MOZ_ASSERT(pipelineMgr);
  pipelineMgr->NotifyPipelinesUpdated(info);

  layers::CompositorThreadHolder::Loop()->PostTask(NewRunnableFunction(
    "NotifyDidRenderRunnable",
    &NotifyDidRender,
    renderer->GetCompositorBridge(),
    info,
    aStartTime, end
  ));
}

void
RenderThread::Pause(wr::WindowId aWindowId)
{
  MOZ_ASSERT(IsInRenderThread());

  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());
  if (it == mRenderers.end()) {
    return;
  }
  auto& renderer = it->second;
  renderer->Pause();
}

bool
RenderThread::Resume(wr::WindowId aWindowId)
{
  MOZ_ASSERT(IsInRenderThread());

  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());
  if (it == mRenderers.end()) {
    return false;
  }
  auto& renderer = it->second;
  return renderer->Resume();
}

bool
RenderThread::TooManyPendingFrames(wr::WindowId aWindowId)
{
  const int64_t maxFrameCount = 1;

  // Too many pending frames if pending frames exit more than maxFrameCount
  // or if RenderBackend is still processing a frame.

  MutexAutoLock lock(mFrameCountMapLock);
  auto it = mWindowInfos.find(AsUint64(aWindowId));
  if (it == mWindowInfos.end()) {
    MOZ_ASSERT(false);
    return true;
  }
  WindowInfo* info = it->second;

  if (info->mPendingCount > maxFrameCount) {
    return true;
  }
  MOZ_ASSERT(info->mPendingCount >= info->mRenderingCount);
  return info->mPendingCount > info->mRenderingCount;
}

bool
RenderThread::IsDestroyed(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mFrameCountMapLock);
  auto it = mWindowInfos.find(AsUint64(aWindowId));
  if (it == mWindowInfos.end()) {
    return true;
  }

  return it->second->mIsDestroyed;
}

void
RenderThread::SetDestroyed(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mFrameCountMapLock);
  auto it = mWindowInfos.find(AsUint64(aWindowId));
  if (it == mWindowInfos.end()) {
    MOZ_ASSERT(false);
    return;
  }
  it->second->mIsDestroyed = true;
}

void
RenderThread::IncPendingFrameCount(wr::WindowId aWindowId, const TimeStamp& aStartTime)
{
  MutexAutoLock lock(mFrameCountMapLock);
  auto it = mWindowInfos.find(AsUint64(aWindowId));
  if (it == mWindowInfos.end()) {
    MOZ_ASSERT(false);
    return;
  }
  it->second->mPendingCount++;
  it->second->mStartTimes.push(aStartTime);
}

void
RenderThread::DecPendingFrameCount(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mFrameCountMapLock);
  auto it = mWindowInfos.find(AsUint64(aWindowId));
  if (it == mWindowInfos.end()) {
    MOZ_ASSERT(false);
    return;
  }
  WindowInfo* info = it->second;
  MOZ_ASSERT(info->mPendingCount > 0);
  if (info->mPendingCount <= 0) {
    return;
  }
  info->mPendingCount--;
  // This function gets called for "nop frames" where nothing was rendered or
  // composited. But we count this time because the non-WR codepath equivalent
  // in CompositorBridgeParent::ComposeToTarget also counts such frames. And
  // anyway this should be relatively infrequent so it shouldn't skew the
  // numbers much.
  mozilla::Telemetry::AccumulateTimeDelta(mozilla::Telemetry::COMPOSITE_TIME,
                                          info->mStartTimes.front());
  info->mStartTimes.pop();
}

void
RenderThread::IncRenderingFrameCount(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mFrameCountMapLock);
  auto it = mWindowInfos.find(AsUint64(aWindowId));
  if (it == mWindowInfos.end()) {
    MOZ_ASSERT(false);
    return;
  }
  it->second->mRenderingCount++;
}

void
RenderThread::FrameRenderingComplete(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mFrameCountMapLock);
  auto it = mWindowInfos.find(AsUint64(aWindowId));
  if (it == mWindowInfos.end()) {
    MOZ_ASSERT(false);
    return;
  }
  WindowInfo* info = it->second;
  MOZ_ASSERT(info->mPendingCount > 0);
  MOZ_ASSERT(info->mRenderingCount > 0);
  if (info->mPendingCount <= 0) {
    return;
  }
  info->mPendingCount--;
  info->mRenderingCount--;
  // The start time is from WebRenderBridgeParent::CompositeToTarget. From that
  // point until now (when the frame is finally pushed to the screen) is
  // equivalent to the COMPOSITE_TIME metric in the non-WR codepath.
  mozilla::Telemetry::AccumulateTimeDelta(mozilla::Telemetry::COMPOSITE_TIME,
                                          info->mStartTimes.front());
  info->mStartTimes.pop();
}

void
RenderThread::RegisterExternalImage(uint64_t aExternalImageId, already_AddRefed<RenderTextureHost> aTexture)
{
  MutexAutoLock lock(mRenderTextureMapLock);

  if (mHasShutdown) {
    return;
  }
  MOZ_ASSERT(mRenderTextures.find(aExternalImageId) == mRenderTextures.end());
  mRenderTextures.emplace(aExternalImageId, std::move(aTexture));
}

void
RenderThread::UnregisterExternalImage(uint64_t aExternalImageId)
{
  MutexAutoLock lock(mRenderTextureMapLock);
  if (mHasShutdown) {
    return;
  }
  auto it = mRenderTextures.find(aExternalImageId);
  MOZ_ASSERT(it != mRenderTextures.end());
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
      "RenderThread::DeferredRenderTextureHostDestroy",
      this, &RenderThread::DeferredRenderTextureHostDestroy
    ));
  } else {
    mRenderTextures.erase(it);
  }
}

void
RenderThread::UnregisterExternalImageDuringShutdown(uint64_t aExternalImageId)
{
  MOZ_ASSERT(IsInRenderThread());
  MutexAutoLock lock(mRenderTextureMapLock);
  MOZ_ASSERT(mHasShutdown);
  MOZ_ASSERT(mRenderTextures.find(aExternalImageId) != mRenderTextures.end());
  mRenderTextures.erase(aExternalImageId);
}

void
RenderThread::DeferredRenderTextureHostDestroy()
{
  MutexAutoLock lock(mRenderTextureMapLock);
  mRenderTexturesDeferred.clear();
}

RenderTextureHost*
RenderThread::GetRenderTexture(wr::WrExternalImageId aExternalImageId)
{
  MOZ_ASSERT(IsInRenderThread());

  MutexAutoLock lock(mRenderTextureMapLock);
  auto it = mRenderTextures.find(aExternalImageId.mHandle);
  MOZ_ASSERT(it != mRenderTextures.end());
  if (it == mRenderTextures.end()) {
    return nullptr;
  }
  return it->second;
}

void
RenderThread::ProgramCacheTask()
{
  ProgramCache();
}

void
RenderThread::HandleDeviceReset(const char* aWhere, bool aNotify)
{
  MOZ_ASSERT(IsInRenderThread());

  if (mHandlingDeviceReset) {
    return;
  }

  if (aNotify) {
    gfxCriticalNote << "GFX: RenderThread detected a device reset in " << aWhere;
    if (XRE_IsGPUProcess()) {
      gfx::GPUParent::GetSingleton()->NotifyDeviceReset();
    }
  }

  {
    MutexAutoLock lock(mRenderTextureMapLock);
    mRenderTexturesDeferred.clear();
    for (const auto& entry : mRenderTextures) {
      entry.second->ClearCachedResources();
    }
  }

  mHandlingDeviceReset = true;
  // All RenderCompositors will be destroyed by GPUChild::RecvNotifyDeviceReset()
}

bool
RenderThread::IsHandlingDeviceReset()
{
  MOZ_ASSERT(IsInRenderThread());
  return mHandlingDeviceReset;
}

void
RenderThread::SimulateDeviceReset()
{
  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod(
      "RenderThread::SimulateDeviceReset",
      this, &RenderThread::SimulateDeviceReset
      ));
  } else {
    // When this function is called GPUProcessManager::SimulateDeviceReset() already
    // triggers destroying all CompositorSessions before re-creating them.
    HandleDeviceReset("SimulateDeviceReset", /* aNotify */ false);
  }
}

WebRenderProgramCache*
RenderThread::ProgramCache()
{
  MOZ_ASSERT(IsInRenderThread());

  if (!mProgramCache) {
    mProgramCache = MakeUnique<WebRenderProgramCache>(ThreadPool().Raw());
  }
  return mProgramCache.get();
}

WebRenderThreadPool::WebRenderThreadPool()
{
  mThreadPool = wr_thread_pool_new();
}

WebRenderThreadPool::~WebRenderThreadPool()
{
  wr_thread_pool_delete(mThreadPool);
}

WebRenderProgramCache::WebRenderProgramCache(wr::WrThreadPool* aThreadPool)
{
  MOZ_ASSERT(aThreadPool);

  nsAutoString path;
  if (gfxVars::UseWebRenderProgramBinaryDisk()) {
    path.Append(gfx::gfxVars::ProfDirectory());
  }
  mProgramCache = wr_program_cache_new(&path, aThreadPool);
  wr_try_load_shader_from_disk(mProgramCache);
}

WebRenderProgramCache::~WebRenderProgramCache()
{
  wr_program_cache_delete(mProgramCache);
}

} // namespace wr
} // namespace mozilla

extern "C" {

static void NewFrameReady(mozilla::wr::WrWindowId aWindowId)
{
  mozilla::wr::RenderThread::Get()->IncRenderingFrameCount(aWindowId);
  mozilla::wr::RenderThread::Get()->NewFrameReady(aWindowId);
}

void wr_notifier_wake_up(mozilla::wr::WrWindowId aWindowId)
{
  mozilla::wr::RenderThread::Get()->WakeUp(aWindowId);
}

void wr_notifier_new_frame_ready(mozilla::wr::WrWindowId aWindowId)
{
  NewFrameReady(aWindowId);
}

void wr_notifier_nop_frame_done(mozilla::wr::WrWindowId aWindowId)
{
  mozilla::wr::RenderThread::Get()->DecPendingFrameCount(aWindowId);
}

void wr_notifier_external_event(mozilla::wr::WrWindowId aWindowId, size_t aRawEvent)
{
  mozilla::UniquePtr<mozilla::wr::RendererEvent> evt(
    reinterpret_cast<mozilla::wr::RendererEvent*>(aRawEvent));
  mozilla::wr::RenderThread::Get()->RunEvent(mozilla::wr::WindowId(aWindowId),
                                             std::move(evt));
}

void wr_schedule_render(mozilla::wr::WrWindowId aWindowId)
{
  RefPtr<mozilla::layers::CompositorBridgeParent> cbp =
      mozilla::layers::CompositorBridgeParent::GetCompositorBridgeParentFromWindowId(aWindowId);
  if (cbp) {
    cbp->ScheduleRenderOnCompositorThread();
  }
}

} // extern C
