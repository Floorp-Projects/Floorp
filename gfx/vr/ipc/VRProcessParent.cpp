/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRProcessParent.h"
#include "VRGPUChild.h"
#include "VRProcessManager.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/gfx/GPUChild.h"
#include "mozilla/ipc/ProtocolTypes.h"
#include "mozilla/ipc/ProtocolUtils.h"  // for IToplevelProtocol
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/TimeStamp.h"  // for TimeStamp
#include "mozilla/Unused.h"
#include "ProcessUtils.h"
#include "VRChild.h"
#include "VRManager.h"
#include "VRThread.h"

#include "nsAppRunner.h"  // for IToplevelProtocol
#include "mozilla/ipc/ProtocolUtils.h"

using std::string;
using std::vector;

using namespace mozilla::ipc;

namespace mozilla {
namespace gfx {

VRProcessParent::VRProcessParent(Listener* aListener)
    : GeckoChildProcessHost(GeckoProcessType_VR),
      mTaskFactory(this),
      mListener(aListener),
      mLaunchPhase(LaunchPhase::Unlaunched),
      mChannelClosed(false),
      mShutdownRequested(false) {
  MOZ_COUNT_CTOR(VRProcessParent);
}

VRProcessParent::~VRProcessParent() {
  // Cancel all tasks. We don't want anything triggering after our caller
  // expects this to go away.
  {
    MonitorAutoLock lock(mMonitor);
    mTaskFactory.RevokeAll();
  }
  MOZ_COUNT_DTOR(VRProcessParent);
}

bool VRProcessParent::Launch() {
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Unlaunched);
  MOZ_ASSERT(!mVRChild);
  mLaunchThread = NS_GetCurrentThread();

  mLaunchPhase = LaunchPhase::Waiting;

  std::vector<std::string> extraArgs;
  nsCString parentBuildID(mozilla::PlatformBuildID());
  extraArgs.push_back("-parentBuildID");
  extraArgs.push_back(parentBuildID.get());

  mPrefSerializer = MakeUnique<ipc::SharedPreferenceSerializer>();
  if (!mPrefSerializer->SerializeToSharedMemory()) {
    return false;
  }
  mPrefSerializer->AddSharedPrefCmdLineArgs(*this, extraArgs);

