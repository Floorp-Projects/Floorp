/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderManagerChild.h"

#include "RemoteAudioDecoder.h"
#include "RemoteDecoderChild.h"
#include "RemoteMediaDataDecoder.h"
#include "RemoteVideoDecoder.h"
#include "VideoUtils.h"
#include "mozilla/DataMutex.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/dom/ContentChild.h"  // for launching RDD w/ ContentChild
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "nsIObserver.h"

namespace mozilla {

using namespace layers;
using namespace gfx;

// Used so that we only ever attempt to check if the RDD process should be
// launched serially.
StaticMutex sLaunchMutex;

// Only modified on the main-thread, read on any thread. While it could be read
// on the main thread directly, for clarity we force access via the DataMutex
// wrapper.
static StaticDataMutex<StaticRefPtr<nsIThread>>
    sRemoteDecoderManagerChildThread("sRemoteDecoderManagerChildThread");

// Only accessed from sRemoteDecoderManagerChildThread
static StaticRefPtr<RemoteDecoderManagerChild>
    sRemoteDecoderManagerChildForRDDProcess;

static StaticRefPtr<RemoteDecoderManagerChild>
    sRemoteDecoderManagerChildForGPUProcess;
static UniquePtr<nsTArray<RefPtr<Runnable>>> sRecreateTasks;

class ShutdownObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

 protected:
  ~ShutdownObserver() = default;
};
NS_IMPL_ISUPPORTS(ShutdownObserver, nsIObserver);

NS_IMETHODIMP
ShutdownObserver::Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData) {
  MOZ_ASSERT(!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID));
  RemoteDecoderManagerChild::Shutdown();
  return NS_OK;
}

StaticRefPtr<ShutdownObserver> sObserver;

/* static */
void RemoteDecoderManagerChild::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  auto remoteDecoderManagerThread = sRemoteDecoderManagerChildThread.Lock();
  if (!*remoteDecoderManagerThread) {
    // We can't use a MediaThreadType::CONTROLLER as the GpuDecoderModule and
    // RemoteDecoderModule runs on it and dispatch synchronous tasks to the
    // manager thread, should more than 4 concurrent videos being instantiated
    // at the same time, we could end up in a deadlock.
    RefPtr<nsIThread> childThread;
    nsresult rv = NS_NewNamedThread(
        "RemVidChild", getter_AddRefs(childThread),
        NS_NewRunnableFunction(
            "RemoteDecoderManagerChild::InitPBackground", []() {
              ipc::PBackgroundChild* bgActor =
                  ipc::BackgroundChild::GetOrCreateForCurrentThread();
              NS_ASSERTION(bgActor, "Failed to start Background channel");
              Unused << bgActor;
            }));

    NS_ENSURE_SUCCESS_VOID(rv);
    *remoteDecoderManagerThread = childThread;
    sRecreateTasks = MakeUnique<nsTArray<RefPtr<Runnable>>>();
    sObserver = new ShutdownObserver();
    nsContentUtils::RegisterShutdownObserver(sObserver);
  }
}

/* static */
void RemoteDecoderManagerChild::InitForGPUProcess(
    Endpoint<PRemoteDecoderManagerChild>&& aVideoManager) {
  MOZ_ASSERT(NS_IsMainThread());

  Init();

  auto remoteDecoderManagerThread = sRemoteDecoderManagerChildThread.Lock();
  MOZ_ALWAYS_SUCCEEDS((*remoteDecoderManagerThread)
                          ->Dispatch(NewRunnableFunction(
                              "InitForContentRunnable", &OpenForGPUProcess,
                              std::move(aVideoManager))));
}

/* static */
void RemoteDecoderManagerChild::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sObserver) {
    nsContentUtils::UnregisterShutdownObserver(sObserver);
    sObserver = nullptr;
  }

  nsCOMPtr<nsIThread> childThread;
  {
    auto remoteDecoderManagerThread = sRemoteDecoderManagerChildThread.Lock();
    childThread = remoteDecoderManagerThread->forget();
  }
  if (childThread) {
    MOZ_ALWAYS_SUCCEEDS(childThread->Dispatch(NS_NewRunnableFunction(
        "dom::RemoteDecoderManagerChild::Shutdown", []() {
          if (sRemoteDecoderManagerChildForRDDProcess &&
              sRemoteDecoderManagerChildForRDDProcess->CanSend()) {
            sRemoteDecoderManagerChildForRDDProcess->Close();
          }
          sRemoteDecoderManagerChildForRDDProcess = nullptr;
          if (sRemoteDecoderManagerChildForGPUProcess &&
              sRemoteDecoderManagerChildForGPUProcess->CanSend()) {
            sRemoteDecoderManagerChildForGPUProcess->Close();
          }
          sRemoteDecoderManagerChildForGPUProcess = nullptr;
          ipc::BackgroundChild::CloseForCurrentThread();
        })));
    childThread->Shutdown();
    sRecreateTasks = nullptr;
  }
}

