/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_PARENT_H
#define GFX_VR_PARENT_H

#include "mozilla/gfx/PVRParent.h"
#include "VRGPUParent.h"

namespace mozilla {
namespace gfx {

class VRService;
class VRSystemManagerExternal;

class VRParent final : public PVRParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRParent);

  friend class PVRParent;

 public:
  explicit VRParent();

  bool Init(base::ProcessId aParentPid, const char* aParentBuildID,
            mozilla::ipc::ScopedPort aPort);
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  bool GetOpenVRControllerActionPath(nsCString* aPath);
  bool GetOpenVRControllerManifestPath(VRControllerType aType,
                                       nsCString* aPath);

 protected:
  ~VRParent() = default;

  mozilla::ipc::IPCResult RecvNewGPUVRManager(
      Endpoint<PVRGPUParent>&& aEndpoint);
  mozilla::ipc::IPCResult RecvInit(nsTArray<GfxVarUpdate>&& vars,
                                   const DevicePrefs& devicePrefs);
  mozilla::ipc::IPCResult RecvNotifyVsync(const TimeStamp& vsyncTimestamp);
  mozilla::ipc::IPCResult RecvUpdateVar(const GfxVarUpdate& pref);
  mozilla::ipc::IPCResult RecvPreferenceUpdate(const Pref& pref);
  mozilla::ipc::IPCResult RecvOpenVRControllerActionPathToVR(
      const nsCString& aPath);
  mozilla::ipc::IPCResult RecvOpenVRControllerManifestPathToVR(
      const VRControllerType& aType, const nsCString& aPath);
  mozilla::ipc::IPCResult RecvRequestMemoryReport(
      const uint32_t& generation, const bool& anonymize,
      const bool& minimizeMemoryUsage,
      const Maybe<ipc::FileDescriptor>& DMDFile,
      const RequestMemoryReportResolver& aResolver);

 private:
  nsCString mOpenVRControllerAction;
  nsTHashMap<nsUint32HashKey, nsCString> mOpenVRControllerManifest;
  RefPtr<VRGPUParent> mVRGPUParent;
  DISALLOW_COPY_AND_ASSIGN(VRParent);
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_PARENT_H
