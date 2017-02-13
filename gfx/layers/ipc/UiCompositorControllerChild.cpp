/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UiCompositorControllerChild.h"
#include "UiCompositorControllerParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"
#include "mozilla/gfx/GPUProcessManager.h"

namespace mozilla {
namespace layers {
using namespace gfx;

static bool sInitialized = false;
static StaticRefPtr<UiCompositorControllerChild> sChild;
static StaticRefPtr<UiCompositorControllerParent> sParent;

UiCompositorControllerChild::UiCompositorControllerChild(RefPtr<nsThread> aThread, const uint64_t& aProcessToken)
 : mUiThread(aThread),
   mProcessToken(aProcessToken)
{
}

UiCompositorControllerChild::~UiCompositorControllerChild()
{
}

/* static */ UiCompositorControllerChild*
UiCompositorControllerChild::Get()
{
  return sChild;
}

/* static */ bool
UiCompositorControllerChild::IsInitialized()
{
  return sInitialized;
}

/* static */ void
UiCompositorControllerChild::Shutdown()
{
  RefPtr<UiCompositorControllerChild> child = sChild;
  if (child) {
    child->Close();
    sInitialized = false;
  }
}

/* static */ void
UiCompositorControllerChild::InitSameProcess(RefPtr<nsThread> aThread)
{
  MOZ_ASSERT(!sChild);
  MOZ_ASSERT(!sParent);
  MOZ_ASSERT(aThread);
  MOZ_ASSERT(!sInitialized);

  sInitialized = true;
  RefPtr<UiCompositorControllerChild> child = new UiCompositorControllerChild(aThread, 0);
  sParent = new UiCompositorControllerParent();
  aThread->Dispatch(NewRunnableMethod(child, &UiCompositorControllerChild::OpenForSameProcess), nsIThread::DISPATCH_NORMAL);
}

/* static */ void
UiCompositorControllerChild::InitWithGPUProcess(RefPtr<nsThread> aThread,
                                                const uint64_t& aProcessToken,
                                                Endpoint<PUiCompositorControllerChild>&& aEndpoint)
{
  MOZ_ASSERT(!sChild);
  MOZ_ASSERT(!sParent);
  MOZ_ASSERT(aThread);
  MOZ_ASSERT(!sInitialized);

  sInitialized = true;
  RefPtr<UiCompositorControllerChild> child = new UiCompositorControllerChild(aThread, aProcessToken);

  RefPtr<nsIRunnable> task = NewRunnableMethod<Endpoint<PUiCompositorControllerChild>&&>(
    child, &UiCompositorControllerChild::OpenForGPUProcess, Move(aEndpoint));

  aThread->Dispatch(task.forget(), nsIThread::DISPATCH_NORMAL);
}

void
UiCompositorControllerChild::OpenForSameProcess()
{
  MOZ_ASSERT(sParent);
  MOZ_ASSERT(!sChild);
  MOZ_ASSERT(IsOnUiThread());

  if (!Open(sParent->GetIPCChannel(),
           mozilla::layers::CompositorThreadHolder::Loop(),
           mozilla::ipc::ChildSide)) {
    sParent = nullptr;
    return;
  }

  AddRef();
  sChild = this;
}

void
UiCompositorControllerChild::OpenForGPUProcess(Endpoint<PUiCompositorControllerChild>&& aEndpoint)
{
  MOZ_ASSERT(!sChild);
  MOZ_ASSERT(IsOnUiThread());

  if (!aEndpoint.Bind(this)) {
    // The GPU Process Manager might be gone if we receive ActorDestroy very
    // late in shutdown.
    if (GPUProcessManager* gpm = GPUProcessManager::Get()) {
      gpm->NotifyRemoteActorDestroyed(mProcessToken);
    }
    return;
  }

  AddRef();
  sChild = this;
}

void
UiCompositorControllerChild::Close()
{
  if (!IsOnUiThread()) {
    mUiThread->Dispatch(NewRunnableMethod(this, &UiCompositorControllerChild::Close), nsIThread::DISPATCH_NORMAL);
    return;
  }

  // We clear mProcessToken when the channel is closed.
  if (!mProcessToken) {
    return;
  }

  // Clear the process token so we don't notify the GPUProcessManager. It already
  // knows we're closed since it manually called Close, and in fact the GPM could
  // have already been destroyed during shutdown.
  mProcessToken = 0;
  if (this == sChild) {
    sChild = nullptr;
  }

  // Close the underlying IPC channel.
  PUiCompositorControllerChild::Close();
}

void
UiCompositorControllerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mProcessToken) {
    GPUProcessManager::Get()->NotifyRemoteActorDestroyed(mProcessToken);
    mProcessToken = 0;
    sParent = nullptr;
  }
}

void
UiCompositorControllerChild::DeallocPUiCompositorControllerChild()
{
  Release();
  sInitialized = false;
}

void
UiCompositorControllerChild::ProcessingError(Result aCode, const char* aReason)
{
  MOZ_RELEASE_ASSERT(aCode == MsgDropped, "Processing error in UiCompositorControllerChild");
}

void
UiCompositorControllerChild::HandleFatalError(const char* aName, const char* aMsg) const
{
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aName, aMsg, OtherPid());
}

bool
UiCompositorControllerChild::IsOnUiThread() const
{
  return NS_GetCurrentThread() == mUiThread;
}

} // namespace layers
} // namespace mozilla
