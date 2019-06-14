/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _include_dom_media_ipc_RDDProcessHost_h_
#define _include_dom_media_ipc_RDDProcessHost_h_
#include "mozilla/ipc/GeckoChildProcessHost.h"

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/TaskFactory.h"

namespace mozilla {
namespace ipc {
class SharedPreferenceSerializer;
}
}  // namespace mozilla
class nsITimer;

namespace mozilla {

class RDDChild;

// RDDProcessHost is the "parent process" container for a subprocess handle and
// IPC connection. It owns the parent process IPDL actor, which in this case,
// is a RDDChild.
//
// RDDProcessHosts are allocated and managed by RDDProcessManager. For all
// intents and purposes it is a singleton, though more than one may be allocated
// at a time due to its shutdown being asynchronous.
class RDDProcessHost final : public mozilla::ipc::GeckoChildProcessHost {
  friend class RDDChild;

 public:
  class Listener {
   public:
    virtual void OnProcessLaunchComplete(RDDProcessHost* aHost) {}

    // The RDDProcessHost has unexpectedly shutdown or had its connection
    // severed. This is not called if an error occurs after calling
    // Shutdown().
    virtual void OnProcessUnexpectedShutdown(RDDProcessHost* aHost) {}
  };

  explicit RDDProcessHost(Listener* listener);

  // Launch the subprocess asynchronously. On failure, false is returned.
  // Otherwise, true is returned, and the OnProcessLaunchComplete listener
  // callback will be invoked either when a connection has been established, or
  // if a connection could not be established due to an asynchronous error.
  //
  // @param aExtraOpts (StringVector)
  //        Extra options to pass to the subprocess.
  bool Launch(StringVector aExtraOpts);

  // If the process is being launched, block until it has launched and
  // connected. If a launch task is pending, it will fire immediately.
  //
  // Returns true if the process is successfully connected; false otherwise.
  bool WaitForLaunch();

  // Inform the process that it should clean up its resources and shut
  // down. This initiates an asynchronous shutdown sequence. After this
  // method returns, it is safe for the caller to forget its pointer to
  // the RDDProcessHost.
  //
  // After this returns, the attached Listener is no longer used.
  void Shutdown();

  // Return the actor for the top-level actor of the process. If the process
  // has not connected yet, this returns null.
  RDDChild* GetActor() const { return mRDDChild.get(); }

  // Return a unique id for this process, guaranteed not to be shared with any
  // past or future instance of RDDProcessHost.
  uint64_t GetProcessToken() const;

  bool IsConnected() const { return !!mRDDChild; }

  // Return the time stamp for when we tried to launch the RDD process.
  // This is currently used for Telemetry so that we can determine how
  // long RDD processes take to spin up. Note this doesn't denote a
  // successful launch, just when we attempted launch.
  TimeStamp GetLaunchTime() const { return mLaunchTime; }

  // Called on the IO thread.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  void SetListener(Listener* aListener);

  // Used for tests and diagnostics
  void KillProcess();

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  // To allow filling a MacSandboxInfo from the child
  // process without an instance of RDDProcessHost.
  // Only needed for late-start sandbox enabling.
  static void StaticFillMacSandboxInfo(MacSandboxInfo& aInfo);

  // Return the sandbox type to be used with this process type.
  static MacSandboxType GetMacSandboxType();
#endif

 private:
  ~RDDProcessHost();

  // Called on the main thread.
  void OnChannelConnectedTask();
  void OnChannelErrorTask();

  // Called on the main thread after a connection has been established.
  void InitAfterConnect(bool aSucceeded);

  // Called on the main thread when the mRDDChild actor is shutting down.
  void OnChannelClosed();

  // Kill the remote process, triggering IPC shutdown.
  void KillHard(const char* aReason);

  void DestroyProcess();

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  static bool sLaunchWithMacSandbox;

  // Sandbox the RDD process at launch for all instances
  bool IsMacSandboxLaunchEnabled() override { return sLaunchWithMacSandbox; }

  // Override so we can turn on RDD process-specific sandbox logging
  void FillMacSandboxInfo(MacSandboxInfo& aInfo) override;
#endif

  DISALLOW_COPY_AND_ASSIGN(RDDProcessHost);

  Listener* mListener;
  mozilla::ipc::TaskFactory<RDDProcessHost> mTaskFactory;

  enum class LaunchPhase { Unlaunched, Waiting, Complete };
  LaunchPhase mLaunchPhase;

  UniquePtr<RDDChild> mRDDChild;
  uint64_t mProcessToken;

  UniquePtr<ipc::SharedPreferenceSerializer> mPrefSerializer;

  bool mShutdownRequested;
  bool mChannelClosed;

  TimeStamp mLaunchTime;
};

}  // namespace mozilla

#endif  // _include_dom_media_ipc_RDDProcessHost_h_
