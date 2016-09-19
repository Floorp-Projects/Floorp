/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VsyncBridgeChild.h"
#include "VsyncIOThreadHolder.h"

namespace mozilla {
namespace gfx {

VsyncBridgeChild::VsyncBridgeChild(RefPtr<VsyncIOThreadHolder> aThread, const uint64_t& aProcessToken)
 : mThread(aThread),
   mLoop(nullptr),
   mProcessToken(aProcessToken)
{
}

VsyncBridgeChild::~VsyncBridgeChild()
{
}

/* static */ RefPtr<VsyncBridgeChild>
VsyncBridgeChild::Create(RefPtr<VsyncIOThreadHolder> aThread,
                         const uint64_t& aProcessToken,
                         Endpoint<PVsyncBridgeChild>&& aEndpoint)
{
  RefPtr<VsyncBridgeChild> child = new VsyncBridgeChild(aThread, aProcessToken);

  RefPtr<nsIRunnable> task = NewRunnableMethod<Endpoint<PVsyncBridgeChild>&&>(
    child, &VsyncBridgeChild::Open, Move(aEndpoint));
  aThread->GetThread()->Dispatch(task.forget(), nsIThread::DISPATCH_NORMAL);

  return child;
}

void
VsyncBridgeChild::Open(Endpoint<PVsyncBridgeChild>&& aEndpoint)
{
  if (!aEndpoint.Bind(this)) {
    // The GPU Process Manager might be gone if we receive ActorDestroy very
    // late in shutdown.
    if (GPUProcessManager* gpm = GPUProcessManager::Get())
      gpm->NotifyRemoteActorDestroyed(mProcessToken);
    return;
  }

  mLoop = MessageLoop::current();

  // Last reference is freed in DeallocPVsyncBridgeChild.
  AddRef();
}

class NotifyVsyncTask : public Runnable
{
public:
  NotifyVsyncTask(RefPtr<VsyncBridgeChild> aVsyncBridge,
                  TimeStamp aTimeStamp,
                  const uint64_t& aLayersId)
   : mVsyncBridge(aVsyncBridge),
     mTimeStamp(aTimeStamp),
     mLayersId(aLayersId)
  {}

  NS_IMETHOD Run() override {
    mVsyncBridge->NotifyVsyncImpl(mTimeStamp, mLayersId);
    return NS_OK;
  }

private:
  RefPtr<VsyncBridgeChild> mVsyncBridge;
  TimeStamp mTimeStamp;
  uint64_t mLayersId;
};

bool
VsyncBridgeChild::IsOnVsyncIOThread() const
{
  return MessageLoop::current() == mLoop;
}

void
VsyncBridgeChild::NotifyVsync(TimeStamp aTimeStamp, const uint64_t& aLayersId)
{
  // This should be on the Vsync thread (not the Vsync I/O thread).
  MOZ_ASSERT(!IsOnVsyncIOThread());

  RefPtr<NotifyVsyncTask> task = new NotifyVsyncTask(this, aTimeStamp, aLayersId);
  mLoop->PostTask(task.forget());
}

void
VsyncBridgeChild::NotifyVsyncImpl(TimeStamp aTimeStamp, const uint64_t& aLayersId)
{
  // This should be on the Vsync I/O thread.
  MOZ_ASSERT(IsOnVsyncIOThread());

  if (!mProcessToken) {
    return;
  }
  SendNotifyVsync(aTimeStamp, aLayersId);
}

void
VsyncBridgeChild::Close()
{
  if (!IsOnVsyncIOThread()) {
    mLoop->PostTask(NewRunnableMethod(this, &VsyncBridgeChild::Close));
    return;
  }

  // We clear mProcessToken when the channel is closed.
  if (!mProcessToken) {
    return;
  }
  PVsyncBridgeChild::Close();
  mProcessToken = 0;
}

void
VsyncBridgeChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mProcessToken) {
    GPUProcessManager::Get()->NotifyRemoteActorDestroyed(mProcessToken);
    mProcessToken = 0;
  }
}

void
VsyncBridgeChild::DeallocPVsyncBridgeChild()
{
  Release();
}

void
VsyncBridgeChild::ProcessingError(Result aCode, const char* aReason)
{
  MOZ_RELEASE_ASSERT(aCode == MsgDropped, "Processing error in VsyncBridgeChild");
}

} // namespace gfx
} // namespace mozilla
