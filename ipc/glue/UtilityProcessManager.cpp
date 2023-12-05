/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "UtilityProcessManager.h"

#include "JSOracleParent.h"
#include "mozilla/ipc/UtilityProcessHost.h"
#include "mozilla/MemoryReportingProcess.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"  // for LaunchUtilityProcess
#include "mozilla/ipc/UtilityProcessParent.h"
#include "mozilla/ipc/UtilityAudioDecoderChild.h"
#include "mozilla/ipc/UtilityAudioDecoderParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "mozilla/ipc/ProcessChild.h"
#include "nsAppRunner.h"
#include "nsContentUtils.h"

#ifdef XP_WIN
#  include "mozilla/dom/WindowsUtilsParent.h"
#  include "mozilla/widget/filedialog/WinFileDialogParent.h"
#endif

#include "mozilla/GeckoArgs.h"

namespace mozilla::ipc {

extern LazyLogModule gUtilityProcessLog;
#define LOGD(...) MOZ_LOG(gUtilityProcessLog, LogLevel::Debug, (__VA_ARGS__))

static StaticRefPtr<UtilityProcessManager> sSingleton;

static bool sXPCOMShutdown = false;

bool UtilityProcessManager::IsShutdown() const {
  MOZ_ASSERT(NS_IsMainThread());
  return sXPCOMShutdown || !sSingleton;
}

RefPtr<UtilityProcessManager> UtilityProcessManager::GetSingleton() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!sXPCOMShutdown && sSingleton == nullptr) {
    sSingleton = new UtilityProcessManager();
    sSingleton->Init();
  }
  return sSingleton;
}

RefPtr<UtilityProcessManager> UtilityProcessManager::GetIfExists() {
  MOZ_ASSERT(NS_IsMainThread());
  return sSingleton;
}

UtilityProcessManager::UtilityProcessManager() {
  LOGD("[%p] UtilityProcessManager::UtilityProcessManager", this);
}

void UtilityProcessManager::Init() {
  // Start listening for pref changes so we can
  // forward them to the process once it is running.
  mObserver = new Observer(this);
  nsContentUtils::RegisterShutdownObserver(mObserver);
  Preferences::AddStrongObserver(mObserver, "");
}

UtilityProcessManager::~UtilityProcessManager() {
  LOGD("[%p] UtilityProcessManager::~UtilityProcessManager", this);

  // The Utility process should ALL have already been shut down.
  MOZ_ASSERT(NoMoreProcesses());
}

NS_IMPL_ISUPPORTS(UtilityProcessManager::Observer, nsIObserver);

UtilityProcessManager::Observer::Observer(UtilityProcessManager* aManager)
    : mManager(aManager) {}

NS_IMETHODIMP
UtilityProcessManager::Observer::Observe(nsISupports* aSubject,
                                         const char* aTopic,
                                         const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    mManager->OnXPCOMShutdown();
  } else if (!strcmp(aTopic, "nsPref:changed")) {
    mManager->OnPreferenceChange(aData);
  }
  return NS_OK;
}

void UtilityProcessManager::OnXPCOMShutdown() {
  LOGD("[%p] UtilityProcessManager::OnXPCOMShutdown", this);

  MOZ_ASSERT(NS_IsMainThread());
  sXPCOMShutdown = true;
  nsContentUtils::UnregisterShutdownObserver(mObserver);
  CleanShutdownAllProcesses();
}

void UtilityProcessManager::OnPreferenceChange(const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());
  if (NoMoreProcesses()) {
    // Process hasn't been launched yet
    return;
  }
  // We know prefs are ASCII here.
  NS_LossyConvertUTF16toASCII strData(aData);

  mozilla::dom::Pref pref(strData, /* isLocked */ false,
                          /* isSanitized */ false, Nothing(), Nothing());
  Preferences::GetPreference(&pref, GeckoProcessType_Utility,
                             /* remoteType */ ""_ns);

  for (auto& p : mProcesses) {
    if (!p) {
      continue;
    }

    if (p->mProcessParent) {
      Unused << p->mProcessParent->SendPreferenceUpdate(pref);
    } else if (IsProcessLaunching(p->mSandbox)) {
      p->mQueuedPrefs.AppendElement(pref);
    }
  }
}

