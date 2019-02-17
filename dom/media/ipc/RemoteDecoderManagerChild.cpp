/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderManagerChild.h"

#include "base/task.h"

#include "RemoteDecoderChild.h"

namespace mozilla {

// Only modified on the main-thread
StaticRefPtr<nsIThread> sRemoteDecoderManagerChildThread;
StaticRefPtr<AbstractThread> sRemoteDecoderManagerChildAbstractThread;

// Only accessed from sRemoteDecoderManagerChildThread
static StaticRefPtr<RemoteDecoderManagerChild> sRemoteDecoderManagerChild;

/* static */ void RemoteDecoderManagerChild::InitializeThread() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sRemoteDecoderManagerChildThread) {
    RefPtr<nsIThread> childThread;
    nsresult rv = NS_NewNamedThread("RemVidChild", getter_AddRefs(childThread));
    NS_ENSURE_SUCCESS_VOID(rv);
    sRemoteDecoderManagerChildThread = childThread;

    sRemoteDecoderManagerChildAbstractThread =
        AbstractThread::CreateXPCOMThreadWrapper(childThread, false);
  }
}

/* static */ void RemoteDecoderManagerChild::InitForContent(
    Endpoint<PRemoteDecoderManagerChild>&& aVideoManager) {
  InitializeThread();
  sRemoteDecoderManagerChildThread->Dispatch(
      NewRunnableFunction("InitForContentRunnable", &Open,
                          std::move(aVideoManager)),
      NS_DISPATCH_NORMAL);
}

/* static */ void RemoteDecoderManagerChild::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sRemoteDecoderManagerChildThread) {
    sRemoteDecoderManagerChildThread->Dispatch(
        NS_NewRunnableFunction("dom::RemoteDecoderManagerChild::Shutdown",
                               []() {
                                 if (sRemoteDecoderManagerChild &&
                                     sRemoteDecoderManagerChild->CanSend()) {
                                   sRemoteDecoderManagerChild->Close();
                                   sRemoteDecoderManagerChild = nullptr;
                                 }
                               }),
        NS_DISPATCH_NORMAL);

    sRemoteDecoderManagerChildAbstractThread = nullptr;
    sRemoteDecoderManagerChildThread->Shutdown();
    sRemoteDecoderManagerChildThread = nullptr;
  }
}

/* static */ RemoteDecoderManagerChild*
RemoteDecoderManagerChild::GetSingleton() {
  MOZ_ASSERT(NS_GetCurrentThread() == GetManagerThread());
  return sRemoteDecoderManagerChild;
}

/* static */ nsIThread* RemoteDecoderManagerChild::GetManagerThread() {
  return sRemoteDecoderManagerChildThread;
}

/* static */ AbstractThread*
RemoteDecoderManagerChild::GetManagerAbstractThread() {
  return sRemoteDecoderManagerChildAbstractThread;
}

PRemoteDecoderChild* RemoteDecoderManagerChild::AllocPRemoteDecoderChild(
    const RemoteDecoderInfoIPDL& /* not used */,
    const CreateDecoderParams::OptionSet& /* not used */, bool* /* not used */,
    nsCString* /* not used */) {
  // RemoteDecoderModule is responsible for creating RemoteDecoderChild
  // classes.
  MOZ_ASSERT(false,
             "RemoteDecoderManagerChild cannot create "
             "RemoteDecoderChild classes");
  return nullptr;
}

bool RemoteDecoderManagerChild::DeallocPRemoteDecoderChild(
    PRemoteDecoderChild* actor) {
  RemoteDecoderChild* child = static_cast<RemoteDecoderChild*>(actor);
  child->IPDLActorDestroyed();
  return true;
}

void RemoteDecoderManagerChild::Open(
    Endpoint<PRemoteDecoderManagerChild>&& aEndpoint) {
  MOZ_ASSERT(NS_GetCurrentThread() == GetManagerThread());
  // Only create RemoteDecoderManagerChild, bind new endpoint and init
  // ipdl if:
  // 1) haven't init'd sRemoteDecoderManagerChild
  // or
  // 2) if ActorDestroy was called (mCanSend is false) meaning the other
  // end of the ipc channel was torn down
  if (sRemoteDecoderManagerChild && sRemoteDecoderManagerChild->mCanSend) {
    return;
  }
  sRemoteDecoderManagerChild = nullptr;
  if (aEndpoint.IsValid()) {
    RefPtr<RemoteDecoderManagerChild> manager = new RemoteDecoderManagerChild();
    if (aEndpoint.Bind(manager)) {
      sRemoteDecoderManagerChild = manager;
      manager->InitIPDL();
    }
  }
}

void RemoteDecoderManagerChild::InitIPDL() {
  mCanSend = true;
  mIPDLSelfRef = this;
}

void RemoteDecoderManagerChild::ActorDestroy(ActorDestroyReason aWhy) {
  mCanSend = false;
}

void RemoteDecoderManagerChild::DeallocPRemoteDecoderManagerChild() {
  mIPDLSelfRef = nullptr;
}

bool RemoteDecoderManagerChild::CanSend() {
  MOZ_ASSERT(NS_GetCurrentThread() == GetManagerThread());
  return mCanSend;
}

}  // namespace mozilla
