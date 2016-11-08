/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sts=8 sw=2 ts=2 tw=99 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GPUProcessHost.h"
#include "chrome/common/process_watcher.h"
#include "gfxPrefs.h"
#include "mozilla/gfx/Logging.h"
#include "nsITimer.h"

namespace mozilla {
namespace gfx {

using namespace ipc;

GPUProcessHost::GPUProcessHost(Listener* aListener)
 : GeckoChildProcessHost(GeckoProcessType_GPU),
   mListener(aListener),
   mTaskFactory(this),
   mLaunchPhase(LaunchPhase::Unlaunched),
   mProcessToken(0),
   mShutdownRequested(false),
   mChannelClosed(false)
{
  MOZ_COUNT_CTOR(GPUProcessHost);
}

GPUProcessHost::~GPUProcessHost()
{
  MOZ_COUNT_DTOR(GPUProcessHost);
}

bool
GPUProcessHost::Launch()
{
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Unlaunched);
  MOZ_ASSERT(!mGPUChild);

  mLaunchPhase = LaunchPhase::Waiting;
  mLaunchTime = TimeStamp::Now();

  if (!GeckoChildProcessHost::AsyncLaunch()) {
    mLaunchPhase = LaunchPhase::Complete;
    return false;
  }
  return true;
}

bool
GPUProcessHost::WaitForLaunch()
{
  if (mLaunchPhase == LaunchPhase::Complete) {
    return !!mGPUChild;
  }

  int32_t timeoutMs = gfxPrefs::GPUProcessDevTimeoutMs();

  // Our caller expects the connection to be finished after we return, so we
  // immediately set up the IPDL actor and fire callbacks. The IO thread will
  // still dispatch a notification to the main thread - we'll just ignore it.
  bool result = GeckoChildProcessHost::WaitUntilConnected(timeoutMs);
  InitAfterConnect(result);
  return result;
}

void
GPUProcessHost::OnChannelConnected(int32_t peer_pid)
{
  MOZ_ASSERT(!NS_IsMainThread());

  GeckoChildProcessHost::OnChannelConnected(peer_pid);

  // Post a task to the main thread. Take the lock because mTaskFactory is not
  // thread-safe.
  RefPtr<Runnable> runnable;
  {
    MonitorAutoLock lock(mMonitor);
    runnable = mTaskFactory.NewRunnableMethod(&GPUProcessHost::OnChannelConnectedTask);
  }
  NS_DispatchToMainThread(runnable);
}

void
GPUProcessHost::OnChannelError()
{
  MOZ_ASSERT(!NS_IsMainThread());

  GeckoChildProcessHost::OnChannelError();

  // Post a task to the main thread. Take the lock because mTaskFactory is not
  // thread-safe.
  RefPtr<Runnable> runnable;
  {
    MonitorAutoLock lock(mMonitor);
    runnable = mTaskFactory.NewRunnableMethod(&GPUProcessHost::OnChannelErrorTask);
  }
  NS_DispatchToMainThread(runnable);
}

void
GPUProcessHost::OnChannelConnectedTask()
{
  if (mLaunchPhase == LaunchPhase::Waiting) {
    InitAfterConnect(true);
  }
}

void
GPUProcessHost::OnChannelErrorTask()
{
  if (mLaunchPhase == LaunchPhase::Waiting) {
    InitAfterConnect(false);
  }
}

static uint64_t sProcessTokenCounter = 0;

void
GPUProcessHost::InitAfterConnect(bool aSucceeded)
{
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Waiting);
  MOZ_ASSERT(!mGPUChild);

  mLaunchPhase = LaunchPhase::Complete;

  if (aSucceeded) {
    mProcessToken = ++sProcessTokenCounter;
    mGPUChild = MakeUnique<GPUChild>(this);
    DebugOnly<bool> rv =
      mGPUChild->Open(GetChannel(), base::GetProcId(GetChildProcessHandle()));
    MOZ_ASSERT(rv);

    mGPUChild->Init();
  }

  if (mListener) {
    mListener->OnProcessLaunchComplete(this);
  }
}

void
GPUProcessHost::Shutdown()
{
  MOZ_ASSERT(!mShutdownRequested);

  mListener = nullptr;

  if (mGPUChild) {
    // OnChannelClosed uses this to check if the shutdown was expected or
    // unexpected.
    mShutdownRequested = true;

#ifdef NS_FREE_PERMANENT_DATA
    // The channel might already be closed if we got here unexpectedly.
    if (!mChannelClosed) {
      mGPUChild->Close();
    }
#else
    // No need to communicate shutdown, the GPU process doesn't need to
    // communicate anything back.
    KillHard("NormalShutdown");
#endif

    // If we're shutting down unexpectedly, we're in the middle of handling an
    // ActorDestroy for PGPUChild, which is still on the stack. We'll return
    // back to OnChannelClosed.
    //
    // Otherwise, we'll wait for OnChannelClose to be called whenever PGPUChild
    // acknowledges shutdown.
    return;
  }

  DestroyProcess();
}

void
GPUProcessHost::OnChannelClosed()
{
  if (!mShutdownRequested) {
    // This is an unclean shutdown. Notify our listener that we're going away.
    mChannelClosed = true;
    if (mListener) {
      mListener->OnProcessUnexpectedShutdown(this);
    }
  }

  // Release the actor.
  GPUChild::Destroy(Move(mGPUChild));
  MOZ_ASSERT(!mGPUChild);

  // If the owner of GPUProcessHost already requested shutdown, we can now
  // schedule destruction. Otherwise we must wait for someone to call
  // Shutdown. Note that GPUProcessManager calls Shutdown within
  // OnProcessUnexpectedShutdown.
  if (mShutdownRequested) {
    DestroyProcess();
  }
}

void
GPUProcessHost::KillHard(const char* aReason)
{
  ProcessHandle handle = GetChildProcessHandle();
  if (!base::KillProcess(handle, base::PROCESS_END_KILLED_BY_USER, false)) {
    NS_WARNING("failed to kill subprocess!");
  }

  SetAlreadyDead();
}

uint64_t
GPUProcessHost::GetProcessToken() const
{
  return mProcessToken;
}

static void
DelayedDeleteSubprocess(GeckoChildProcessHost* aSubprocess)
{
  XRE_GetIOMessageLoop()->
    PostTask(mozilla::MakeAndAddRef<DeleteTask<GeckoChildProcessHost>>(aSubprocess));
}

void
GPUProcessHost::KillProcess()
{
  KillHard("DiagnosticKill");
}

void
GPUProcessHost::DestroyProcess()
{
  // Cancel all tasks. We don't want anything triggering after our caller
  // expects this to go away.
  {
    MonitorAutoLock lock(mMonitor);
    mTaskFactory.RevokeAll();
  }

  MessageLoop::current()->
    PostTask(NewRunnableFunction(DelayedDeleteSubprocess, this));
}

} // namespace gfx
} // namespace mozilla
