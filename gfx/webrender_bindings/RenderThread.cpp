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
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/webrender/RendererOGL.h"
#include "mozilla/webrender/RenderTextureHost.h"
#include "mozilla/widget/CompositorWidget.h"

namespace mozilla {
namespace wr {

static StaticRefPtr<RenderThread> sRenderThread;

RenderThread::RenderThread(base::Thread* aThread)
  : mThread(aThread)
  , mFrameCountMapLock("RenderThread.mFrameCountMapLock")
  , mRenderTextureMapLock("RenderThread.mRenderTextureMapLock")
  , mHasShutdown(false)
{

}

RenderThread::~RenderThread()
{
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
}

void
RenderThread::ShutDownTask(layers::SynchronousTask* aTask)
{
  layers::AutoCompleteTask complete(aTask);
  MOZ_ASSERT(IsInRenderThread());
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

  mRenderers[aWindowId] = Move(aRenderer);

  MutexAutoLock lock(mFrameCountMapLock);
  mWindowInfos.Put(AsUint64(aWindowId), WindowInfo());
}

void
RenderThread::RemoveRenderer(wr::WindowId aWindowId)
{
  MOZ_ASSERT(IsInRenderThread());

  if (mHasShutdown) {
    return;
  }

  mRenderers.erase(aWindowId);

  MutexAutoLock lock(mFrameCountMapLock);
  mWindowInfos.Remove(AsUint64(aWindowId));
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

  UpdateAndRender(aWindowId);
  DecPendingFrameCount(aWindowId);
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
        Move(aEvent)));
    return;
  }

  aEvent->Run(*this, aWindowId);
  aEvent = nullptr;
}

static void
NotifyDidRender(layers::CompositorBridgeParentBase* aBridge,
                wr::WrRenderedEpochs* aEpochs,
                TimeStamp aStart,
                TimeStamp aEnd)
{
  wr::WrPipelineId pipeline;
  wr::WrEpoch epoch;
  while (wr_rendered_epochs_next(aEpochs, &pipeline, &epoch)) {
    aBridge->NotifyDidCompositeToPipeline(pipeline, epoch, aStart, aEnd);
  }
  wr_rendered_epochs_delete(aEpochs);
}

void
RenderThread::UpdateAndRender(wr::WindowId aWindowId)
{
  AUTO_PROFILER_TRACING("Paint", "Composite");
  MOZ_ASSERT(IsInRenderThread());

  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());
  if (it == mRenderers.end()) {
    return;
  }

  auto& renderer = it->second;
  renderer->Update();

  TimeStamp start = TimeStamp::Now();

  bool ret = renderer->Render();
  if (!ret) {
    // Render did not happen, do not call NotifyDidRender.
    return;
  }

  TimeStamp end = TimeStamp::Now();

  auto epochs = renderer->FlushRenderedEpochs();
  layers::CompositorThreadHolder::Loop()->PostTask(NewRunnableFunction(
    "NotifyDidRenderRunnable",
    &NotifyDidRender,
    renderer->GetCompositorBridge(),
    epochs,
    start, end
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
  WindowInfo info;
  if (!mWindowInfos.Get(AsUint64(aWindowId), &info)) {
    MOZ_ASSERT(false);
    return true;
  }

  if (info.mPendingCount > maxFrameCount) {
    return true;
  }
  MOZ_ASSERT(info.mPendingCount >= info.mRenderingCount);
  return info.mPendingCount > info.mRenderingCount;
}

bool
RenderThread::IsDestroyed(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mFrameCountMapLock);
  WindowInfo info;
  if (!mWindowInfos.Get(AsUint64(aWindowId), &info)) {
    return true;
  }

  return info.mIsDestroyed;
}

void
RenderThread::SetDestroyed(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mFrameCountMapLock);
  WindowInfo info;
  if (!mWindowInfos.Get(AsUint64(aWindowId), &info)) {
    MOZ_ASSERT(false);
    return;
  }
  info.mIsDestroyed = true;
  mWindowInfos.Put(AsUint64(aWindowId), info);
}

void
RenderThread::IncPendingFrameCount(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mFrameCountMapLock);
  // Get the old count.
  WindowInfo info;
  if (!mWindowInfos.Get(AsUint64(aWindowId), &info)) {
    MOZ_ASSERT(false);
    return;
  }
  // Update pending frame count.
  info.mPendingCount = info.mPendingCount + 1;
  mWindowInfos.Put(AsUint64(aWindowId), info);
}

void
RenderThread::IncRenderingFrameCount(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mFrameCountMapLock);
  // Get the old count.
  WindowInfo info;
  if (!mWindowInfos.Get(AsUint64(aWindowId), &info)) {
    MOZ_ASSERT(false);
    return;
  }
  // Update rendering frame count.
  info.mRenderingCount = info.mRenderingCount + 1;
  mWindowInfos.Put(AsUint64(aWindowId), info);
}

