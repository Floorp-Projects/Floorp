/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RDDProcessHost.h"

#include "chrome/common/process_watcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs.h"

#include "RDDChild.h"

namespace mozilla {

using namespace ipc;

RDDProcessHost::RDDProcessHost(Listener* aListener)
    : GeckoChildProcessHost(GeckoProcessType_RDD),
      mListener(aListener),
      mTaskFactory(this),
      mLaunchPhase(LaunchPhase::Unlaunched),
      mProcessToken(0),
      mShutdownRequested(false),
      mChannelClosed(false) {
  MOZ_COUNT_CTOR(RDDProcessHost);
}

RDDProcessHost::~RDDProcessHost() { MOZ_COUNT_DTOR(RDDProcessHost); }

bool RDDProcessHost::Launch(StringVector aExtraOpts) {
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Unlaunched);
  MOZ_ASSERT(!mRDDChild);

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  mSandboxLevel = Preferences::GetInt("security.sandbox.rdd.level");
#endif

  mLaunchPhase = LaunchPhase::Waiting;
  mLaunchTime = TimeStamp::Now();

  if (!GeckoChildProcessHost::AsyncLaunch(aExtraOpts)) {
    mLaunchPhase = LaunchPhase::Complete;
    return false;
  }
  return true;
}

bool RDDProcessHost::WaitForLaunch() {
  if (mLaunchPhase == LaunchPhase::Complete) {
    return !!mRDDChild;
  }

  int32_t timeoutMs = StaticPrefs::MediaRddProcessStartupTimeoutMs();

  // If one of the following environment variables are set we can
  // effectively ignore the timeout - as we can guarantee the RDD
  // process will be terminated
  if (PR_GetEnv("MOZ_DEBUG_CHILD_PROCESS") ||
      PR_GetEnv("MOZ_DEBUG_CHILD_PAUSE")) {
    timeoutMs = 0;
  }

  // Our caller expects the connection to be finished after we return, so we
  // immediately set up the IPDL actor and fire callbacks. The IO thread will
  // still dispatch a notification to the main thread - we'll just ignore it.
  bool result = GeckoChildProcessHost::WaitUntilConnected(timeoutMs);
  InitAfterConnect(result);
  return result;
}

void RDDProcessHost::OnChannelConnected(int32_t peer_pid) {
  MOZ_ASSERT(!NS_IsMainThread());

  GeckoChildProcessHost::OnChannelConnected(peer_pid);

  // Post a task to the main thread. Take the lock because mTaskFactory is not
  // thread-safe.
  RefPtr<Runnable> runnable;
  {
    MonitorAutoLock lock(mMonitor);
    runnable =
        mTaskFactory.NewRunnableMethod(&RDDProcessHost::OnChannelConnectedTask);
  }
  NS_DispatchToMainThread(runnable);
}

void RDDProcessHost::OnChannelError() {
  MOZ_ASSERT(!NS_IsMainThread());

  GeckoChildProcessHost::OnChannelError();

  // Post a task to the main thread. Take the lock because mTaskFactory is not
  // thread-safe.
  RefPtr<Runnable> runnable;
  {
    MonitorAutoLock lock(mMonitor);
    runnable =
        mTaskFactory.NewRunnableMethod(&RDDProcessHost::OnChannelErrorTask);
  }
  NS_DispatchToMainThread(runnable);
}

void RDDProcessHost::OnChannelConnectedTask() {
  if (mLaunchPhase == LaunchPhase::Waiting) {
    InitAfterConnect(true);
  }
}

void RDDProcessHost::OnChannelErrorTask() {
  if (mLaunchPhase == LaunchPhase::Waiting) {
    InitAfterConnect(false);
  }
}

static uint64_t sRDDProcessTokenCounter = 0;

void RDDProcessHost::InitAfterConnect(bool aSucceeded) {
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Waiting);
  MOZ_ASSERT(!mRDDChild);

  mLaunchPhase = LaunchPhase::Complete;

  if (aSucceeded) {
    mProcessToken = ++sRDDProcessTokenCounter;
    mRDDChild = MakeUnique<RDDChild>(this);
    DebugOnly<bool> rv =
        mRDDChild->Open(GetChannel(), base::GetProcId(GetChildProcessHandle()));
    MOZ_ASSERT(rv);

    if (!mRDDChild->Init()) {
      // Can't just kill here because it will create a timing race that
      // will crash the tab. We don't really want to crash the tab just
      // because RDD linux sandbox failed to initialize.  In this case,
      // we'll close the child channel which will cause the RDD process
      // to shutdown nicely avoiding the tab crash (which manifests as
      // Bug 1535335).
      mRDDChild->Close();
      return;
    }
  }

  if (mListener) {
    mListener->OnProcessLaunchComplete(this);
  }
}

void RDDProcessHost::Shutdown() {
  MOZ_ASSERT(!mShutdownRequested);

  mListener = nullptr;

  if (mRDDChild) {
    // OnChannelClosed uses this to check if the shutdown was expected or
    // unexpected.
    mShutdownRequested = true;

    // The channel might already be closed if we got here unexpectedly.
    if (!mChannelClosed) {
      mRDDChild->Close();
    }

#ifndef NS_FREE_PERMANENT_DATA
    // No need to communicate shutdown, the RDD process doesn't need to
    // communicate anything back.
    KillHard("NormalShutdown");
#endif

    // If we're shutting down unexpectedly, we're in the middle of handling an
    // ActorDestroy for PRDDChild, which is still on the stack. We'll return
    // back to OnChannelClosed.
    //
    // Otherwise, we'll wait for OnChannelClose to be called whenever PRDDChild
    // acknowledges shutdown.
    return;
  }

  DestroyProcess();
}

void RDDProcessHost::OnChannelClosed() {
  mChannelClosed = true;

  if (!mShutdownRequested && mListener) {
    // This is an unclean shutdown. Notify our listener that we're going away.
    mListener->OnProcessUnexpectedShutdown(this);
  } else {
    DestroyProcess();
  }

  // Release the actor.
  RDDChild::Destroy(std::move(mRDDChild));
  MOZ_ASSERT(!mRDDChild);
}

void RDDProcessHost::KillHard(const char* aReason) {
  ProcessHandle handle = GetChildProcessHandle();
  if (!base::KillProcess(handle, base::PROCESS_END_KILLED_BY_USER, false)) {
    NS_WARNING("failed to kill subprocess!");
  }

  SetAlreadyDead();
}

uint64_t RDDProcessHost::GetProcessToken() const { return mProcessToken; }

void RDDProcessHost::KillProcess() { KillHard("DiagnosticKill"); }

void RDDProcessHost::DestroyProcess() {
  // Cancel all tasks. We don't want anything triggering after our caller
  // expects this to go away.
  {
    MonitorAutoLock lock(mMonitor);
    mTaskFactory.RevokeAll();
  }

  MessageLoop::current()->PostTask(
      NS_NewRunnableFunction("DestroyProcessRunnable", [this] { Destroy(); }));
}

}  // namespace mozilla