void RemoteDecoderManagerChild::RunWhenGPUProcessRecreated(
    already_AddRefed<Runnable> aTask) {
  MOZ_ASSERT(GetManagerThread() && GetManagerThread()->IsOnCurrentThread());

  // If we've already been recreated, then run the task immediately.
  auto* manager = GetSingleton(RemoteDecodeIn::GpuProcess);
  if (manager && manager != this && manager->CanSend()) {
    RefPtr<Runnable> task = aTask;
    task->Run();
  } else {
    sRecreateTasks->AppendElement(aTask);
  }
}

/* static */
RemoteDecoderManagerChild* RemoteDecoderManagerChild::GetSingleton(
    RemoteDecodeIn aLocation) {
  MOZ_ASSERT(GetManagerThread() && GetManagerThread()->IsOnCurrentThread());
  switch (aLocation) {
    case RemoteDecodeIn::GpuProcess:
      return sRemoteDecoderManagerChildForGPUProcess;
    case RemoteDecodeIn::RddProcess:
      return sRemoteDecoderManagerChildForRDDProcess;
    default:
      MOZ_CRASH("Unexpected RemoteDecode variant");
  }
}

/* static */
nsISerialEventTarget* RemoteDecoderManagerChild::GetManagerThread() {
  auto remoteDecoderManagerThread = sRemoteDecoderManagerChildThread.Lock();
  return *remoteDecoderManagerThread;
}

/* static */
bool RemoteDecoderManagerChild::Supports(
    RemoteDecodeIn aLocation, const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) {
  RefPtr<PDMFactory> pdm;
  switch (aLocation) {
    case RemoteDecodeIn::RddProcess:
      pdm = PDMFactory::PDMFactoryForRdd();
      break;
    case RemoteDecodeIn::GpuProcess:
      pdm = PDMFactory::PDMFactoryForGpu();
      break;
    default:
      return false;
  }
  return pdm->Supports(aParams, aDiagnostics);
}

/* static */
already_AddRefed<MediaDataDecoder>
RemoteDecoderManagerChild::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return nullptr;
  }

  RefPtr<RemoteAudioDecoderChild> child;
  MediaResult result(NS_ERROR_DOM_MEDIA_CANCELED);

  // We can use child as a ref here because this is a sync dispatch. In
  // the error case for InitIPDL, we can't just let the RefPtr go out of
  // scope at the end of the method because it will release the
  // RemoteAudioDecoderChild on the wrong thread.  This will assert in
  // RemoteDecoderChild's destructor.  Passing the RefPtr by reference
  // allows us to release the RemoteAudioDecoderChild on the manager
  // thread during this single dispatch.
  RefPtr<Runnable> task =
      NS_NewRunnableFunction("RemoteDecoderModule::CreateAudioDecoder", [&]() {
        child = new RemoteAudioDecoderChild();
        result = child->InitIPDL(aParams.AudioConfig(), aParams.mOptions);
        if (NS_FAILED(result)) {
          // Release RemoteAudioDecoderChild here, while we're on
          // manager thread.  Don't just let the RefPtr go out of scope.
          child = nullptr;
        }
      });
  SyncRunnable::DispatchToThread(managerThread, task);

  if (NS_FAILED(result)) {
    if (aParams.mError) {
      *aParams.mError = result;
    }
    return nullptr;
  }

  RefPtr<RemoteMediaDataDecoder> object = new RemoteMediaDataDecoder(child);

  return object.forget();
}

/* static */
already_AddRefed<MediaDataDecoder>
RemoteDecoderManagerChild::CreateVideoDecoder(
    const CreateDecoderParams& aParams, RemoteDecodeIn aLocation) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return nullptr;
  }

  MOZ_ASSERT(aLocation != RemoteDecodeIn::Unspecified);
  RefPtr<RemoteVideoDecoderChild> child;
  MediaResult result(NS_ERROR_DOM_MEDIA_CANCELED);

  // We can use child as a ref here because this is a sync dispatch. In
  // the error case for InitIPDL, we can't just let the RefPtr go out of
  // scope at the end of the method because it will release the
  // RemoteVideoDecoderChild on the wrong thread.  This will assert in
  // RemoteDecoderChild's destructor.  Passing the RefPtr by reference
  // allows us to release the RemoteVideoDecoderChild on the manager
  // thread during this single dispatch.
  RefPtr<Runnable> task = NS_NewRunnableFunction(
      "RemoteDecoderManagerChild::CreateVideoDecoder", [&]() {
        child = new RemoteVideoDecoderChild(aLocation);
        result = child->InitIPDL(
            aParams.VideoConfig(), aParams.mRate.mValue, aParams.mOptions,
            aParams.mKnowsCompositor
                ? &aParams.mKnowsCompositor->GetTextureFactoryIdentifier()
                : nullptr);
        if (NS_FAILED(result)) {
          // Release RemoteVideoDecoderChild here, while we're on
          // manager thread.  Don't just let the RefPtr go out of scope.
          child = nullptr;
        }
      });
  SyncRunnable::DispatchToThread(managerThread, task);

  if (NS_FAILED(result)) {
    if (aParams.mError) {
      *aParams.mError = result;
    }
    return nullptr;
  }

  RefPtr<RemoteMediaDataDecoder> object = new RemoteMediaDataDecoder(child);

  return object.forget();
}

