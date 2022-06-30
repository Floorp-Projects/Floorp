/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderManagerChild.h"

#include "PDMFactory.h"
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
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "nsContentUtils.h"
#include "nsIObserver.h"
#include "mozilla/StaticPrefs_media.h"

#ifdef MOZ_WMF
#  include "MFMediaEngineChild.h"
#endif

namespace mozilla {

using namespace layers;
using namespace gfx;

// Used so that we only ever attempt to check if the RDD process should be
// launched serially. Protects sLaunchPromise
StaticMutex sLaunchRDDMutex;
static StaticRefPtr<GenericNonExclusivePromise> sLaunchRDDPromise
    GUARDED_BY(sLaunchRDDMutex);

StaticMutex sLaunchUtilityMutex;
static StaticRefPtr<GenericNonExclusivePromise> sLaunchUtilityPromise
    GUARDED_BY(sLaunchUtilityMutex);

// Only modified on the main-thread, read on any thread. While it could be read
// on the main thread directly, for clarity we force access via the DataMutex
// wrapper.
static StaticDataMutex<StaticRefPtr<nsIThread>>
    sRemoteDecoderManagerChildThread("sRemoteDecoderManagerChildThread");

// Only accessed from sRemoteDecoderManagerChildThread
static StaticRefPtr<RemoteDecoderManagerChild>
    sRemoteDecoderManagerChildForRDDProcess;
static StaticRefPtr<RemoteDecoderManagerChild>
    sRemoteDecoderManagerChildForUtilityProcess;

static StaticRefPtr<RemoteDecoderManagerChild>
    sRemoteDecoderManagerChildForGPUProcess;
static UniquePtr<nsTArray<RefPtr<Runnable>>> sRecreateTasks;

static StaticDataMutex<Maybe<PDMFactory::MediaCodecsSupported>> sGPUSupported(
    "RDMC::sGPUSupported");
static StaticDataMutex<Maybe<PDMFactory::MediaCodecsSupported>> sRDDSupported(
    "RDMC::sRDDSupported");
static StaticDataMutex<Maybe<PDMFactory::MediaCodecsSupported>>
    sUtilitySupported("RDMC::sUtilitySupported");

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
              NS_WARNING_ASSERTION(bgActor,
                                   "Failed to start Background channel");
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
          {
            StaticMutexAutoLock lock(sLaunchRDDMutex);
            sLaunchRDDPromise = nullptr;
          }
          if (sRemoteDecoderManagerChildForUtilityProcess &&
              sRemoteDecoderManagerChildForUtilityProcess->CanSend()) {
            sRemoteDecoderManagerChildForUtilityProcess->Close();
          }
          sRemoteDecoderManagerChildForUtilityProcess = nullptr;
          {
            StaticMutexAutoLock lock(sLaunchUtilityMutex);
            sLaunchUtilityPromise = nullptr;
          }
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
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We've been shutdown, bail.
    return;
  }
  MOZ_ASSERT(managerThread->IsOnCurrentThread());

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
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We've been shutdown, bail.
    return nullptr;
  }
  MOZ_ASSERT(managerThread->IsOnCurrentThread());
  switch (aLocation) {
    case RemoteDecodeIn::GpuProcess:
      return sRemoteDecoderManagerChildForGPUProcess;
    case RemoteDecodeIn::RddProcess:
      return sRemoteDecoderManagerChildForRDDProcess;
    case RemoteDecodeIn::UtilityProcess:
      return sRemoteDecoderManagerChildForUtilityProcess;
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
  Maybe<PDMFactory::MediaCodecsSupported> supported;
  switch (aLocation) {
    case RemoteDecodeIn::RddProcess: {
      auto supportedRDD = sRDDSupported.Lock();
      supported = *supportedRDD;
      break;
    }
    case RemoteDecodeIn::UtilityProcess: {
      auto supportedUtility = sUtilitySupported.Lock();
      supported = *supportedUtility;
      break;
    }
    case RemoteDecodeIn::GpuProcess: {
      auto supportedGPU = sGPUSupported.Lock();
      supported = *supportedGPU;
      break;
    }
    default:
      return false;
  }
  if (!supported) {
    // We haven't received the correct information yet from either the GPU or
    // the RDD process nor the Utility process; assume it is supported to
    // prevent false negative.
    if (aLocation == RemoteDecodeIn::UtilityProcess) {
      LaunchUtilityProcessIfNeeded();
    }
    if (aLocation == RemoteDecodeIn::RddProcess) {
      // Ensure the RDD process got started.
      // TODO: This can be removed once bug 1684991 is fixed.
      LaunchRDDProcessIfNeeded();
    }
    return true;
  }

  // We can ignore the SupportDecoderParams argument for now as creation of the
  // decoder will actually fail later and fallback PDMs will be tested on later.
  return PDMFactory::SupportsMimeType(aParams.MimeType(), *supported,
                                      aLocation);
}

