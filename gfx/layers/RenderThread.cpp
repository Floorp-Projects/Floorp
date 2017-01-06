/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderThread.h"
#include "nsThreadUtils.h"
#include "mozilla/layers/RendererOGL.h"

namespace mozilla {
namespace layers {

static StaticRefPtr<RenderThread> sRenderThread;

RenderThread::RenderThread(base::Thread* aThread)
: mThread(aThread)
{

}

RenderThread::~RenderThread()
{
  delete mThread;
}

// statuc
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

  base::Thread* thread = new base::Thread("Compositor");

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
  mRenderers[aWindowId] = Move(aRenderer);
}

void
RenderThread::RemoveRenderer(wr::WindowId aWindowId)
{
  mRenderers.erase(aWindowId);
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
RenderThread::PipelineSizeChanged(wr::WindowId aWindowId, uint64_t aPipelineId, float aWidth, float aHeight)
{
  if (!IsInRenderThread()) {
    Loop()->PostTask(NewRunnableMethod<wr::WindowId, uint64_t, float, float>(
      this, &RenderThread::PipelineSizeChanged,
      aWindowId, aPipelineId, aWidth, aHeight
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
  }

  aEvent->Run(*this, aWindowId);
  aEvent = nullptr;
}


void
RenderThread::UpdateAndRender(wr::WindowId aWindowId)
{
  auto it = mRenderers.find(aWindowId);
  MOZ_ASSERT(it != mRenderers.end());
  if (it == mRenderers.end()) {
    return;
  }

  auto& renderer = it->second;

  auto transactionId = 0; // TODO!

  renderer->Update();
  renderer->Render(transactionId);
}

} // namespace
} // namespace

extern "C" {

void wr_notifier_new_frame_ready(uint64_t aWindowId)
{
  mozilla::layers::RenderThread::Get()->NewFrameReady(mozilla::wr::WindowId(aWindowId));
}

void wr_notifier_new_scroll_frame_ready(uint64_t aWindowId, bool aCompositeNeeded)
{
  mozilla::layers::RenderThread::Get()->NewScrollFrameReady(mozilla::wr::WindowId(aWindowId),
                                                            aCompositeNeeded);
}

void wr_notifier_pipeline_size_changed(uint64_t aWindowId,
                                       uint64_t aPipelineId,
                                       float aWidth,
                                       float aHeight)
{
  mozilla::layers::RenderThread::Get()->PipelineSizeChanged(mozilla::wr::WindowId(aWindowId),
                                                            aPipelineId, aWidth, aHeight);
}

void wr_notifier_external_event(uint64_t aWindowId, size_t aRawEvent)
{
  mozilla::UniquePtr<mozilla::layers::RendererEvent> evt(
    reinterpret_cast<mozilla::layers::RendererEvent*>(aRawEvent));
  mozilla::layers::RenderThread::Get()->RunEvent(mozilla::wr::WindowId(aWindowId),
                                                 mozilla::Move(evt));
}

} // extern C
