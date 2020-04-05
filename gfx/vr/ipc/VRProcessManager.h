/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GFX_VR_PROCESS_MANAGER_H
#define GFX_VR_PROCESS_MANAGER_H

#include "VRProcessParent.h"

namespace mozilla {
class MemoryReportingProcess;
namespace ipc {
template <typename T>
class Endpoint;
}
namespace gfx {

class VRManagerChild;
class PVRGPUChild;
class VRChild;

// The VRProcessManager is a singleton responsible for creating VR-bound
// objects that may live in another process.
class VRProcessManager final : public VRProcessParent::Listener {
 public:
  static VRProcessManager* Get();
  static void Initialize();
  static void Shutdown();

  ~VRProcessManager();

  // If not using a VR process, launch a new VR process asynchronously.
  void LaunchVRProcess();

  // Ensure that VR-bound methods can be used. If no VR process is being
  // used, or one is launched and ready, this function returns immediately.
  // Otherwise it blocks until the VR process has finished launching.
  bool EnsureVRReady();

  bool CreateGPUBridges(base::ProcessId aOtherProcess,
                        mozilla::ipc::Endpoint<PVRGPUChild>* aOutVRBridge);

  VRChild* GetVRChild();
  // If a VR process is present, create a MemoryReportingProcess object.
  // Otherwise, return null.
  RefPtr<MemoryReportingProcess> GetProcessMemoryReporter();

  virtual void OnProcessLaunchComplete(VRProcessParent* aParent) override;
  virtual void OnProcessUnexpectedShutdown(VRProcessParent* aParent) override;

 private:
  VRProcessManager();

  DISALLOW_COPY_AND_ASSIGN(VRProcessManager);

  bool CreateGPUVRManager(base::ProcessId aOtherProcess,
                          mozilla::ipc::Endpoint<PVRGPUChild>* aOutEndpoint);
  void OnXPCOMShutdown();
  void OnPreferenceChange(const char16_t* aData);
  void CleanShutdown();
  void DestroyProcess();

  // Permanently disable the VR process and record a message why.
  void DisableVRProcess(const char* aMessage);

  class Observer final : public nsIObserver {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    explicit Observer(VRProcessManager* aManager);

   protected:
    ~Observer() = default;

    VRProcessManager* mManager;
  };
  friend class Observer;

  RefPtr<Observer> mObserver;
  VRProcessParent* mProcess;
  VRChild* mVRChild;
  // Collects any pref changes that occur during process launch (after
  // the initial map is passed in command-line arguments) to be sent
  // when the process can receive IPC messages.
  nsTArray<mozilla::dom::Pref> mQueuedPrefs;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_PROCESS_MANAGER_H