/* static */
RefPtr<PlatformDecoderModule::CreateDecoderPromise>
RemoteDecoderManagerChild::CreateAudioDecoder(
    const CreateDecoderParams& aParams, RemoteDecodeIn aLocation) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  }

  bool useUtilityAudioDecoding = StaticPrefs::media_utility_process_enabled() &&
                                 aLocation == RemoteDecodeIn::UtilityProcess;
#ifdef MOZ_WMF
  // If the media engine Id is specified, using the media engine in the RDD
  // process instead.
  useUtilityAudioDecoding = useUtilityAudioDecoding &&
                            !(aParams.mMediaEngineId &&
                              StaticPrefs::media_wmf_media_engine_enabled());
#endif
  RefPtr<GenericNonExclusivePromise> launchPromise =
      useUtilityAudioDecoding ? LaunchUtilityProcessIfNeeded()
                              : LaunchRDDProcessIfNeeded();

  return launchPromise->Then(
      managerThread, __func__,
      [params = CreateDecoderParamsForAsync(aParams), aLocation](bool) {
        auto child = MakeRefPtr<RemoteAudioDecoderChild>();
        MediaResult result = child->InitIPDL(
            params.AudioConfig(), params.mOptions, params.mMediaEngineId);
        if (NS_FAILED(result)) {
          return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
              result, __func__);
        }
        return Construct(std::move(child), aLocation);
      },
      [aLocation](nsresult aResult) {
        return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
            MediaResult(aResult,
                        aLocation == RemoteDecodeIn::GpuProcess
                            ? "Couldn't start GPU process"
                            : (aLocation == RemoteDecodeIn::RddProcess
                                   ? "Couldn't start RDD process"
                                   : "Couldn't start Utility process")),
            __func__);
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
                : Nothing(),
            params.mMediaEngineId);
        if (NS_FAILED(result)) {
          return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
              result, __func__);
        }
        return Construct(std::move(child), aLocation);
      },
      [](nsresult aResult) {
        return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
            MediaResult(aResult, "Couldn't start RDD process"), __func__);
      });
}

/* static */
RefPtr<PlatformDecoderModule::CreateDecoderPromise>
RemoteDecoderManagerChild::Construct(RefPtr<RemoteDecoderChild>&& aChild,
                                     RemoteDecodeIn aLocation) {
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
          [aLocation](const mozilla::ipc::ResponseRejectReason& aReason) {
            // The parent has died.
            nsresult err =
                ((aLocation == RemoteDecodeIn::GpuProcess) ||
                 (aLocation == RemoteDecodeIn::RddProcess))
                    ? NS_ERROR_DOM_MEDIA_REMOTE_DECODER_CRASHED_RDD_OR_GPU_ERR
                    : NS_ERROR_DOM_MEDIA_REMOTE_DECODER_CRASHED_UTILITY_ERR;
            return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
                err, __func__);
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

  StaticMutexAutoLock lock(sLaunchRDDMutex);

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
        StaticMutexAutoLock lock(sLaunchRDDMutex);
        sLaunchRDDPromise = nullptr;
        return GenericNonExclusivePromise::CreateAndResolveOrReject(aResult,
                                                                    __func__);
      });
  sLaunchRDDPromise = p;
  return sLaunchRDDPromise;
}

/* static */
RefPtr<GenericNonExclusivePromise>
RemoteDecoderManagerChild::LaunchUtilityProcessIfNeeded() {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess(),
                        "Only supported from a content process.");

  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We got shutdown.
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
  }

  StaticMutexAutoLock lock(sLaunchUtilityMutex);

  if (sLaunchUtilityPromise) {
    return sLaunchUtilityPromise;
  }

  // We have a couple possible states here.  We are in a content process
  // and:
  // 1) the Utility process has never been launched.  Utility should be launched
  //    and the IPC connections setup.
  // 2) the Utility process has been launched, but this particular content
  //    process has not setup (or has lost) its IPC connection.
  // In the code below, we assume we need to launch the Utility process and
  // setup the IPC connections.  However, if the manager thread for
  // RemoteDecoderManagerChild is available we do a quick check to see
  // if we can send (meaning the IPC channel is open).  If we can send,
  // then no work is necessary.  If we can't send, then we call
  // LaunchUtilityProcess which will launch Utility if necessary, and setup the
  // IPC connections between *this* content process and the Utility process.

  RefPtr<GenericNonExclusivePromise> p = InvokeAsync(
      managerThread, __func__, []() -> RefPtr<GenericNonExclusivePromise> {
        auto* rps = GetSingleton(RemoteDecodeIn::UtilityProcess);
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

        return bgActor->SendEnsureUtilityProcessAndCreateBridge()->Then(
            managerThread, __func__,
            [](ipc::PBackgroundChild::
                   EnsureUtilityProcessAndCreateBridgePromise::
                       ResolveOrRejectValue&& aResult)
                -> RefPtr<GenericNonExclusivePromise> {
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
              OpenForUtilityProcess(Get<1>(std::move(aResult.ResolveValue())));
              return GenericNonExclusivePromise::CreateAndResolve(true,
                                                                  __func__);
            });
      });

  p = p->Then(
      GetCurrentSerialEventTarget(), __func__,
      [](const GenericNonExclusivePromise::ResolveOrRejectValue& aResult) {
        StaticMutexAutoLock lock(sLaunchUtilityMutex);
        sLaunchUtilityPromise = nullptr;
        return GenericNonExclusivePromise::CreateAndResolveOrReject(aResult,
                                                                    __func__);
      });
  sLaunchUtilityPromise = p;
  return sLaunchUtilityPromise;
}

