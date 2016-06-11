/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sts=8 sw=2 ts=2 tw=99 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _include_mozilla_gfx_ipc_GPUProcessHost_h_
#define _include_mozilla_gfx_ipc_GPUProcessHost_h_

#include "mozilla/Function.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/TaskFactory.h"

namespace mozilla {
namespace gfx {

class GPUChild;

// GPUProcessHost is the "parent process" container for a subprocess handle and
// IPC connection. It owns the parent process IPDL actor, which in this case,
// is a GPUChild.
//
// GPUProcessHosts are allocated and managed by GPUProcessManager. For all
// intents and purposes it is a singleton, though more than one may be allocated
// at a time due to its shutdown being asynchronous.
class GPUProcessHost final : public ipc::GeckoChildProcessHost
{
public:
  class Listener {
  public:
    virtual void OnProcessLaunchComplete(GPUProcessHost* aHost)
    {}
  };

public:
  GPUProcessHost(Listener* listener);
  ~GPUProcessHost();

  // Launch the subprocess asynchronously. On failure, false is returned.
  // Otherwise, true is returned, and the OnLaunchComplete listener callback
  // will be invoked either when a connection has been established, or if a
  // connection could not be established due to an asynchronous error.
  bool Launch();

  // If the process is being launched, block until it has launched and
  // connected. If a launch task is pending, it will fire immediately.
  //
  // Returns true if the process is successfully connected; false otherwise.
  bool WaitForLaunch();

  // Inform the process that it should clean up its resources and shut down.
  // This initiates an asynchronous shutdown sequence. After this method returns,
  // it is safe for the caller to forget its pointer to the GPUProcessHost.
  //
  // After this returns, the attached Listener is no longer used.
  void Shutdown();

  // Return the actor for the top-level actor of the process. If the process
  // has not connected yet, this returns null.
  GPUChild* GetActor() const {
    return mGPUChild.get();
  }

  bool IsConnected() const {
    return !!mGPUChild;
  }

  // Called on the IO thread.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

private:
  // Called on the main thread.
  void OnChannelConnectedTask();
  void OnChannelErrorTask();

  // Called on the main thread after a connection has been established.
  void InitAfterConnect(bool aSucceeded);

  void DestroyProcess();

private:
  DISALLOW_COPY_AND_ASSIGN(GPUProcessHost);

  Listener* mListener;
  ipc::TaskFactory<GPUProcessHost> mTaskFactory;

  enum class LaunchPhase {
    Unlaunched,
    Waiting,
    Complete
  };
  LaunchPhase mLaunchPhase;

  UniquePtr<GPUChild> mGPUChild;
};

} // namespace gfx
} // namespace mozilla

#endif // _include_mozilla_gfx_ipc_GPUProcessHost_h_
