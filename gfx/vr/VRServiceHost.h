/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_HOST_H
#define GFX_VR_SERVICE_HOST_H

#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

#include <cstdint>

namespace mozilla {
namespace gfx {

struct VRExternalShmem;
class VRService;

/**
 * VRServiceHost is allocated as a singleton in the GPU process.
 * It is responsible for allocating VRService either within the GPU process
 * or in the VR process.
 * When the VR process is enabled, it maintains the state of the VR process,
 * starting and stopping it as needed.
 * VRServiceHost provides an interface that enables communication of the
 * VRService in the same way regardless of it running within the GPU process
 * or the VR process.
 */

class VRServiceHost {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::gfx::VRServiceHost)
 public:
  static void Init(bool aEnableVRProcess);
  static VRServiceHost* Get();

  void Refresh();
  void StartService();
  void StopService();
  void Shutdown();
  void CreateService(volatile VRExternalShmem* aShmem);
  void NotifyVRProcessStarted();
  void CheckForPuppetCompletion();

  void PuppetSubmit(const nsTArray<uint64_t>& aBuffer);
  void PuppetReset();

 protected:
 private:
  explicit VRServiceHost(bool aEnableVRProcess);
  ~VRServiceHost();

  void RefreshVRProcess();
  bool NeedVRProcess();
  void CreateVRProcess();
  void ShutdownVRProcess();
  void SendPuppetResetToVRProcess();
  void SendPuppetCheckForCompletionToVRProcess();
  void SendPuppetSubmitToVRProcess(const nsTArray<uint64_t>& aBuffer);

  // Commands pending to be sent to the puppet device
  // once the VR service is started.
  nsTArray<uint64_t> mPuppetPendingCommands;

  RefPtr<VRService> mVRService;
  // mVRProcessEnabled indicates that a separate, VR Process, should be used.
  // This may be false if the VR process is disabled with the
  // dom.vr.process.enabled preference or when the GPU process is disabled.
  // mVRProcessEnabled will not change once the browser is started and does not
  // reflect the started / stopped state of the VR Process.
  bool mVRProcessEnabled;
  // mVRProcessStarted is true when the VR Process is running.
  bool mVRProcessStarted;
  // mVRServiceReadyInVRProcess is true when the VR Process is running and the
  // VRService in the VR Process is ready to accept commands.
  bool mVRServiceReadyInVRProcess;
  // mVRServiceRequested is true when the VRService is needed.  This can be due
  // to Web API activity (WebXR, WebVR), browser activity (eg, VR Video
  // Playback), or a request to simulate a VR device with the VRServiceTest /
  // puppet API. mVRServiceRequested indicates the intended state of the VR
  // Service and is not an indication that the VR Service is ready to accept
  // requests or that the VR Process is enabled or running. Toggling the
  // mVRServiceRequested flag will result in the VR Service and/or the VR
  // Process either starting or stopping as needed.
  bool mVRServiceRequested;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_SERVICE_HOST_H