PRemoteDecoderChild* RemoteDecoderManagerChild::AllocPRemoteDecoderChild(
    const RemoteDecoderInfoIPDL& /* not used */,
    const CreateDecoderParams::OptionSet& aOptions,
    const Maybe<layers::TextureFactoryIdentifier>& aIdentifier,
    const Maybe<uint64_t>& aMediaEngineId) {
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

PMFMediaEngineChild* RemoteDecoderManagerChild::AllocPMFMediaEngineChild() {
  MOZ_ASSERT_UNREACHABLE(
      "RemoteDecoderManagerChild cannot create MFMediaEngineChild classes");
  return nullptr;
}

bool RemoteDecoderManagerChild::DeallocPMFMediaEngineChild(
    PMFMediaEngineChild* actor) {
#ifdef MOZ_WMF
  MFMediaEngineChild* child = static_cast<MFMediaEngineChild*>(actor);
  child->IPDLActorDestroyed();
#endif
  return true;
}

RemoteDecoderManagerChild::RemoteDecoderManagerChild(RemoteDecodeIn aLocation)
    : mLocation(aLocation) {
  MOZ_ASSERT(mLocation == RemoteDecodeIn::GpuProcess ||
             mLocation == RemoteDecodeIn::RddProcess ||
             mLocation == RemoteDecodeIn::UtilityProcess);
}

void RemoteDecoderManagerChild::OpenForRDDProcess(
    Endpoint<PRemoteDecoderManagerChild>&& aEndpoint) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We've been shutdown, bail.
    return;
  }
  MOZ_ASSERT(managerThread->IsOnCurrentThread());

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

void RemoteDecoderManagerChild::OpenForUtilityProcess(
    Endpoint<PRemoteDecoderManagerChild>&& aEndpoint) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We've been shutdown, bail.
    return;
  }
  MOZ_ASSERT(managerThread->IsOnCurrentThread());

  // Only create RemoteDecoderManagerChild, bind new endpoint and init
  // ipdl if:
  // 1) haven't init'd sRemoteDecoderManagerChild
  // or
  // 2) if ActorDestroy was called meaning the other end of the ipc channel was
  //    torn down
  if (sRemoteDecoderManagerChildForUtilityProcess &&
      sRemoteDecoderManagerChildForUtilityProcess->CanSend()) {
    return;
  }
  sRemoteDecoderManagerChildForUtilityProcess = nullptr;
  if (aEndpoint.IsValid()) {
    RefPtr<RemoteDecoderManagerChild> manager =
        new RemoteDecoderManagerChild(RemoteDecodeIn::UtilityProcess);
    if (aEndpoint.Bind(manager)) {
      sRemoteDecoderManagerChildForUtilityProcess = manager;
      manager->InitIPDL();
    }
  }
}

void RemoteDecoderManagerChild::OpenForGPUProcess(
    Endpoint<PRemoteDecoderManagerChild>&& aEndpoint) {
  nsCOMPtr<nsISerialEventTarget> managerThread = GetManagerThread();
  if (!managerThread) {
    // We've been shutdown, bail.
    return;
  }
  MOZ_ASSERT(managerThread->IsOnCurrentThread());
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

void RemoteDecoderManagerChild::SetSupported(
    RemoteDecodeIn aLocation,
    const PDMFactory::MediaCodecsSupported& aSupported) {
  switch (aLocation) {
    case RemoteDecodeIn::GpuProcess: {
      auto supported = sGPUSupported.Lock();
      *supported = Some(aSupported);
      break;
    }
    case RemoteDecodeIn::RddProcess: {
      auto supported = sRDDSupported.Lock();
      *supported = Some(aSupported);
      break;
    }
    case RemoteDecodeIn::UtilityProcess: {
      auto supported = sUtilitySupported.Lock();
      *supported = Some(aSupported);
      break;
    }
    default:
      MOZ_CRASH("Not to be used for any other process");
  }
}

}  // namespace mozilla
