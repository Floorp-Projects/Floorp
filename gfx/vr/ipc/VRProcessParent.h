/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_PROCESS_PARENT_H
#define GFX_VR_PROCESS_PARENT_H

#include "mozilla/UniquePtr.h"

#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/TaskFactory.h"

namespace mozilla {
namespace gfx {

class VRChild;

class VRProcessParent final : public mozilla::ipc::GeckoChildProcessHost {
 public:
  explicit VRProcessParent();
  ~VRProcessParent();

  bool Launch();
  void Shutdown();
  void DestroyProcess();
  bool CanShutdown() override { return true; }

  void OnChannelError() override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelConnectedTask();
  void OnChannelErrorTask();
  void OnChannelClosed();

  base::ProcessId OtherPid();
  VRChild* GetActor() const { return mVRChild.get(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(VRProcessParent);

  void InitAfterConnect(bool aSucceeded);
  void KillHard(const char* aReason);

  UniquePtr<VRChild> mVRChild;
  mozilla::ipc::TaskFactory<VRProcessParent> mTaskFactory;
  nsCOMPtr<nsIThread> mLaunchThread;
  bool mChannelClosed;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // ifndef GFX_VR_PROCESS_PARENT_H