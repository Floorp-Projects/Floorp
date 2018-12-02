/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasParent.h"

#include "base/thread.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/layers/TextureClient.h"

#if defined(XP_WIN)
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/layers/DeviceAttachmentsD3D11.h"
#endif

bool NS_IsInCanvasThread() {
  return mozilla::layers::CanvasParent::IsInCanvasThread();
}

namespace mozilla {
namespace layers {

static base::Thread* sCanvasThread = nullptr;

static MessageLoop* CanvasPlaybackLoop() {
  if (!sCanvasThread) {
    MOZ_ASSERT(NS_IsInCompositorThread());
    base::Thread* canvasThread = new base::Thread("Canvas");
    if (canvasThread->Start()) {
      sCanvasThread = canvasThread;
    }
  }

  return sCanvasThread ? sCanvasThread->message_loop() : MessageLoop::current();
}

/* static */
already_AddRefed<CanvasParent> CanvasParent::Create(
    ipc::Endpoint<PCanvasParent>&& aEndpoint) {
  MOZ_ASSERT(NS_IsInCompositorThread());

  RefPtr<CanvasParent> canvasParent = new CanvasParent();
  if (CanvasPlaybackLoop()->IsAcceptingTasks()) {
    RefPtr<Runnable> runnable = NewRunnableMethod<Endpoint<PCanvasParent>&&>(
        "CanvasParent::Bind", canvasParent, &CanvasParent::Bind,
        std::move(aEndpoint));
    CanvasPlaybackLoop()->PostTask(runnable.forget());
  }
  return do_AddRef(canvasParent);
}

/* static */ bool CanvasParent::IsInCanvasThread() {
  return sCanvasThread &&
         sCanvasThread->thread_id() == PlatformThread::CurrentId();
}

/* static */ void CanvasParent::Shutdown() {
  if (sCanvasThread) {
    delete sCanvasThread;
    sCanvasThread = nullptr;
  }
}

CanvasParent::CanvasParent() {}

CanvasParent::~CanvasParent() {}

void CanvasParent::Bind(Endpoint<PCanvasParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    return;
  }

  mSelfRef = this;
}

mozilla::ipc::IPCResult CanvasParent::RecvCreateTranslator(
    const TextureType& aTextureType,
    const ipc::SharedMemoryBasic::Handle& aReadHandle,
    const CrossProcessSemaphoreHandle& aReaderSem,
    const CrossProcessSemaphoreHandle& aWriterSem) {
  mTranslator = CanvasTranslator::Create(aTextureType, aReadHandle, aReaderSem,
                                         aWriterSem);
  return RecvResumeTranslation();
}

ipc::IPCResult CanvasParent::RecvResumeTranslation() {
  MOZ_ASSERT(mTranslator);

  if (!mTranslator->IsValid()) {
    return IPC_FAIL(this, "Canvas Translation failed.");
  }

  PostStartTranslationTask();

  return IPC_OK();
}

void CanvasParent::PostStartTranslationTask() {
  if (MessageLoop::current()->IsAcceptingTasks()) {
    RefPtr<Runnable> runnable =
        NewRunnableMethod("CanvasParent::StartTranslation", this,
                          &CanvasParent::StartTranslation);
    MessageLoop::current()->PostTask(runnable.forget());
  }
}

void CanvasParent::StartTranslation() {
  if (!mTranslator->TranslateRecording()) {
    PostStartTranslationTask();
  }
}

UniquePtr<SurfaceDescriptor>
CanvasParent::LookupSurfaceDescriptorForClientDrawTarget(
    const uintptr_t aDrawTarget) {
  return mTranslator->WaitForSurfaceDescriptor(
      reinterpret_cast<void*>(aDrawTarget));
}

void CanvasParent::DeallocPCanvasParent() { mSelfRef = nullptr; }

}  // namespace layers
}  // namespace mozilla