RefPtr<UtilityProcessManager::ProcessFields> UtilityProcessManager::GetProcess(
    SandboxingKind aSandbox) {
  if (!mProcesses[aSandbox]) {
    return nullptr;
  }

  return mProcesses[aSandbox];
}

RefPtr<GenericNonExclusivePromise> UtilityProcessManager::LaunchProcess(
    SandboxingKind aSandbox) {
  LOGD("[%p] UtilityProcessManager::LaunchProcess SandboxingKind=%" PRIu64,
       this, aSandbox);

  MOZ_ASSERT(NS_IsMainThread());

  if (IsShutdown()) {
    NS_WARNING("Reject early LaunchProcess() for Shutdown");
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                       __func__);
  }

  RefPtr<ProcessFields> p = GetProcess(aSandbox);
  if (p && p->mNumProcessAttempts) {
    // We failed to start the Utility process earlier, abort now.
    NS_WARNING("Reject LaunchProcess() for earlier mNumProcessAttempts");
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                       __func__);
  }

  if (p && p->mLaunchPromise && p->mProcess) {
    return p->mLaunchPromise;
  }

  if (!p) {
    p = new ProcessFields(aSandbox);
    mProcesses[aSandbox] = p;
  }

  std::vector<std::string> extraArgs;
  ProcessChild::AddPlatformBuildID(extraArgs);
  geckoargs::sSandboxingKind.Put(aSandbox, extraArgs);

  // The subprocess is launched asynchronously, so we
  // wait for the promise to be resolved to acquire the IPDL actor.
  p->mProcess = new UtilityProcessHost(aSandbox, this);
  if (!p->mProcess->Launch(extraArgs)) {
    p->mNumProcessAttempts++;
    DestroyProcess(aSandbox);
    NS_WARNING("Reject LaunchProcess() for mNumProcessAttempts++");
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                       __func__);
  }

  RefPtr<UtilityProcessManager> self = this;
  p->mLaunchPromise = p->mProcess->LaunchPromise()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self, p, aSandbox](bool) {
        if (self->IsShutdown()) {
          NS_WARNING(
              "Reject LaunchProcess() after LaunchPromise() for Shutdown");
          return GenericNonExclusivePromise::CreateAndReject(
              NS_ERROR_NOT_AVAILABLE, __func__);
        }

        if (self->IsProcessDestroyed(aSandbox)) {
          NS_WARNING(
              "Reject LaunchProcess() after LaunchPromise() for destroyed "
              "process");
          return GenericNonExclusivePromise::CreateAndReject(
              NS_ERROR_NOT_AVAILABLE, __func__);
        }

        p->mProcessParent = p->mProcess->GetActor();

        // Flush any pref updates that happened during
        // launch and weren't included in the blobs set
        // up in LaunchUtilityProcess.
        for (const mozilla::dom::Pref& pref : p->mQueuedPrefs) {
          Unused << NS_WARN_IF(!p->mProcessParent->SendPreferenceUpdate(pref));
        }
        p->mQueuedPrefs.Clear();

        CrashReporter::AnnotateCrashReport(
            CrashReporter::Annotation::UtilityProcessStatus, "Running"_ns);

        return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
      },
      [self, p, aSandbox](nsresult aError) {
        if (GetSingleton()) {
          p->mNumProcessAttempts++;
          self->DestroyProcess(aSandbox);
        }
        NS_WARNING("Reject LaunchProcess() for LaunchPromise() rejection");
        return GenericNonExclusivePromise::CreateAndReject(aError, __func__);
      });

  return p->mLaunchPromise;
}

