/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderThread.h"
#include "nsThreadUtils.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/webrender/RendererOGL.h"
#include "mozilla/webrender/RenderTextureHost.h"
#include "mozilla/widget/CompositorWidget.h"
#include "base/task.h"

namespace mozilla {
namespace wr {

static StaticRefPtr<RenderThread> sRenderThread;

RenderThread::RenderThread(base::Thread* aThread)
  : mThread(aThread)
  , mPendingFrameCountMapLock("RenderThread.mPendingFrameCountMapLock")
  , mRenderTextureMapLock("RenderThread.mRenderTextureMapLock")
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

  // TODO(nical): sync with the render thread

  sRenderThread = nullptr;
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
  mRenderers[aWindowId] = Move(aRenderer);

  MutexAutoLock lock(mPendingFrameCountMapLock);
  mPendingFrameCounts.Put(AsUint64(aWindowId), 0);
}

void
RenderThread::RemoveRenderer(wr::WindowId aWindowId)
{
  MOZ_ASSERT(IsInRenderThread());
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
  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod<wr::WindowId>(
      this, &RenderThread::NewFrameReady, aWindowId
    ));
    return;
  }

  UpdateAndRender(aWindowId);
  DecPendingFrameCount(aWindowId);
}

void
RenderThread::NewScrollFrameReady(wr::WindowId aWindowId, bool aCompositeNeeded)
{
  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod<wr::WindowId, bool>(
      this, &RenderThread::NewScrollFrameReady, aWindowId, aCompositeNeeded
    ));
    return;
  }

  UpdateAndRender(aWindowId);
}

void
RenderThread::RunEvent(wr::WindowId aWindowId, UniquePtr<RendererEvent> aEvent)
{
  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod<wr::WindowId, UniquePtr<RendererEvent>&&>(
      this, &RenderThread::RunEvent,
      aWindowId, Move(aEvent)
    ));
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
  MOZ_ASSERT(IsInRenderThread());

  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());
  if (it == mRenderers.end()) {
    return;
  }

  auto& renderer = it->second;
  renderer->Update();

  TimeStamp start = TimeStamp::Now();

  renderer->Render();

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
RenderThread::RegisterExternalImage(uint64_t aExternalImageId, RenderTextureHost* aTexture)
{
  MutexAutoLock lock(mRenderTextureMapLock);
  MOZ_ASSERT(!mRenderTextures.Get(aExternalImageId));
  mRenderTextures.Put(aExternalImageId, aTexture);
}

void
RenderThread::UnregisterExternalImage(uint64_t aExternalImageId)
{
  MutexAutoLock lock(mRenderTextureMapLock);
  MOZ_ASSERT(mRenderTextures.Get(aExternalImageId).get());
  mRenderTextures.Remove(aExternalImageId);
}

RenderTextureHost*
RenderThread::GetRenderTexture(WrExternalImageId aExternalImageId)
{
  MutexAutoLock lock(mRenderTextureMapLock);
  MOZ_ASSERT(mRenderTextures.Get(aExternalImageId.mHandle).get());
  return mRenderTextures.Get(aExternalImageId.mHandle).get();
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