void
RenderThread::DecPendingFrameCount(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mFrameCountMapLock);
  // Get the old count.
  WindowInfo info;
  if (!mWindowInfos.Get(AsUint64(aWindowId), &info)) {
    MOZ_ASSERT(false);
    return;
  }
  MOZ_ASSERT(info.mPendingCount > 0);
  MOZ_ASSERT(info.mRenderingCount > 0);
  if (info.mPendingCount <= 0) {
    return;
  }
  // Update frame counts.
  info.mPendingCount = info.mPendingCount - 1;
  info.mRenderingCount = info.mRenderingCount - 1;
  mWindowInfos.Put(AsUint64(aWindowId), info);
}

void
RenderThread::RegisterExternalImage(uint64_t aExternalImageId, already_AddRefed<RenderTextureHost> aTexture)
{
  MutexAutoLock lock(mRenderTextureMapLock);

  if (mHasShutdown) {
    return;
  }
  MOZ_ASSERT(!mRenderTextures.GetWeak(aExternalImageId));
  mRenderTextures.Put(aExternalImageId, Move(aTexture));
}

void
RenderThread::UnregisterExternalImage(uint64_t aExternalImageId)
{
  MutexAutoLock lock(mRenderTextureMapLock);
  if (mHasShutdown) {
    return;
  }
  MOZ_ASSERT(mRenderTextures.GetWeak(aExternalImageId));
  if (!IsInRenderThread()) {
    // The RenderTextureHost should be released in render thread. So, post the
    // deletion task here.
    // The shmem and raw buffer are owned by compositor ipc channel. It's
    // possible that RenderTextureHost is still exist after the shmem/raw buffer
    // deletion. Then the buffer in RenderTextureHost becomes invalid. It's fine
    // for this situation. Gecko will only release the buffer if WR doesn't need
    // it. So, no one will access the invalid buffer in RenderTextureHost.
    RefPtr<RenderTextureHost> texture;
    mRenderTextures.Remove(aExternalImageId, getter_AddRefs(texture));
    Loop()->PostTask(NewRunnableMethod<RefPtr<RenderTextureHost>>(
      "RenderThread::DeferredRenderTextureHostDestroy",
      this, &RenderThread::DeferredRenderTextureHostDestroy, Move(texture)
    ));
  } else {
    mRenderTextures.Remove(aExternalImageId);
  }
}

void
RenderThread::DeferredRenderTextureHostDestroy(RefPtr<RenderTextureHost>)
{
  // Do nothing. Just decrease the ref-count of RenderTextureHost.
}

RenderTextureHost*
RenderThread::GetRenderTexture(wr::WrExternalImageId aExternalImageId)
{
  MOZ_ASSERT(IsInRenderThread());

  MutexAutoLock lock(mRenderTextureMapLock);
  MOZ_ASSERT(mRenderTextures.GetWeak(aExternalImageId.mHandle));
  return mRenderTextures.GetWeak(aExternalImageId.mHandle);
}

WebRenderProgramCache*
RenderThread::ProgramCache()
{
  MOZ_ASSERT(IsInRenderThread());

  if (!mProgramCache) {
    mProgramCache = MakeUnique<WebRenderProgramCache>();
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

WebRenderProgramCache::WebRenderProgramCache()
{
  mProgramCache = wr_program_cache_new();
}

WebRenderProgramCache::~WebRenderProgramCache()
{
  wr_program_cache_delete(mProgramCache);
}

} // namespace wr
} // namespace mozilla

extern "C" {

void wr_notifier_new_frame_ready(mozilla::wr::WrWindowId aWindowId)
{
  mozilla::wr::RenderThread::Get()->IncRenderingFrameCount(aWindowId);
  mozilla::wr::RenderThread::Get()->NewFrameReady(mozilla::wr::WindowId(aWindowId));
}

void wr_notifier_new_scroll_frame_ready(mozilla::wr::WrWindowId aWindowId, bool aCompositeNeeded)
{
  // It is not necessary to update rendering with new_scroll_frame_ready.
  // WebRenderBridgeParent::CompositeToTarget() is implemented to call
  // WebRenderAPI::GenerateFrame() if it is necessary to trigger UpdateAndRender().
  // See Bug 1377688.
}

void wr_notifier_external_event(mozilla::wr::WrWindowId aWindowId, size_t aRawEvent)
{
  mozilla::UniquePtr<mozilla::wr::RendererEvent> evt(
    reinterpret_cast<mozilla::wr::RendererEvent*>(aRawEvent));
  mozilla::wr::RenderThread::Get()->RunEvent(mozilla::wr::WindowId(aWindowId),
                                             mozilla::Move(evt));
}

} // extern C
