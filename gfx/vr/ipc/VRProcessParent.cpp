/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRProcessParent.h"
#include "VRGPUChild.h"
#include "VRProcessManager.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/gfx/GPUChild.h"
#include "mozilla/ipc/ProtocolTypes.h"
#include "mozilla/ipc/ProtocolUtils.h"  // for IToplevelProtocol
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/Unused.h"
#include "VRChild.h"
#include "VRManager.h"
#include "VRThread.h"
#include "gfxVRPuppet.h"

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
  mLaunchThread = NS_GetCurrentThread();

  std::vector<std::string> extraArgs;
  nsCString parentBuildID(mozilla::PlatformBuildID());
  extraArgs.push_back("-parentBuildID");
  extraArgs.push_back(parentBuildID.get());

  if (!GeckoChildProcessHost::AsyncLaunch(extraArgs)) {
    return false;
  }
  return true;
}

void VRProcessParent::Shutdown() {
  MOZ_ASSERT(!mShutdownRequested);
  mListener = nullptr;

  // Tell GPU process to shutdown PVRGPU channel.
  GPUChild* gpuChild = GPUProcessManager::Get()->GetGPUChild();
  MOZ_ASSERT(gpuChild);
  gpuChild->SendShutdownVR();

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

static void DelayedDeleteSubprocess(GeckoChildProcessHost* aSubprocess) {
  XRE_GetIOMessageLoop()->PostTask(
      mozilla::MakeAndAddRef<DeleteTask<GeckoChildProcessHost>>(aSubprocess));
}

void VRProcessParent::DestroyProcess() {
  if (mLaunchThread) {
    mLaunchThread->Dispatch(NewRunnableFunction("DestroyProcessRunnable",
                                                DelayedDeleteSubprocess, this));
  }
}

void VRProcessParent::InitAfterConnect(bool aSucceeded) {
  if (aSucceeded) {
    mVRChild = MakeUnique<VRChild>(this);

    DebugOnly<bool> rv =
        mVRChild->Open(GetChannel(), base::GetProcId(GetChildProcessHandle()));
    MOZ_ASSERT(rv);

    mVRChild->Init();

    if (mListener) {
      mListener->OnProcessLaunchComplete(this);
    }

    // Make vr-gpu process connection
    GPUChild* gpuChild = GPUProcessManager::Get()->GetGPUChild();
    MOZ_ASSERT(gpuChild);

    Endpoint<PVRGPUChild> vrGPUBridge;
    VRProcessManager* vpm = VRProcessManager::Get();
    DebugOnly<bool> opened =
        vpm->CreateGPUBridges(gpuChild->OtherPid(), &vrGPUBridge);
    MOZ_ASSERT(opened);

    Unused << gpuChild->SendInitVR(std::move(vrGPUBridge));
  }
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

void VRProcessParent::OnChannelConnectedTask() { InitAfterConnect(true); }

void VRProcessParent::OnChannelErrorTask() {
  MOZ_ASSERT(false, "VR process channel error.");
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