/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UtilityProcessHost.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/UtilityProcessManager.h"
#include "mozilla/Telemetry.h"

#include "chrome/common/process_watcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_general.h"

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
#  include "mozilla/Sandbox.h"
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxBrokerPolicyFactory.h"
#endif

#include "ProfilerParent.h"
#include "mozilla/PProfilerChild.h"

namespace mozilla::ipc {

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
bool UtilityProcessHost::sLaunchWithMacSandbox = false;
#endif

UtilityProcessHost::UtilityProcessHost(SandboxingKind aSandbox,
                                       RefPtr<Listener> aListener)
    : GeckoChildProcessHost(GeckoProcessType_Utility),
      mListener(std::move(aListener)),
      mLiveToken(new media::Refcountable<bool>(true)) {
  MOZ_COUNT_CTOR(UtilityProcessHost);
#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  if (!sLaunchWithMacSandbox) {
    sLaunchWithMacSandbox =
        (PR_GetEnv("MOZ_DISABLE_UTILITY_SANDBOX") == nullptr);
  }
  mDisableOSActivityMode = sLaunchWithMacSandbox;
#endif
#if defined(MOZ_SANDBOX)
  mSandbox = aSandbox;
#endif
}

UtilityProcessHost::~UtilityProcessHost() {
  MOZ_COUNT_DTOR(UtilityProcessHost);
}

bool UtilityProcessHost::Launch(StringVector aExtraOpts) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Unlaunched);
  MOZ_ASSERT(!mUtilityProcessParent);

  mPrefSerializer = MakeUnique<ipc::SharedPreferenceSerializer>();
  if (!mPrefSerializer->SerializeToSharedMemory(GeckoProcessType_Utility,
                                                /* remoteType */ ""_ns)) {
    return false;
  }
  mPrefSerializer->AddSharedPrefCmdLineArgs(*this, aExtraOpts);

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  mSandboxLevel = Preferences::GetInt("security.sandbox.utility.level");
#endif

  mLaunchPhase = LaunchPhase::Waiting;

  int32_t timeoutMs = StaticPrefs::general_utility_process_startup_timeout_ms();

  // If one of the following environment variables are set we can
  // effectively ignore the timeout - as we can guarantee the Utility
  // process will be terminated
  if (PR_GetEnv("MOZ_DEBUG_CHILD_PROCESS") ||
      PR_GetEnv("MOZ_DEBUG_CHILD_PAUSE")) {
    timeoutMs = 0;
  }
  if (timeoutMs) {
    // We queue a delayed task. If that task runs before the
    // WhenProcessHandleReady promise gets resolved, we will abort the launch.
    GetMainThreadSerialEventTarget()->DelayedDispatch(
        NS_NewRunnableFunction(
            "UtilityProcessHost::Launchtimeout",
            [this, liveToken = mLiveToken]() {
              if (!*liveToken || mTimerChecked) {
                // We have been deleted or the runnable has already started, we
                // can abort.
                return;
              }
              InitAfterConnect(false);
              MOZ_ASSERT(mTimerChecked,
                         "InitAfterConnect must have acted on the promise");
            }),
        timeoutMs);
  }

  if (!GeckoChildProcessHost::AsyncLaunch(aExtraOpts)) {
    NS_WARNING("UtilityProcess AsyncLaunch failed, aborting.");
    mLaunchPhase = LaunchPhase::Complete;
    mPrefSerializer = nullptr;
    return false;
  }
  return true;
}

RefPtr<GenericNonExclusivePromise> UtilityProcessHost::LaunchPromise() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mLaunchPromise) {
    return mLaunchPromise;
  }
  mLaunchPromise = MakeRefPtr<GenericNonExclusivePromise::Private>(__func__);
  WhenProcessHandleReady()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [this, liveToken = mLiveToken](
          const ipc::ProcessHandlePromise::ResolveOrRejectValue& aResult) {
        if (!*liveToken) {
          // The UtilityProcessHost got deleted. Abort. The promise would have
          // already been rejected.
          return;
        }
        if (mTimerChecked) {
          // We hit the timeout earlier, abort.
          return;
        }
        mTimerChecked = true;
        if (aResult.IsReject()) {
          RejectPromise();
        }
        // If aResult.IsResolve() then we have succeeded in launching the
        // Utility process. The promise will be resolved once the channel has
        // connected (or failed to) later.
      });
  return mLaunchPromise;
}

void UtilityProcessHost::OnChannelConnected(base::ProcessId peer_pid) {
  MOZ_ASSERT(!NS_IsMainThread());

  GeckoChildProcessHost::OnChannelConnected(peer_pid);

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "UtilityProcessHost::OnChannelConnected",
      [this, liveToken = mLiveToken]() {
        if (*liveToken && mLaunchPhase == LaunchPhase::Waiting) {
          InitAfterConnect(true);
        }
      }));
}

void UtilityProcessHost::OnChannelError() {
  MOZ_ASSERT(!NS_IsMainThread());

  GeckoChildProcessHost::OnChannelError();

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "UtilityProcessHost::OnChannelError", [this, liveToken = mLiveToken]() {
        if (*liveToken && mLaunchPhase == LaunchPhase::Waiting) {
          InitAfterConnect(false);
        }
      }));
}

