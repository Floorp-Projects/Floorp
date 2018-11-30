/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderManagerChild.h"

#include "base/task.h"

#include "RemoteVideoDecoderChild.h"

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

PRemoteVideoDecoderChild*
RemoteDecoderManagerChild::AllocPRemoteVideoDecoderChild(
    const VideoInfo& /* not used */, const float& /* not used */,
    const CreateDecoderParams::OptionSet& /* not used */, bool* /* not used */,
    nsCString* /* not used */) {
  return new RemoteVideoDecoderChild();
}

bool RemoteDecoderManagerChild::DeallocPRemoteVideoDecoderChild(
    PRemoteVideoDecoderChild* actor) {
  RemoteVideoDecoderChild* child = static_cast<RemoteVideoDecoderChild*>(actor);
  child->IPDLActorDestroyed();
  return true;
}

void RemoteDecoderManagerChild::Open(
    Endpoint<PRemoteDecoderManagerChild>&& aEndpoint) {
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
