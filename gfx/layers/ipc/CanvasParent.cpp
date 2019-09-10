/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasParent.h"

#include "base/thread.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/SharedThreadPool.h"
#include "nsTHashtable.h"
#include "prsystem.h"

#if defined(XP_WIN)
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/layers/DeviceAttachmentsD3D11.h"
#endif

bool NS_IsInCanvasThread() {
  return mozilla::layers::CanvasParent::IsInCanvasThread();
}

namespace mozilla {
namespace layers {

class RingBufferReaderServices final
    : public CanvasEventRingBuffer::ReaderServices {
 public:
  explicit RingBufferReaderServices(RefPtr<CanvasParent> aCanvasParent)
      : mCanvasParent(std::move(aCanvasParent)) {}

  ~RingBufferReaderServices() final = default;

  bool WriterClosed() final {
    return mCanvasParent->GetIPCChannel()->Unsound_IsClosed();
  }

 private:
  RefPtr<CanvasParent> mCanvasParent;
};

static base::Thread* sCanvasThread = nullptr;
static StaticRefPtr<nsIThreadPool> sCanvasWorkers;
static bool sShuttingDown = false;

static MessageLoop* CanvasPlaybackLoop() {
  if (!sCanvasThread && !sShuttingDown) {
    MOZ_ASSERT(NS_IsInCompositorThread());
    base::Thread* canvasThread = new base::Thread("Canvas");
    if (canvasThread->Start()) {
      sCanvasThread = canvasThread;
    }
  }

  return sCanvasThread ? sCanvasThread->message_loop() : nullptr;
}

typedef nsTHashtable<nsRefPtrHashKey<CanvasParent>> CanvasParentSet;

static CanvasParentSet& CanvasParents() {
  static CanvasParentSet* sCanvasParents = new CanvasParentSet();
  return *sCanvasParents;
}

/* static */
already_AddRefed<CanvasParent> CanvasParent::Create(
    ipc::Endpoint<PCanvasParent>&& aEndpoint) {
  MOZ_ASSERT(NS_IsInCompositorThread());

  if (sShuttingDown) {
    return nullptr;
  }

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
  return (sCanvasWorkers && sCanvasWorkers->IsOnCurrentThread()) ||
         (sCanvasThread &&
          sCanvasThread->thread_id() == PlatformThread::CurrentId());
}

static already_AddRefed<nsIThreadPool> GetCanvasWorkers() {
  if (!sCanvasWorkers && !sShuttingDown) {
    // Given that the canvas workers are receiving instructions from content
    // processes, it probably doesn't make sense to have more than half the
    // number of processors doing canvas drawing. We set the lower limit to 2,
    // so that even on single processor systems, if there is more than one
    // window with canvas drawing, the OS can manage the load between them.
    uint32_t threadLimit = std::max(2, PR_GetNumberOfProcessors() / 2);
    sCanvasWorkers =
        SharedThreadPool::Get(NS_LITERAL_CSTRING("CanvasWorkers"), threadLimit);
  }

  return do_AddRef(sCanvasWorkers);
}

static void EnsureAllClosed() {
  // Close removes the CanvasParent from the set so take a copy first.
  CanvasParentSet canvasParents;
  for (auto iter = CanvasParents().Iter(); !iter.Done(); iter.Next()) {
    canvasParents.PutEntry(iter.Get()->GetKey());
  }

  for (auto iter = canvasParents.Iter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->Close();
    iter.Remove();
  }

  MOZ_DIAGNOSTIC_ASSERT(CanvasParents().IsEmpty(),
                        "Closing should have cleared all entries.");
}

/* static */ void CanvasParent::Shutdown() {
  sShuttingDown = true;

  if (sCanvasThread) {
    RefPtr<Runnable> runnable =
        NewRunnableFunction("CanvasParent::EnsureAllClosed", &EnsureAllClosed);
    sCanvasThread->message_loop()->PostTask(runnable.forget());
    sCanvasThread->Stop();
    delete sCanvasThread;
    sCanvasThread = nullptr;
  }

  if (sCanvasWorkers) {
    sCanvasWorkers->Shutdown();
    sCanvasWorkers = nullptr;
  }
}

CanvasParent::CanvasParent() : mTranslator(CanvasTranslator::Create()) {}

CanvasParent::~CanvasParent() {}

void CanvasParent::Bind(Endpoint<PCanvasParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    return;
  }

  CanvasParents().PutEntry(this);
}

mozilla::ipc::IPCResult CanvasParent::RecvInitTranslator(
    const TextureType& aTextureType,
    const ipc::SharedMemoryBasic::Handle& aReadHandle,
    const CrossProcessSemaphoreHandle& aReaderSem,
    const CrossProcessSemaphoreHandle& aWriterSem) {
  if (!mTranslator->Init(aTextureType, aReadHandle, aReaderSem, aWriterSem,
                         MakeUnique<RingBufferReaderServices>(this))) {
    return IPC_FAIL(this, "Failed to initialize CanvasTranslator.");
  }

  return RecvResumeTranslation();
}

ipc::IPCResult CanvasParent::RecvResumeTranslation() {
  MOZ_ASSERT(mTranslator);

  if (!mTranslator->IsValid()) {
    return IPC_FAIL(this, "Canvas Translation failed.");
  }

  PostStartTranslationTask(nsIThread::DISPATCH_NORMAL);

  return IPC_OK();
}

void CanvasParent::PostStartTranslationTask(uint32_t aDispatchFlags) {
  if (sShuttingDown) {
    return;
  }

  RefPtr<nsIThreadPool> canvasWorkers = GetCanvasWorkers();
  RefPtr<Runnable> runnable = NewRunnableMethod(
      "CanvasParent::StartTranslation", this, &CanvasParent::StartTranslation);
  mTranslating = true;
  canvasWorkers->Dispatch(runnable.forget(), aDispatchFlags);
}

void CanvasParent::StartTranslation() {
  if (!mTranslator->TranslateRecording() &&
      !GetIPCChannel()->Unsound_IsClosed()) {
    PostStartTranslationTask(nsIThread::DISPATCH_AT_END);
    return;
  }

  mTranslating = false;
}

UniquePtr<SurfaceDescriptor>
CanvasParent::LookupSurfaceDescriptorForClientDrawTarget(
    const uintptr_t aDrawTarget) {
  if (!mTranslator) {
    // We are shutting down.
    return nullptr;
  }

  return mTranslator->WaitForSurfaceDescriptor(
      reinterpret_cast<void*>(aDrawTarget));
}

void CanvasParent::ActorDestroy(ActorDestroyReason why) {
  while (mTranslating) {
  }

  if (mTranslator) {
    mTranslator = nullptr;
  }
}

void CanvasParent::ActorDealloc() {
  MOZ_DIAGNOSTIC_ASSERT(!mTranslating);
  MOZ_DIAGNOSTIC_ASSERT(!mTranslator);

  CanvasParents().RemoveEntry(this);
}

}  // namespace layers
}  // namespace mozilla
