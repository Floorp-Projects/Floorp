/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VsyncBridgeChild.h"
#include "VsyncIOThreadHolder.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/ipc/Endpoint.h"

namespace mozilla {
namespace gfx {

VsyncBridgeChild::VsyncBridgeChild(RefPtr<VsyncIOThreadHolder> aThread,
                                   const uint64_t& aProcessToken)
    : mThread(aThread), mProcessToken(aProcessToken) {}

VsyncBridgeChild::~VsyncBridgeChild() = default;

/* static */
RefPtr<VsyncBridgeChild> VsyncBridgeChild::Create(
    RefPtr<VsyncIOThreadHolder> aThread, const uint64_t& aProcessToken,
    Endpoint<PVsyncBridgeChild>&& aEndpoint) {
  RefPtr<VsyncBridgeChild> child = new VsyncBridgeChild(aThread, aProcessToken);

  RefPtr<nsIRunnable> task = NewRunnableMethod<Endpoint<PVsyncBridgeChild>&&>(
      "gfx::VsyncBridgeChild::Open", child, &VsyncBridgeChild::Open,
      std::move(aEndpoint));
  aThread->GetThread()->Dispatch(task.forget(), nsIThread::DISPATCH_NORMAL);

  return child;
}

void VsyncBridgeChild::Open(Endpoint<PVsyncBridgeChild>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    // The GPU Process Manager might be gone if we receive ActorDestroy very
    // late in shutdown.
    if (GPUProcessManager* gpm = GPUProcessManager::Get())
      gpm->NotifyRemoteActorDestroyed(mProcessToken);
    return;
  }

  // Last reference is freed in DeallocPVsyncBridgeChild.
  AddRef();
}

class NotifyVsyncTask : public Runnable {
 public:
  NotifyVsyncTask(RefPtr<VsyncBridgeChild> aVsyncBridge,
                  const VsyncEvent& aVsync, const layers::LayersId& aLayersId)
      : Runnable("gfx::NotifyVsyncTask"),
        mVsyncBridge(aVsyncBridge),
        mVsync(aVsync),
        mLayersId(aLayersId) {}

  NS_IMETHOD Run() override {
    mVsyncBridge->NotifyVsyncImpl(mVsync, mLayersId);
    return NS_OK;
  }

 private:
  RefPtr<VsyncBridgeChild> mVsyncBridge;
  VsyncEvent mVsync;
  layers::LayersId mLayersId;
};

bool VsyncBridgeChild::IsOnVsyncIOThread() const {
  return mThread->IsOnCurrentThread();
}

void VsyncBridgeChild::NotifyVsync(const VsyncEvent& aVsync,
                                   const layers::LayersId& aLayersId) {
  // This should be on the Vsync thread (not the Vsync I/O thread).
  MOZ_ASSERT(!IsOnVsyncIOThread());

  RefPtr<NotifyVsyncTask> task = new NotifyVsyncTask(this, aVsync, aLayersId);
  mThread->Dispatch(task.forget());
}

void VsyncBridgeChild::NotifyVsyncImpl(const VsyncEvent& aVsync,
                                       const layers::LayersId& aLayersId) {
  // This should be on the Vsync I/O thread.
  MOZ_ASSERT(IsOnVsyncIOThread());

  if (!mProcessToken) {
    return;
  }
  SendNotifyVsync(aVsync, aLayersId);
}

void VsyncBridgeChild::Close() {
  if (!IsOnVsyncIOThread()) {
    mThread->Dispatch(NewRunnableMethod("gfx::VsyncBridgeChild::Close", this,
                                        &VsyncBridgeChild::Close));
    return;
  }

  // We clear mProcessToken when the channel is closed.
  if (!mProcessToken) {
    return;
  }

  // Clear the process token so we don't notify the GPUProcessManager. It
  // already knows we're closed since it manually called Close, and in fact the
  // GPM could have already been destroyed during shutdown.
  mProcessToken = 0;

  // Close the underlying IPC channel.
  PVsyncBridgeChild::Close();
}

void VsyncBridgeChild::ActorDestroy(ActorDestroyReason aWhy) {
  if (mProcessToken) {
    GPUProcessManager::Get()->NotifyRemoteActorDestroyed(mProcessToken);
    mProcessToken = 0;
  }
}

void VsyncBridgeChild::ActorDealloc() { Release(); }

void VsyncBridgeChild::ProcessingError(Result aCode, const char* aReason) {
  MOZ_RELEASE_ASSERT(aCode == MsgDropped,
                     "Processing error in VsyncBridgeChild");
}

void VsyncBridgeChild::HandleFatalError(const char* aMsg) const {
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aMsg, OtherPid());
}

}  // namespace gfx
}  // namespace mozilla