void UtilityProcessHost::InitAfterConnect(bool aSucceeded) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Waiting);
  MOZ_ASSERT(!mUtilityProcessParent);

  mLaunchPhase = LaunchPhase::Complete;

  if (!aSucceeded) {
    RejectPromise();
    return;
  }

  mUtilityProcessParent = MakeRefPtr<UtilityProcessParent>(this);
  DebugOnly<bool> rv = TakeInitialEndpoint().Bind(mUtilityProcessParent.get());
  MOZ_ASSERT(rv);

  // Only clear mPrefSerializer in the success case to avoid a
  // possible race in the case case of a timeout on Windows launch.
  // See Bug 1555076 comment 7:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1555076#c7
  mPrefSerializer = nullptr;

  Maybe<FileDescriptor> brokerFd;

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  UniquePtr<SandboxBroker::Policy> policy;
  switch (mSandbox) {
    case SandboxingKind::GENERIC_UTILITY:
    case SandboxingKind::UTILITY_AUDIO_DECODING_GENERIC:
      policy = SandboxBrokerPolicyFactory::GetUtilityProcessPolicy(
          GetActor()->OtherPid());
      break;

    default:
      MOZ_ASSERT(false, "Invalid SandboxingKind");
      break;
  }
  if (policy != nullptr) {
    brokerFd = Some(FileDescriptor());
    mSandboxBroker = SandboxBroker::Create(
        std::move(policy), GetActor()->OtherPid(), brokerFd.ref());
    // This is unlikely to fail and probably indicates OS resource
    // exhaustion, but we can at least try to recover.
    Unused << NS_WARN_IF(mSandboxBroker == nullptr);
    MOZ_ASSERT(brokerFd.ref().IsValid());
  }
#endif  // XP_LINUX && MOZ_SANDBOX

  Unused << GetActor()->SendInit(brokerFd, Telemetry::CanRecordReleaseData());

  Unused << GetActor()->SendInitProfiler(
      ProfilerParent::CreateForProcess(GetActor()->OtherPid()));

  // Promise will be resolved later, from UtilityProcessParent when the child
  // will send the InitCompleted message.
}

void UtilityProcessHost::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mShutdownRequested);

  RejectPromise();

  if (mUtilityProcessParent) {
    // OnChannelClosed uses this to check if the shutdown was expected or
    // unexpected.
    mShutdownRequested = true;

    // The channel might already be closed if we got here unexpectedly.
    if (!mChannelClosed) {
      mUtilityProcessParent->Close();
    }

#ifndef NS_FREE_PERMANENT_DATA
    // No need to communicate shutdown, the Utility process doesn't need to
    // communicate anything back.
    KillHard("NormalShutdown");
#endif

    // If we're shutting down unexpectedly, we're in the middle of handling an
    // ActorDestroy for PUtilityProcessParent, which is still on the stack.
    // We'll return back to OnChannelClosed.
    //
    // Otherwise, we'll wait for OnChannelClose to be called whenever
    // PUtilityProcessParent acknowledges shutdown.
    return;
  }

  DestroyProcess();
}

void UtilityProcessHost::OnChannelClosed() {
  MOZ_ASSERT(NS_IsMainThread());

  mChannelClosed = true;
  RejectPromise();

  if (!mShutdownRequested && mListener) {
    // This is an unclean shutdown. Notify our listener that we're going away.
    mListener->OnProcessUnexpectedShutdown(this);
  }

  DestroyProcess();

  // Release the actor.
  UtilityProcessParent::Destroy(std::move(mUtilityProcessParent));
}

void UtilityProcessHost::KillHard(const char* aReason) {
  MOZ_ASSERT(NS_IsMainThread());

  ProcessHandle handle = GetChildProcessHandle();
  if (!base::KillProcess(handle, base::PROCESS_END_KILLED_BY_USER)) {
    NS_WARNING("failed to kill subprocess!");
  }

  SetAlreadyDead();
}

void UtilityProcessHost::DestroyProcess() {
  MOZ_ASSERT(NS_IsMainThread());
  RejectPromise();

  // Any pending tasks will be cancelled from now on.
  *mLiveToken = false;

  NS_DispatchToMainThread(
      NS_NewRunnableFunction("DestroyProcessRunnable", [this] { Destroy(); }));
}

void UtilityProcessHost::ResolvePromise() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mLaunchPromiseSettled) {
    mLaunchPromise->Resolve(true, __func__);
    mLaunchPromiseSettled = true;
  }
  // We have already acted on the promise; the timeout runnable no longer needs
  // to interrupt anything.
  mTimerChecked = true;
}

void UtilityProcessHost::RejectPromise() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mLaunchPromiseSettled) {
    mLaunchPromise->Reject(NS_ERROR_FAILURE, __func__);
    mLaunchPromiseSettled = true;
  }
  // We have already acted on the promise; the timeout runnable no longer needs
  // to interrupt anything.
  mTimerChecked = true;
}

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
bool UtilityProcessHost::FillMacSandboxInfo(MacSandboxInfo& aInfo) {
  GeckoChildProcessHost::FillMacSandboxInfo(aInfo);
  if (!aInfo.shouldLog && PR_GetEnv("MOZ_SANDBOX_UTILITY_LOGGING")) {
    aInfo.shouldLog = true;
  }
  return true;
}

/* static */
MacSandboxType UtilityProcessHost::GetMacSandboxType() {
  return MacSandboxType_Utility;
}
#endif

}  // namespace mozilla::ipc