template <typename Actor>
RefPtr<GenericNonExclusivePromise> UtilityProcessManager::StartUtility(
    RefPtr<Actor> aActor, SandboxingKind aSandbox) {
  LOGD(
      "[%p] UtilityProcessManager::StartUtility actor=%p "
      "SandboxingKind=%" PRIu64,
      this, aActor.get(), aSandbox);

  TimeStamp utilityStart = TimeStamp::Now();

  if (!aActor) {
    MOZ_ASSERT(false, "Actor singleton failure");
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
  }

  if (aActor->CanSend()) {
    // Actor has already been setup, so we:
    //   - know the process has been launched
    //   - the ipc actors are ready
    PROFILER_MARKER_TEXT(
        "UtilityProcessManager::StartUtility", IPC,
        MarkerOptions(MarkerTiming::InstantNow()),
        nsPrintfCString("SandboxingKind=%" PRIu64 " aActor->CanSend()",
                        aSandbox));
    return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
  }

  RefPtr<UtilityProcessManager> self = this;
  return LaunchProcess(aSandbox)->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self, aActor, aSandbox, utilityStart]() {
        RefPtr<UtilityProcessParent> utilityParent =
            self->GetProcessParent(aSandbox);
        if (!utilityParent) {
          NS_WARNING("Missing parent in StartUtility");
          return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                             __func__);
        }

        // It is possible if multiple processes concurrently request a utility
        // actor that the previous CanSend() check returned false for both but
        // that by the time we have started our process for real, one of them
        // has already been able to establish the IPC connection and thus we
        // would perform more than one Open() call.
        //
        // The tests within browser_utility_multipleAudio.js should be able to
        // catch that behavior.
        if (!aActor->CanSend()) {
          nsresult rv = aActor->BindToUtilityProcess(utilityParent);
          if (NS_FAILED(rv)) {
            MOZ_ASSERT(false, "Protocol endpoints failure");
            return GenericNonExclusivePromise::CreateAndReject(rv, __func__);
          }

          MOZ_DIAGNOSTIC_ASSERT(aActor->CanSend(), "IPC established for actor");
          self->RegisterActor(utilityParent, aActor->GetActorName());
        }

        PROFILER_MARKER_TEXT(
            "UtilityProcessManager::StartUtility", IPC,
            MarkerOptions(MarkerTiming::IntervalUntilNowFrom(utilityStart)),
            nsPrintfCString("SandboxingKind=%" PRIu64 " Resolve", aSandbox));
        return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
      },
      [self, aSandbox, utilityStart](nsresult aError) {
        NS_WARNING("Reject StartUtility() for LaunchProcess() rejection");
        if (!self->IsShutdown()) {
          NS_WARNING("Reject StartUtility() when !IsShutdown()");
        }
        PROFILER_MARKER_TEXT(
            "UtilityProcessManager::StartUtility", IPC,
            MarkerOptions(MarkerTiming::IntervalUntilNowFrom(utilityStart)),
            nsPrintfCString("SandboxingKind=%" PRIu64 " Reject", aSandbox));
        return GenericNonExclusivePromise::CreateAndReject(aError, __func__);
      });
}