/* static */
void RemoteDecoderManagerChild::LaunchRDDProcessIfNeeded(
    RemoteDecodeIn aLocation) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess(),
                        "Only supported from a content process.");

  if (aLocation != RemoteDecodeIn::RddProcess) {
    // Not targeting RDD process? No need to launch RDD.
    return;
  }

  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return;
  }

  StaticMutexAutoLock lock(sLaunchMutex);

  // We have a couple possible states here.  We are in a content process
  // and:
  // 1) the RDD process has never been launched.  RDD should be launched
  //    and the IPC connections setup.
  // 2) the RDD process has been launched, but this particular content
  //    process has not setup (or has lost) its IPC connection.
  // In the code below, we assume we need to launch the RDD process and
  // setup the IPC connections.  However, if the manager thread for
  // RemoteDecoderManagerChild is available we do a quick check to see
  // if we can send (meaning the IPC channel is open).  If we can send,
  // then no work is necessary.  If we can't send, then we call
  // LaunchRDDProcess which will launch RDD if necessary, and setup the
  // IPC connections between *this* content process and the RDD process.

  bool needsLaunch = true;
  RefPtr<Runnable> task = NS_NewRunnableFunction(
      "RemoteDecoderManagerChild::LaunchRDDProcessIfNeeded-CheckSend", [&]() {
        auto* rps = GetSingleton(RemoteDecodeIn::RddProcess);
        needsLaunch = rps ? !rps->CanSend() : true;
      });
  if (NS_FAILED(SyncRunnable::DispatchToThread(managerThread, task))) {
    return;
  };

  if (needsLaunch) {
    managerThread->Dispatch(NS_NewRunnableFunction(
        "RemoteDecoderManagerChild::EnsureRDDProcessAndCreateBridge", [&]() {
          ipc::PBackgroundChild* bgActor =
              ipc::BackgroundChild::GetForCurrentThread();
          if (NS_WARN_IF(!bgActor)) {
            return;
          }
          nsresult rv;
          Endpoint<PRemoteDecoderManagerChild> endpoint;
          Unused << bgActor->SendEnsureRDDProcessAndCreateBridge(&rv,
                                                                 &endpoint);
          if (NS_SUCCEEDED(rv)) {
            OpenForRDDProcess(std::move(endpoint));
          }
        }));
  }
}

