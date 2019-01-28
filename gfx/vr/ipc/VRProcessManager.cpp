/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRProcessManager.h"

#include "VRProcessParent.h"
#include "VRChild.h"
#include "VRGPUChild.h"
#include "VRGPUParent.h"

namespace mozilla {
namespace gfx {

static StaticAutoPtr<VRProcessManager> sSingleton;

/* static */ VRProcessManager* VRProcessManager::Get() { return sSingleton; }

/* static */ void VRProcessManager::Initialize() {
  MOZ_ASSERT(XRE_IsParentProcess());
  sSingleton = new VRProcessManager();
}

/* static */ void VRProcessManager::Shutdown() { sSingleton = nullptr; }

VRProcessManager::VRProcessManager() : mProcess(nullptr) {
  MOZ_COUNT_CTOR(VRProcessManager);

  mObserver = new Observer(this);
  nsContentUtils::RegisterShutdownObserver(mObserver);
}

VRProcessManager::~VRProcessManager() {
  MOZ_COUNT_DTOR(VRProcessManager);

  if (mObserver) {
    nsContentUtils::UnregisterShutdownObserver(mObserver);
    mObserver = nullptr;
  }

  DestroyProcess();
  // The VR process should have already been shut down.
  MOZ_ASSERT(!mProcess);
}

void VRProcessManager::LaunchVRProcess() {
  if (mProcess) {
    return;
  }

  // The subprocess is launched asynchronously, so we wait for a callback to
  // acquire the IPDL actor.
  mProcess = new VRProcessParent(this);
  if (!mProcess->Launch()) {
    DisableVRProcess("Failed to launch VR process");
  }
}

void VRProcessManager::DisableVRProcess(const char* aMessage) {
  if (!gfxPrefs::VRProcessEnabled()) {
    return;
  }

  DestroyProcess();
}

void VRProcessManager::DestroyProcess() {
  if (!mProcess) {
    return;
  }

  mProcess->Shutdown();
  mProcess = nullptr;

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::VRProcessStatus,
                                     NS_LITERAL_CSTRING("Destroyed"));
}

void VRProcessManager::OnProcessLaunchComplete(VRProcessParent* aParent) {
  MOZ_ASSERT(mProcess && mProcess == aParent);

  if (!mProcess->IsConnected()) {
    DestroyProcess();
    return;
  }

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::VRProcessStatus,
                                     NS_LITERAL_CSTRING("Running"));
}

void VRProcessManager::OnProcessUnexpectedShutdown(VRProcessParent* aParent) {
  MOZ_ASSERT(mProcess && mProcess == aParent);

  DestroyProcess();
}

bool VRProcessManager::CreateGPUBridges(
    base::ProcessId aOtherProcess,
    mozilla::ipc::Endpoint<PVRGPUChild>* aOutVRBridge) {
  if (!CreateGPUVRManager(aOtherProcess, aOutVRBridge)) {
    return false;
  }
  return true;
}

bool VRProcessManager::CreateGPUVRManager(
    base::ProcessId aOtherProcess,
    mozilla::ipc::Endpoint<PVRGPUChild>* aOutEndpoint) {
  base::ProcessId vrparentPid = mProcess
                                    ? mProcess->OtherPid()  // VR process id.
                                    : base::GetCurrentProcId();

  ipc::Endpoint<PVRGPUParent> vrparentPipe;
  ipc::Endpoint<PVRGPUChild> vrchildPipe;
  nsresult rv = PVRGPU::CreateEndpoints(vrparentPid,    // vr process id
                                        aOtherProcess,  // gpu process id
                                        &vrparentPipe, &vrchildPipe);

  if (NS_FAILED(rv)) {
    gfxCriticalNote << "Could not create gpu-vr bridge: " << hexa(int(rv));
    return false;
  }

  // Bind vr-gpu pipe to VRParent and make a PVRGPU connection.
  VRChild* vrChild = mProcess->GetActor();
  vrChild->SendNewGPUVRManager(std::move(vrparentPipe));

  *aOutEndpoint = std::move(vrchildPipe);
  return true;
}

NS_IMPL_ISUPPORTS(VRProcessManager::Observer, nsIObserver);

VRProcessManager::Observer::Observer(VRProcessManager* aManager)
    : mManager(aManager) {}

NS_IMETHODIMP
VRProcessManager::Observer::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    mManager->OnXPCOMShutdown();
  }
  return NS_OK;
}

void VRProcessManager::CleanShutdown() { DestroyProcess(); }

void VRProcessManager::OnXPCOMShutdown() {
  if (mObserver) {
    nsContentUtils::UnregisterShutdownObserver(mObserver);
    mObserver = nullptr;
  }

  CleanShutdown();
}

VRChild* VRProcessManager::GetVRChild() { return mProcess->GetActor(); }

}  // namespace gfx
}  // namespace mozilla