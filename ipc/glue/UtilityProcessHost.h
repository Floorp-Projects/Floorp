/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _include_ipc_glue_UtilityProcessHost_h_
#define _include_ipc_glue_UtilityProcessHost_h_

#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/UtilityProcessParent.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/ipc/ProcessUtils.h"

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxBroker.h"
#endif

namespace mozilla::ipc {

class UtilityProcessParent;

// UtilityProcessHost is the "parent process" container for a subprocess handle
// and IPC connection. It owns the parent process IPDL actor, which in this
// case, is a UtilityChild.
//
// UtilityProcessHosts are allocated and managed by UtilityProcessManager.
class UtilityProcessHost final : public mozilla::ipc::GeckoChildProcessHost {
  friend class UtilityProcessParent;

 public:
  class Listener {
   protected:
    virtual ~Listener() = default;

   public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UtilityProcessHost::Listener);

    // The UtilityProcessHost has unexpectedly shutdown or had its connection
    // severed. This is not called if an error occurs after calling
    // Shutdown().
    virtual void OnProcessUnexpectedShutdown(UtilityProcessHost* aHost) {}
  };

  explicit UtilityProcessHost(SandboxingKind aSandbox,
                              RefPtr<Listener> listener);

  // Launch the subprocess asynchronously. On failure, false is returned.
  // Otherwise, true is returned. If succeeded, a follow-up call should be made
  // to LaunchPromise() which will return a promise that will be resolved once
  // the Utility process has launched and a channel has been established.
  //
  // @param aExtraOpts (StringVector)
  //        Extra options to pass to the subprocess.
  bool Launch(StringVector aExtraOpts);

  // Return a promise that will be resolved once the process has completed its
  // launch. The promise will be immediately resolved if the launch has already
  // succeeded.
  RefPtr<GenericNonExclusivePromise> LaunchPromise();

  // Inform the process that it should clean up its resources and shut
  // down. This initiates an asynchronous shutdown sequence. After this
  // method returns, it is safe for the caller to forget its pointer to
  // the UtilityProcessHost.
  //
  // After this returns, the attached Listener is no longer used.
  void Shutdown();

  // Return the actor for the top-level actor of the process. If the process
  // has not connected yet, this returns null.
  RefPtr<UtilityProcessParent> GetActor() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mUtilityProcessParent;
  }

  bool IsConnected() const {
    MOZ_ASSERT(NS_IsMainThread());
    return bool(mUtilityProcessParent);
  }

  // Called on the IO thread.
  void OnChannelConnected(base::ProcessId peer_pid) override;

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  // Return the sandbox type to be used with this process type.
  static MacSandboxType GetMacSandboxType();
#endif

 private:
  ~UtilityProcessHost();

  // Called on the main thread with true after a connection has been established
  // or false if it failed (including if it failed before the timeout kicked in)
  void InitAfterConnect(bool aSucceeded);

  // Called on the main thread when the mUtilityProcessParent actor is shutting
  // down.
  void OnChannelClosed();

  // Kill the remote process, triggering IPC shutdown.
  void KillHard(const char* aReason);

  void DestroyProcess();

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  static bool sLaunchWithMacSandbox;

  // Sandbox the Utility process at launch for all instances
  bool IsMacSandboxLaunchEnabled() override { return sLaunchWithMacSandbox; }

  // Override so we can turn on Utility process-specific sandbox logging
  bool FillMacSandboxInfo(MacSandboxInfo& aInfo) override;
#endif

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessHost);

  RefPtr<Listener> mListener;

  // All members below are only ever accessed on the main thread.
  enum class LaunchPhase { Unlaunched, Waiting, Complete };
  LaunchPhase mLaunchPhase = LaunchPhase::Unlaunched;

  RefPtr<UtilityProcessParent> mUtilityProcessParent;

  UniquePtr<ipc::SharedPreferenceSerializer> mPrefSerializer{};

  bool mShutdownRequested = false;

  void RejectPromise();
  void ResolvePromise();

#if defined(MOZ_WMF_CDM) && defined(MOZ_SANDBOX) && !defined(MOZ_ASAN)
  void EnsureWidevineL1PathForSandbox(StringVector& aExtraOpts);
#endif

  // Set to true on construction and to false just prior deletion.
  // The UtilityProcessHost isn't refcounted; so we can capture this by value in
  // lambdas along with a strong reference to mLiveToken and check if that value
  // is true before accessing "this".
  // While a reference to mLiveToken can be taken on any thread; its value can
  // only be read or written on the main thread.
  const RefPtr<media::Refcountable<bool>> mLiveToken;

  RefPtr<GenericNonExclusivePromise::Private> mLaunchPromise{};
  bool mLaunchPromiseSettled = false;
  bool mLaunchPromiseLaunched = false;
  // Will be set to true if the Utility process as successfully started.
  bool mLaunchCompleted = false;

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  UniquePtr<SandboxBroker> mSandboxBroker{};
#endif
};

}  // namespace mozilla::ipc

#endif  // _include_ipc_glue_UtilityProcessHost_h_