PRemoteDecoderChild* RemoteDecoderManagerChild::AllocPRemoteDecoderChild(
    const RemoteDecoderInfoIPDL& /* not used */,
    const CreateDecoderParams::OptionSet& aOptions,
    const Maybe<layers::TextureFactoryIdentifier>& aIdentifier, bool* aSuccess,
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

RemoteDecoderManagerChild::RemoteDecoderManagerChild(RemoteDecodeIn aLocation)
    : mLocation(aLocation) {}

void RemoteDecoderManagerChild::OpenForRDDProcess(
    Endpoint<PRemoteDecoderManagerChild>&& aEndpoint) {
  MOZ_ASSERT(GetManagerThread() && GetManagerThread()->IsOnCurrentThread());
  // Only create RemoteDecoderManagerChild, bind new endpoint and init
  // ipdl if:
  // 1) haven't init'd sRemoteDecoderManagerChild
  // or
  // 2) if ActorDestroy was called meaning the other end of the ipc channel was
  //    torn down
  if (sRemoteDecoderManagerChildForRDDProcess &&
      sRemoteDecoderManagerChildForRDDProcess->CanSend()) {
    return;
  }
  sRemoteDecoderManagerChildForRDDProcess = nullptr;
  if (aEndpoint.IsValid()) {
    RefPtr<RemoteDecoderManagerChild> manager =
        new RemoteDecoderManagerChild(RemoteDecodeIn::RddProcess);
    if (aEndpoint.Bind(manager)) {
      sRemoteDecoderManagerChildForRDDProcess = manager;
      manager->InitIPDL();
    }
  }
}

void RemoteDecoderManagerChild::OpenForGPUProcess(
    Endpoint<PRemoteDecoderManagerChild>&& aEndpoint) {
  MOZ_ASSERT(GetManagerThread() && GetManagerThread()->IsOnCurrentThread());
  // Make sure we always dispatch everything in sRecreateTasks, even if we
  // fail since this is as close to being recreated as we will ever be.
  sRemoteDecoderManagerChildForGPUProcess = nullptr;
  if (aEndpoint.IsValid()) {
    RefPtr<RemoteDecoderManagerChild> manager =
        new RemoteDecoderManagerChild(RemoteDecodeIn::GpuProcess);
    if (aEndpoint.Bind(manager)) {
      sRemoteDecoderManagerChildForGPUProcess = manager;
      manager->InitIPDL();
    }
  }
  for (Runnable* task : *sRecreateTasks) {
    task->Run();
  }
  sRecreateTasks->Clear();
}

void RemoteDecoderManagerChild::InitIPDL() { mIPDLSelfRef = this; }

void RemoteDecoderManagerChild::ActorDealloc() { mIPDLSelfRef = nullptr; }

VideoBridgeSource RemoteDecoderManagerChild::GetSource() const {
  switch (mLocation) {
    case RemoteDecodeIn::RddProcess:
      return VideoBridgeSource::RddProcess;
    case RemoteDecodeIn::GpuProcess:
      return VideoBridgeSource::GpuProcess;
    default:
      MOZ_CRASH("Unexpected RemoteDecode variant");
  }
}

bool RemoteDecoderManagerChild::DeallocShmem(mozilla::ipc::Shmem& aShmem) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    return false;
  }
  if (!managerThread->IsOnCurrentThread()) {
    MOZ_ALWAYS_SUCCEEDS(managerThread->Dispatch(NS_NewRunnableFunction(
        "RemoteDecoderManagerChild::DeallocShmem",
        [self = RefPtr{this}, shmem = aShmem]() mutable {
          if (self->CanSend()) {
            self->PRemoteDecoderManagerChild::DeallocShmem(shmem);
          }
        })));
    return true;
  }
  return PRemoteDecoderManagerChild::DeallocShmem(aShmem);
}

struct SurfaceDescriptorUserData {
  SurfaceDescriptorUserData(RemoteDecoderManagerChild* aAllocator,
                            SurfaceDescriptor& aSD)
      : mAllocator(aAllocator), mSD(aSD) {}
  ~SurfaceDescriptorUserData() { DestroySurfaceDescriptor(mAllocator, &mSD); }

  RefPtr<RemoteDecoderManagerChild> mAllocator;
  SurfaceDescriptor mSD;
};

void DeleteSurfaceDescriptorUserData(void* aClosure) {
  SurfaceDescriptorUserData* sd =
      reinterpret_cast<SurfaceDescriptorUserData*>(aClosure);
  delete sd;
}

already_AddRefed<SourceSurface> RemoteDecoderManagerChild::Readback(
    const SurfaceDescriptorGPUVideo& aSD) {
  // We can't use NS_DISPATCH_SYNC here since that can spin the event
  // loop while it waits. This function can be called from JS and we
  // don't want that to happen.
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    return nullptr;
  }

  SurfaceDescriptor sd;
  RefPtr<Runnable> task =
      NS_NewRunnableFunction("RemoteDecoderManagerChild::Readback", [&]() {
        if (CanSend()) {
          SendReadback(aSD, &sd);
        }
      });
  SyncRunnable::DispatchToThread(managerThread, task);

  if (!IsSurfaceDescriptorValid(sd)) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> source = GetSurfaceForDescriptor(sd);
  if (!source) {
    DestroySurfaceDescriptor(this, &sd);
    NS_WARNING("Failed to map SurfaceDescriptor in Readback");
    return nullptr;
  }

  static UserDataKey sSurfaceDescriptor;
  source->AddUserData(&sSurfaceDescriptor,
                      new SurfaceDescriptorUserData(this, sd),
                      DeleteSurfaceDescriptorUserData);

  return source.forget();
}

void RemoteDecoderManagerChild::DeallocateSurfaceDescriptor(
    const SurfaceDescriptorGPUVideo& aSD) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    return;
  }
  MOZ_ALWAYS_SUCCEEDS(managerThread->Dispatch(NS_NewRunnableFunction(
      "RemoteDecoderManagerChild::DeallocateSurfaceDescriptor",
      [ref = RefPtr{this}, sd = aSD]() {
        if (ref->CanSend()) {
          ref->SendDeallocateSurfaceDescriptorGPUVideo(sd);
        }
      })));
}

void RemoteDecoderManagerChild::HandleFatalError(const char* aMsg) const {
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aMsg, OtherPid());
}

}  // namespace mozilla
