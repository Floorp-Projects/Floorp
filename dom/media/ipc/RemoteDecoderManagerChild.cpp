/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderManagerChild.h"

#include "RemoteAudioDecoder.h"
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
// launched serially. Protects sLaunchPromise
StaticMutex sLaunchMutex;
static StaticRefPtr<GenericNonExclusivePromise> sLaunchRDDPromise;

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
    // We can't use a MediaThreadType::SUPERVISOR as the RemoteDecoderModule
    // runs on it and dispatch synchronous tasks to the manager thread, should
    // more than 4 concurrent videos being instantiated at the same time, we
    // could end up in a deadlock.
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
RefPtr<PlatformDecoderModule::CreateDecoderPromise>
RemoteDecoderManagerChild::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  }
  return LaunchRDDProcessIfNeeded()->Then(
      managerThread, __func__,
      [params = CreateDecoderParamsForAsync(aParams)](bool) {
        auto child = MakeRefPtr<RemoteAudioDecoderChild>();
        MediaResult result =
            child->InitIPDL(params.AudioConfig(), params.mOptions);
        if (NS_FAILED(result)) {
          return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
              result, __func__);
        }
        return Construct(std::move(child));
      },
      [](nsresult aResult) {
        return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
            MediaResult(aResult, "Couldn't start RDD process"), __func__);
      });
}

/* static */
RefPtr<PlatformDecoderModule::CreateDecoderPromise>
RemoteDecoderManagerChild::CreateVideoDecoder(
    const CreateDecoderParams& aParams, RemoteDecodeIn aLocation) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  }

  if (!aParams.mKnowsCompositor && aLocation == RemoteDecodeIn::GpuProcess) {
    // We don't have an image bridge; don't attempt to decode in the GPU
    // process.
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR, __func__);
  }

  MOZ_ASSERT(aLocation != RemoteDecodeIn::Unspecified);

  RefPtr<GenericNonExclusivePromise> p =
      aLocation == RemoteDecodeIn::GpuProcess
          ? GenericNonExclusivePromise::CreateAndResolve(true, __func__)
          : LaunchRDDProcessIfNeeded();

  return p->Then(
      managerThread, __func__,
      [aLocation, params = CreateDecoderParamsForAsync(aParams)](bool) {
        auto child = MakeRefPtr<RemoteVideoDecoderChild>(aLocation);
        MediaResult result = child->InitIPDL(
            params.VideoConfig(), params.mRate.mValue, params.mOptions,
            params.mKnowsCompositor
                ? Some(params.mKnowsCompositor->GetTextureFactoryIdentifier())
                : Nothing());
        if (NS_FAILED(result)) {
          return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
              result, __func__);
        }
        return Construct(std::move(child));
      },
      [](nsresult aResult) {
        return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
            MediaResult(aResult, "Couldn't start RDD process"), __func__);
      });
}

/* static */
RefPtr<PlatformDecoderModule::CreateDecoderPromise>
RemoteDecoderManagerChild::Construct(RefPtr<RemoteDecoderChild>&& aChild) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  }
  MOZ_ASSERT(managerThread->IsOnCurrentThread());

  RefPtr<PlatformDecoderModule::CreateDecoderPromise> p =
      aChild->SendConstruct()->Then(
          managerThread, __func__,
          [child = std::move(aChild)](MediaResult aResult) {
            if (NS_FAILED(aResult)) {
              // We will never get to use this remote decoder, tear it down.
              child->DestroyIPDL();
              return PlatformDecoderModule::CreateDecoderPromise::
                  CreateAndReject(aResult, __func__);
            }
            return PlatformDecoderModule::CreateDecoderPromise::
                CreateAndResolve(MakeRefPtr<RemoteMediaDataDecoder>(child),
                                 __func__);
          },
          [](const mozilla::ipc::ResponseRejectReason& aReason) {
            // The parent has died.
            return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
                NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER, __func__);
          });
  return p;
}

/* static */
RefPtr<GenericNonExclusivePromise>
RemoteDecoderManagerChild::LaunchRDDProcessIfNeeded() {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess(),
                        "Only supported from a content process.");

  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
  }

  StaticMutexAutoLock lock(sLaunchMutex);

  if (sLaunchRDDPromise) {
    return sLaunchRDDPromise;
  }

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

  RefPtr<GenericNonExclusivePromise> p = InvokeAsync(
      managerThread, __func__, []() -> RefPtr<GenericNonExclusivePromise> {
        auto* rps = GetSingleton(RemoteDecodeIn::RddProcess);
        if (rps && rps->CanSend()) {
          return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
        }
        nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
        ipc::PBackgroundChild* bgActor =
            ipc::BackgroundChild::GetForCurrentThread();
        if (!managerThread || NS_WARN_IF(!bgActor)) {
          return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                             __func__);
        }

        return bgActor->SendEnsureRDDProcessAndCreateBridge()->Then(
            managerThread, __func__,
            [](ipc::PBackgroundChild::EnsureRDDProcessAndCreateBridgePromise::
                   ResolveOrRejectValue&& aResult) {
              nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
              if (!managerThread || aResult.IsReject()) {
                // The parent process died or we got shutdown
                return GenericNonExclusivePromise::CreateAndReject(
                    NS_ERROR_FAILURE, __func__);
              }
              nsresult rv = Get<0>(aResult.ResolveValue());
              if (NS_FAILED(rv)) {
                return GenericNonExclusivePromise::CreateAndReject(rv,
                                                                   __func__);
              }
              OpenForRDDProcess(Get<1>(std::move(aResult.ResolveValue())));
              return GenericNonExclusivePromise::CreateAndResolve(true,
                                                                  __func__);
            });
      });

  p = p->Then(
      GetCurrentSerialEventTarget(), __func__,
      [](const GenericNonExclusivePromise::ResolveOrRejectValue& aResult) {
        StaticMutexAutoLock lock(sLaunchMutex);
        sLaunchRDDPromise = nullptr;
        return GenericNonExclusivePromise::CreateAndResolveOrReject(aResult,
                                                                    __func__);
      });
  sLaunchRDDPromise = p;
  return sLaunchRDDPromise;
}

PRemoteDecoderChild* RemoteDecoderManagerChild::AllocPRemoteDecoderChild(
    const RemoteDecoderInfoIPDL& /* not used */,
    const CreateDecoderParams::OptionSet& aOptions,
    const Maybe<layers::TextureFactoryIdentifier>& aIdentifier) {
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