  if (!GeckoChildProcessHost::AsyncLaunch(extraArgs)) {
    mLaunchPhase = LaunchPhase::Complete;
    mPrefSerializer = nullptr;
    return false;
  }
  return true;
}

bool VRProcessParent::WaitForLaunch() {
  if (mLaunchPhase == LaunchPhase::Complete) {
    return !!mVRChild;
  }

  int32_t timeoutMs =
      StaticPrefs::dom_vr_process_startup_timeout_ms_AtStartup();

  // If one of the following environment variables are set we can effectively
  // ignore the timeout - as we can guarantee the compositor process will be
  // terminated
  if (PR_GetEnv("MOZ_DEBUG_CHILD_PROCESS") ||
      PR_GetEnv("MOZ_DEBUG_CHILD_PAUSE")) {
    timeoutMs = 0;
  }

  // Our caller expects the connection to be finished after we return, so we
  // immediately set up the IPDL actor and fire callbacks. The IO thread will
  // still dispatch a notification to the main thread - we'll just ignore it.
  bool result = GeckoChildProcessHost::WaitUntilConnected(timeoutMs);
  result &= InitAfterConnect(result);
  return result;
}

void VRProcessParent::Shutdown() {
  MOZ_ASSERT(!mShutdownRequested);
  mListener = nullptr;

  if (mVRChild) {
    // The channel might already be closed if we got here unexpectedly.
    if (!mChannelClosed) {
      mVRChild->Close();
    }
    // OnChannelClosed uses this to check if the shutdown was expected or
    // unexpected.
    mShutdownRequested = true;

#ifndef NS_FREE_PERMANENT_DATA
    // No need to communicate shutdown, the VR process doesn't need to
    // communicate anything back.
    KillHard("NormalShutdown");
#endif

    // If we're shutting down unexpectedly, we're in the middle of handling an
    // ActorDestroy for PVRChild, which is still on the stack. We'll return
    // back to OnChannelClosed.
    //
    // Otherwise, we'll wait for OnChannelClose to be called whenever PVRChild
    // acknowledges shutdown.
    return;
  }

  DestroyProcess();
}

void VRProcessParent::DestroyProcess() {
  if (mLaunchThread) {
    mLaunchThread->Dispatch(NS_NewRunnableFunction("DestroyProcessRunnable",
                                                   [this] { Destroy(); }));
  }
}

bool VRProcessParent::InitAfterConnect(bool aSucceeded) {
  MOZ_ASSERT(mLaunchPhase == LaunchPhase::Waiting);
  MOZ_ASSERT(!mVRChild);

  mLaunchPhase = LaunchPhase::Complete;
  mPrefSerializer = nullptr;

  if (aSucceeded) {
    GPUChild* gpuChild = GPUProcessManager::Get()->GetGPUChild();
    if (!gpuChild) {
      NS_WARNING(
          "GPU process haven't connected with the parent process yet"
          "when creating VR process.");
      return false;
    }

    mVRChild = MakeUnique<VRChild>(this);

    DebugOnly<bool> rv =
        mVRChild->Open(TakeChannel(), base::GetProcId(GetChildProcessHandle()));
    MOZ_ASSERT(rv);

    mVRChild->Init();

    if (mListener) {
      mListener->OnProcessLaunchComplete(this);
    }

    // Make vr-gpu process connection
    Endpoint<PVRGPUChild> vrGPUBridge;
    VRProcessManager* vpm = VRProcessManager::Get();
    DebugOnly<bool> opened =
        vpm->CreateGPUBridges(gpuChild->OtherPid(), &vrGPUBridge);
    MOZ_ASSERT(opened);

    Unused << gpuChild->SendInitVR(std::move(vrGPUBridge));
  }

  return true;
}

void VRProcessParent::KillHard(const char* aReason) {
  ProcessHandle handle = GetChildProcessHandle();
  if (!base::KillProcess(handle, base::PROCESS_END_KILLED_BY_USER, false)) {
    NS_WARNING("failed to kill subprocess!");
  }

  SetAlreadyDead();
}

void VRProcessParent::OnChannelError() {
  MOZ_ASSERT(false, "VR process channel error.");
}

void VRProcessParent::OnChannelConnected(int32_t peer_pid) {
  MOZ_ASSERT(!NS_IsMainThread());

  GeckoChildProcessHost::OnChannelConnected(peer_pid);

  // Post a task to the main thread. Take the lock because mTaskFactory is not
  // thread-safe.
  RefPtr<Runnable> runnable;
  {
    MonitorAutoLock lock(mMonitor);
    runnable = mTaskFactory.NewRunnableMethod(
        &VRProcessParent::OnChannelConnectedTask);
  }
  NS_DispatchToMainThread(runnable);
}

void VRProcessParent::OnChannelConnectedTask() {
  if (mLaunchPhase == LaunchPhase::Waiting) {
    InitAfterConnect(true);
  }
}

void VRProcessParent::OnChannelErrorTask() {
  if (mLaunchPhase == LaunchPhase::Waiting) {
    InitAfterConnect(false);
  }
}

void VRProcessParent::OnChannelClosed() {
  mChannelClosed = true;
  if (!mShutdownRequested && mListener) {
    // This is an unclean shutdown. Notify we're going away.
    mListener->OnProcessUnexpectedShutdown(this);
  } else {
    DestroyProcess();
  }

  // Release the actor.
  VRChild::Destroy(std::move(mVRChild));
  MOZ_ASSERT(!mVRChild);
}

base::ProcessId VRProcessParent::OtherPid() { return mVRChild->OtherPid(); }

bool VRProcessParent::IsConnected() const { return !!mVRChild; }

}  // namespace gfx
}  // namespace mozilla
