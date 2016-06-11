/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sts=8 sw=2 ts=2 tw=99 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPrefs.h"
#include "GPUProcessHost.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {
namespace gfx {

GPUProcessHost::GPUProcessHost(Listener* aListener)
 : GeckoChildProcessHost(GeckoProcessType_GPU),
   mListener(aListener),
   mTaskFactory(this),
   mLaunchPhase(LaunchPhase::Unlaunched)
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

void
GPUProcessHost::InitAfterConnect(bool aSucceeded)
{
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Waiting);
  MOZ_ASSERT(!mGPUChild);

  mLaunchPhase = LaunchPhase::Complete;

  if (aSucceeded) {
    mGPUChild = MakeUnique<GPUChild>();
    DebugOnly<bool> rv =
      mGPUChild->Open(GetChannel(), base::GetProcId(GetChildProcessHandle()));
    MOZ_ASSERT(rv);
  }

  if (mListener) {
    mListener->OnProcessLaunchComplete(this);
  }
}

void
GPUProcessHost::Shutdown()
{
  mListener = nullptr;
  DestroyProcess();
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

  DissociateActor();
}

} // namespace gfx
} // namespace mozilla