RefPtr<UtilityProcessManager::StartRemoteDecodingUtilityPromise>
UtilityProcessManager::StartProcessForRemoteMediaDecoding(
    base::ProcessId aOtherProcess, dom::ContentParentId aChildId,
    SandboxingKind aSandbox) {
  // Not supported kinds.
  if (aSandbox != SandboxingKind::GENERIC_UTILITY
#ifdef MOZ_APPLEMEDIA
      && aSandbox != SandboxingKind::UTILITY_AUDIO_DECODING_APPLE_MEDIA
#endif
#ifdef XP_WIN
      && aSandbox != SandboxingKind::UTILITY_AUDIO_DECODING_WMF
#endif
#ifdef MOZ_WMF_MEDIA_ENGINE
      && aSandbox != SandboxingKind::MF_MEDIA_ENGINE_CDM
#endif
  ) {
    return StartRemoteDecodingUtilityPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                              __func__);
  }
  TimeStamp remoteDecodingStart = TimeStamp::Now();

  RefPtr<UtilityProcessManager> self = this;
  RefPtr<UtilityAudioDecoderChild> uadc =
      UtilityAudioDecoderChild::GetSingleton(aSandbox);
  MOZ_ASSERT(uadc, "Unable to get a singleton for UtilityAudioDecoderChild");
  return StartUtility(uadc, aSandbox)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self, uadc, aOtherProcess, aChildId, aSandbox,
           remoteDecodingStart]() {
            RefPtr<UtilityProcessParent> parent =
                self->GetProcessParent(aSandbox);
            if (!parent) {
              NS_WARNING("UtilityAudioDecoderParent lost in the middle");
              return StartRemoteDecodingUtilityPromise::CreateAndReject(
                  NS_ERROR_FAILURE, __func__);
            }

            if (!uadc->CanSend()) {
              NS_WARNING("UtilityAudioDecoderChild lost in the middle");
              return StartRemoteDecodingUtilityPromise::CreateAndReject(
                  NS_ERROR_FAILURE, __func__);
            }

            base::ProcessId process = parent->OtherPid();

            Endpoint<PRemoteDecoderManagerChild> childPipe;
            Endpoint<PRemoteDecoderManagerParent> parentPipe;
            nsresult rv = PRemoteDecoderManager::CreateEndpoints(
                process, aOtherProcess, &parentPipe, &childPipe);
            if (NS_FAILED(rv)) {
              MOZ_ASSERT(false, "Could not create content remote decoder");
              return StartRemoteDecodingUtilityPromise::CreateAndReject(
                  rv, __func__);
            }

            if (!uadc->SendNewContentRemoteDecoderManager(std::move(parentPipe),
                                                          aChildId)) {
              MOZ_ASSERT(false, "SendNewContentRemoteDecoderManager failure");
              return StartRemoteDecodingUtilityPromise::CreateAndReject(
                  NS_ERROR_FAILURE, __func__);
            }

#ifdef MOZ_WMF_MEDIA_ENGINE
            if (aSandbox == SandboxingKind::MF_MEDIA_ENGINE_CDM &&
                !uadc->CreateVideoBridge()) {
              MOZ_ASSERT(false, "Failed to create video bridge");
              return StartRemoteDecodingUtilityPromise::CreateAndReject(
                  NS_ERROR_FAILURE, __func__);
            }
#endif
            PROFILER_MARKER_TEXT(
                "UtilityProcessManager::StartProcessForRemoteMediaDecoding",
                MEDIA,
                MarkerOptions(
                    MarkerTiming::IntervalUntilNowFrom(remoteDecodingStart)),
                "Resolve"_ns);
            return StartRemoteDecodingUtilityPromise::CreateAndResolve(
                std::move(childPipe), __func__);
          },
          [self, remoteDecodingStart](nsresult aError) {
            NS_WARNING(
                "Reject StartProcessForRemoteMediaDecoding() for "
                "StartUtility() rejection");
            PROFILER_MARKER_TEXT(
                "UtilityProcessManager::StartProcessForRemoteMediaDecoding",
                MEDIA,
                MarkerOptions(
                    MarkerTiming::IntervalUntilNowFrom(remoteDecodingStart)),
                "Reject"_ns);
            return StartRemoteDecodingUtilityPromise::CreateAndReject(aError,
                                                                      __func__);
          });
}

RefPtr<UtilityProcessManager::JSOraclePromise>
UtilityProcessManager::StartJSOracle(dom::JSOracleParent* aParent) {
  return StartUtility(RefPtr{aParent}, SandboxingKind::GENERIC_UTILITY);
}

#ifdef XP_WIN

// Windows Utils

