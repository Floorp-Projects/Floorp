/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderManagerParent.h"

#if XP_WIN
#  include <objbase.h>
#endif

#include "RemoteAudioDecoder.h"
#include "RemoteVideoDecoder.h"
#include "VideoUtils.h"  // for MediaThreadType

namespace mozilla {

StaticRefPtr<nsIThread> sRemoteDecoderManagerParentThread;
StaticRefPtr<TaskQueue> sRemoteDecoderManagerTaskQueue;

class RemoteDecoderManagerThreadHolder {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteDecoderManagerThreadHolder)

 public:
  RemoteDecoderManagerThreadHolder() {}

 private:
  ~RemoteDecoderManagerThreadHolder() {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("dom::RemoteDecoderManagerThreadHolder::~"
                               "RemoteDecoderManagerThreadHolder",
                               []() {
                                 sRemoteDecoderManagerParentThread->Shutdown();
                                 sRemoteDecoderManagerParentThread = nullptr;
                               }));
  }
};

StaticRefPtr<RemoteDecoderManagerThreadHolder>
    sRemoteDecoderManagerParentThreadHolder;

class RemoteDecoderManagerThreadShutdownObserver : public nsIObserver {
  virtual ~RemoteDecoderManagerThreadShutdownObserver() = default;

 public:
  RemoteDecoderManagerThreadShutdownObserver() = default;

  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);

    RemoteDecoderManagerParent::ShutdownThreads();
    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(RemoteDecoderManagerThreadShutdownObserver, nsIObserver);

bool RemoteDecoderManagerParent::StartupThreads() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sRemoteDecoderManagerParentThread) {
    return true;
  }

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (!observerService) {
    return false;
  }

  RefPtr<nsIThread> managerThread;
  nsresult rv =
      NS_NewNamedThread("RemVidParent", getter_AddRefs(managerThread));
  if (NS_FAILED(rv)) {
    return false;
  }
  sRemoteDecoderManagerParentThread = managerThread;
  sRemoteDecoderManagerParentThreadHolder =
      new RemoteDecoderManagerThreadHolder();
#if XP_WIN
  sRemoteDecoderManagerParentThread->Dispatch(
      NS_NewRunnableFunction("RemoteDecoderManagerParent::StartupThreads",
                             []() {
                               DebugOnly<HRESULT> hr =
                                   CoInitializeEx(0, COINIT_MULTITHREADED);
                               MOZ_ASSERT(SUCCEEDED(hr));
                             }),
      NS_DISPATCH_NORMAL);
#endif

  sRemoteDecoderManagerTaskQueue = new TaskQueue(
      managerThread.forget(),
      "RemoteDecoderManagerParent::sRemoteDecoderManagerTaskQueue");

  auto* obs = new RemoteDecoderManagerThreadShutdownObserver();
  observerService->AddObserver(obs, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  return true;
}

void RemoteDecoderManagerParent::ShutdownThreads() {
  sRemoteDecoderManagerTaskQueue = nullptr;

  sRemoteDecoderManagerParentThreadHolder = nullptr;
  while (sRemoteDecoderManagerParentThread) {
    NS_ProcessNextEvent(nullptr, true);
  }
}

bool RemoteDecoderManagerParent::OnManagerThread() {
  return NS_GetCurrentThread() == sRemoteDecoderManagerParentThread;
}

bool RemoteDecoderManagerParent::CreateForContent(
    Endpoint<PRemoteDecoderManagerParent>&& aEndpoint) {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_RDD);
  MOZ_ASSERT(NS_IsMainThread());

  if (!StartupThreads()) {
    return false;
  }

  RefPtr<RemoteDecoderManagerParent> parent =
      new RemoteDecoderManagerParent(sRemoteDecoderManagerParentThreadHolder);

  RefPtr<Runnable> task =
      NewRunnableMethod<Endpoint<PRemoteDecoderManagerParent>&&>(
          "dom::RemoteDecoderManagerParent::Open", parent,
          &RemoteDecoderManagerParent::Open, std::move(aEndpoint));
  sRemoteDecoderManagerParentThread->Dispatch(task.forget(),
                                              NS_DISPATCH_NORMAL);
  return true;
}

RemoteDecoderManagerParent::RemoteDecoderManagerParent(
    RemoteDecoderManagerThreadHolder* aHolder)
    : mThreadHolder(aHolder) {
  MOZ_COUNT_CTOR(RemoteDecoderManagerParent);
}

RemoteDecoderManagerParent::~RemoteDecoderManagerParent() {
  MOZ_COUNT_DTOR(RemoteDecoderManagerParent);
}

void RemoteDecoderManagerParent::ActorDestroy(
    mozilla::ipc::IProtocol::ActorDestroyReason) {
  mThreadHolder = nullptr;
}

PRemoteDecoderParent* RemoteDecoderManagerParent::AllocPRemoteDecoderParent(
    const RemoteDecoderInfoIPDL& aRemoteDecoderInfo,
    const CreateDecoderParams::OptionSet& aOptions, bool* aSuccess,
    nsCString* aErrorDescription) {
  RefPtr<TaskQueue> decodeTaskQueue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
                    "RemoteVideoDecoderParent::mDecodeTaskQueue");

  if (aRemoteDecoderInfo.type() ==
      RemoteDecoderInfoIPDL::TVideoDecoderInfoIPDL) {
    const VideoDecoderInfoIPDL& decoderInfo =
        aRemoteDecoderInfo.get_VideoDecoderInfoIPDL();
    return new RemoteVideoDecoderParent(
        this, decoderInfo.videoInfo(), decoderInfo.framerate(), aOptions,
        sRemoteDecoderManagerTaskQueue, decodeTaskQueue, aSuccess,
        aErrorDescription);
  } else if (aRemoteDecoderInfo.type() == RemoteDecoderInfoIPDL::TAudioInfo) {
    return new RemoteAudioDecoderParent(
        this, aRemoteDecoderInfo.get_AudioInfo(), aOptions,
        sRemoteDecoderManagerTaskQueue, decodeTaskQueue, aSuccess,
        aErrorDescription);
  }

  MOZ_CRASH("unrecognized type of RemoteDecoderInfoIPDL union");
  return nullptr;
}

bool RemoteDecoderManagerParent::DeallocPRemoteDecoderParent(
    PRemoteDecoderParent* actor) {
  RemoteDecoderParent* parent = static_cast<RemoteDecoderParent*>(actor);
  parent->Destroy();
  return true;
}

void RemoteDecoderManagerParent::Open(
    Endpoint<PRemoteDecoderManagerParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    // We can't recover from this.
    MOZ_CRASH("Failed to bind RemoteDecoderManagerParent to endpoint");
  }
  AddRef();
}

void RemoteDecoderManagerParent::DeallocPRemoteDecoderManagerParent() {
  Release();
}

}  // namespace mozilla
