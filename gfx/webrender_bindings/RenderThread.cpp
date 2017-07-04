/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
  , mPendingFrameCountMapLock("RenderThread.mPendingFrameCountMapLock")
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

  MutexAutoLock lock(mPendingFrameCountMapLock);
  mPendingFrameCounts.Put(AsUint64(aWindowId), 0);
}

void
RenderThread::RemoveRenderer(wr::WindowId aWindowId)
{
  MOZ_ASSERT(IsInRenderThread());

  if (mHasShutdown) {
    return;
  }

  mRenderers.erase(aWindowId);

  MutexAutoLock lock(mPendingFrameCountMapLock);
  mPendingFrameCounts.Remove(AsUint64(aWindowId));
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

  UpdateAndRender(aWindowId);
  DecPendingFrameCount(aWindowId);
}

void
RenderThread::NewScrollFrameReady(wr::WindowId aWindowId, bool aCompositeNeeded)
{
  if (mHasShutdown) {
    return;
  }

  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod<wr::WindowId, bool>(
      "wr::RenderThread::NewScrollFrameReady",
      this,
      &RenderThread::NewScrollFrameReady,
      aWindowId,
      aCompositeNeeded));
    return;
  }

  UpdateAndRender(aWindowId);
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
                WrRenderedEpochs* aEpochs,
                TimeStamp aStart,
                TimeStamp aEnd)
{
  WrPipelineId pipeline;
  WrEpoch epoch;
  while (wr_rendered_epochs_next(aEpochs, &pipeline, &epoch)) {
    aBridge->NotifyDidCompositeToPipeline(pipeline, epoch, aStart, aEnd);
  }
  wr_rendered_epochs_delete(aEpochs);
}

void
RenderThread::UpdateAndRender(wr::WindowId aWindowId)
{
  AutoProfilerTracing tracing("Paint", "Composite");
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

uint32_t
RenderThread::GetPendingFrameCount(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mPendingFrameCountMapLock);
  uint32_t count = 0;
  MOZ_ASSERT(mPendingFrameCounts.Get(AsUint64(aWindowId), &count));
  mPendingFrameCounts.Get(AsUint64(aWindowId), &count);
  return count;
}

void
RenderThread::IncPendingFrameCount(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mPendingFrameCountMapLock);
  // Get the old count.
  uint32_t oldCount = 0;
  if (!mPendingFrameCounts.Get(AsUint64(aWindowId), &oldCount)) {
    MOZ_ASSERT(false);
    return;
  }
  // Update pending frame count.
  mPendingFrameCounts.Put(AsUint64(aWindowId), oldCount + 1);
}

void
RenderThread::DecPendingFrameCount(wr::WindowId aWindowId)
{
  MutexAutoLock lock(mPendingFrameCountMapLock);
  // Get the old count.
  uint32_t oldCount = 0;
  if (!mPendingFrameCounts.Get(AsUint64(aWindowId), &oldCount)) {
    MOZ_ASSERT(false);
    return;
  }
  MOZ_ASSERT(oldCount > 0);
  if (oldCount <= 0) {
    return;
  }
  // Update pending frame count.
  mPendingFrameCounts.Put(AsUint64(aWindowId), oldCount - 1);
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
RenderThread::GetRenderTexture(WrExternalImageId aExternalImageId)
{
  MOZ_ASSERT(IsInRenderThread());

  MutexAutoLock lock(mRenderTextureMapLock);
  MOZ_ASSERT(mRenderTextures.GetWeak(aExternalImageId.mHandle));
  return mRenderTextures.GetWeak(aExternalImageId.mHandle);
}

WebRenderThreadPool::WebRenderThreadPool()
{
  mThreadPool = wr_thread_pool_new();
}

WebRenderThreadPool::~WebRenderThreadPool()
{
  wr_thread_pool_delete(mThreadPool);
}

} // namespace wr
} // namespace mozilla

extern "C" {

void wr_notifier_new_frame_ready(WrWindowId aWindowId)
{
  mozilla::wr::RenderThread::Get()->IncPendingFrameCount(aWindowId);
  mozilla::wr::RenderThread::Get()->NewFrameReady(mozilla::wr::WindowId(aWindowId));
}

void wr_notifier_new_scroll_frame_ready(WrWindowId aWindowId, bool aCompositeNeeded)
{
  mozilla::wr::RenderThread::Get()->NewScrollFrameReady(mozilla::wr::WindowId(aWindowId),
                                                        aCompositeNeeded);
}

void wr_notifier_external_event(WrWindowId aWindowId, size_t aRawEvent)
{
  mozilla::UniquePtr<mozilla::wr::RendererEvent> evt(
    reinterpret_cast<mozilla::wr::RendererEvent*>(aRawEvent));
  mozilla::wr::RenderThread::Get()->RunEvent(mozilla::wr::WindowId(aWindowId),
                                             mozilla::Move(evt));
}

} // extern C