RefPtr<UtilityProcessManager::WindowsUtilsPromise>
UtilityProcessManager::GetWindowsUtilsPromise() {
  TimeStamp windowsUtilsStart = TimeStamp::Now();
  RefPtr<UtilityProcessManager> self = this;
  if (!mWindowsUtils) {
    mWindowsUtils = new dom::WindowsUtilsParent();
  }

  RefPtr<dom::WindowsUtilsParent> wup = mWindowsUtils;
  MOZ_ASSERT(wup, "Unable to get a singleton for WindowsUtils");
  return StartUtility(wup, SandboxingKind::WINDOWS_UTILS)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self, wup, windowsUtilsStart]() {
            if (!wup->CanSend()) {
              MOZ_ASSERT(false, "WindowsUtilsParent can't send");
              return WindowsUtilsPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                          __func__);
            }
            PROFILER_MARKER_TEXT(
                "UtilityProcessManager::GetWindowsUtilsPromise", OTHER,
                MarkerOptions(
                    MarkerTiming::IntervalUntilNowFrom(windowsUtilsStart)),
                "Resolve"_ns);
            return WindowsUtilsPromise::CreateAndResolve(wup, __func__);
          },
          [self, windowsUtilsStart](nsresult aError) {
            NS_WARNING("StartUtility rejected promise for PWindowsUtils");
            PROFILER_MARKER_TEXT(
                "UtilityProcessManager::GetWindowsUtilsPromise", OTHER,
                MarkerOptions(
                    MarkerTiming::IntervalUntilNowFrom(windowsUtilsStart)),
                "Reject"_ns);
            return WindowsUtilsPromise::CreateAndReject(aError, __func__);
          });
}

void UtilityProcessManager::ReleaseWindowsUtils() { mWindowsUtils = nullptr; }

RefPtr<UtilityProcessManager::WinFileDialogPromise>
UtilityProcessManager::CreateWinFileDialogAsync() {
  using Promise = WinFileDialogPromise;
  TimeStamp startTime = TimeStamp::Now();
  auto wfdp = MakeRefPtr<widget::filedialog::WinFileDialogParent>();

  return StartUtility(wfdp, SandboxingKind::WINDOWS_FILE_DIALOG)
      ->Then(
          GetMainThreadSerialEventTarget(), __PRETTY_FUNCTION__,
          [wfdp, startTime]() mutable {
            LOGD("CreateWinFileDialogAsync() resolve: wfdp = [%p]", wfdp.get());
            if (!wfdp->CanSend()) {
              MOZ_ASSERT(false, "WinFileDialogParent can't send");
              return Promise::CreateAndReject(NS_ERROR_FAILURE,
                                              __PRETTY_FUNCTION__);
            }
            PROFILER_MARKER_TEXT(
                "UtilityProcessManager::CreateWinFileDialogAsync", OTHER,
                MarkerOptions(MarkerTiming::IntervalUntilNowFrom(startTime)),
                "Resolve"_ns);

            return Promise::CreateAndResolve(
                widget::filedialog::ProcessProxy(std::move(wfdp)),
                __PRETTY_FUNCTION__);
          },
          [self = RefPtr(this), startTime](nsresult error) {
            LOGD("CreateWinFileDialogAsync() reject");
            if (!self->IsShutdown()) {
              MOZ_ASSERT_UNREACHABLE("failure when starting file-dialog actor");
            }
            PROFILER_MARKER_TEXT(
                "UtilityProcessManager::CreateWinFileDialogAsync", OTHER,
                MarkerOptions(MarkerTiming::IntervalUntilNowFrom(startTime)),
                "Reject"_ns);

            return Promise::CreateAndReject(error, __PRETTY_FUNCTION__);
          });
}

#endif  // XP_WIN

bool UtilityProcessManager::IsProcessLaunching(SandboxingKind aSandbox) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ProcessFields> p = GetProcess(aSandbox);
  if (!p) {
    MOZ_CRASH("Cannot check process launching with no process");
    return false;
  }

  return p->mProcess && !(p->mProcessParent);
}

bool UtilityProcessManager::IsProcessDestroyed(SandboxingKind aSandbox) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ProcessFields> p = GetProcess(aSandbox);
  if (!p) {
    MOZ_CRASH("Cannot check process destroyed with no process");
    return false;
  }
  return !p->mProcess && !p->mProcessParent;
}

void UtilityProcessManager::OnProcessUnexpectedShutdown(
    UtilityProcessHost* aHost) {
  MOZ_ASSERT(NS_IsMainThread());

  for (auto& it : mProcesses) {
    if (it && it->mProcess && it->mProcess == aHost) {
      it->mNumUnexpectedCrashes++;
      DestroyProcess(it->mSandbox);
      return;
    }
  }

  MOZ_CRASH(
      "Called UtilityProcessManager::OnProcessUnexpectedShutdown with invalid "
      "aHost");
}

void UtilityProcessManager::CleanShutdownAllProcesses() {
  LOGD("[%p] UtilityProcessManager::CleanShutdownAllProcesses", this);

  for (auto& it : mProcesses) {
    if (it) {
      DestroyProcess(it->mSandbox);
    }
  }
}

void UtilityProcessManager::CleanShutdown(SandboxingKind aSandbox) {
  LOGD("[%p] UtilityProcessManager::CleanShutdown SandboxingKind=%" PRIu64,
       this, aSandbox);

  DestroyProcess(aSandbox);
}

uint16_t UtilityProcessManager::AliveProcesses() {
  uint16_t alive = 0;
  for (auto& p : mProcesses) {
    if (p != nullptr) {
      alive++;
    }
  }
  return alive;
}

bool UtilityProcessManager::NoMoreProcesses() { return AliveProcesses() == 0; }

void UtilityProcessManager::DestroyProcess(SandboxingKind aSandbox) {
  LOGD("[%p] UtilityProcessManager::DestroyProcess SandboxingKind=%" PRIu64,
       this, aSandbox);

  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (AliveProcesses() <= 1) {
    if (mObserver) {
      Preferences::RemoveObserver(mObserver, "");
    }

    mObserver = nullptr;
  }

  RefPtr<ProcessFields> p = GetProcess(aSandbox);
  if (!p) {
    return;
  }

  p->mQueuedPrefs.Clear();
  p->mProcessParent = nullptr;

  if (!p->mProcess) {
    return;
  }

  p->mProcess->Shutdown();
  p->mProcess = nullptr;

  mProcesses[aSandbox] = nullptr;

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::UtilityProcessStatus, "Destroyed"_ns);

  if (NoMoreProcesses()) {
    sSingleton = nullptr;
  }
}

Maybe<base::ProcessId> UtilityProcessManager::ProcessPid(
    SandboxingKind aSandbox) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ProcessFields> p = GetProcess(aSandbox);
  if (!p) {
    return Nothing();
  }
  if (p->mProcessParent) {
    return Some(p->mProcessParent->OtherPid());
  }
  return Nothing();
}

class UtilityMemoryReporter : public MemoryReportingProcess {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UtilityMemoryReporter, override)

  explicit UtilityMemoryReporter(UtilityProcessParent* aParent) {
    mParent = aParent;
  }

  bool IsAlive() const override { return bool(GetParent()); }

  bool SendRequestMemoryReport(
      const uint32_t& aGeneration, const bool& aAnonymize,
      const bool& aMinimizeMemoryUsage,
      const Maybe<ipc::FileDescriptor>& aDMDFile) override {
    RefPtr<UtilityProcessParent> parent = GetParent();
    if (!parent) {
      return false;
    }

    return parent->SendRequestMemoryReport(aGeneration, aAnonymize,
                                           aMinimizeMemoryUsage, aDMDFile);
  }

  int32_t Pid() const override {
    if (RefPtr<UtilityProcessParent> parent = GetParent()) {
      return (int32_t)parent->OtherPid();
    }
    return 0;
  }

 private:
  RefPtr<UtilityProcessParent> GetParent() const { return mParent; }

  RefPtr<UtilityProcessParent> mParent = nullptr;

 protected:
  ~UtilityMemoryReporter() = default;
};

RefPtr<MemoryReportingProcess> UtilityProcessManager::GetProcessMemoryReporter(
    UtilityProcessParent* parent) {
  return new UtilityMemoryReporter(parent);
}

}  // namespace mozilla::ipc
