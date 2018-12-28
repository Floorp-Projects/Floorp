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

 public:
  explicit VRParent();

  bool Init(base::ProcessId aParentPid, const char* aParentBuildID,
            MessageLoop* aIOLoop, IPC::Channel* aChannel);
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  bool GetOpenVRControllerActionPath(nsCString* aPath);
  bool GetOpenVRControllerManifestPath(OpenVRControllerType aType,
                                       nsCString* aPath);

 protected:
  ~VRParent() = default;

  virtual mozilla::ipc::IPCResult RecvNewGPUVRManager(
      Endpoint<PVRGPUParent>&& aEndpoint) override;
  virtual mozilla::ipc::IPCResult RecvInit(
      nsTArray<GfxPrefSetting>&& prefs, nsTArray<GfxVarUpdate>&& vars,
      const DevicePrefs& devicePrefs) override;
  virtual mozilla::ipc::IPCResult RecvNotifyVsync(
      const TimeStamp& vsyncTimestamp) override;
  virtual mozilla::ipc::IPCResult RecvUpdatePref(
      const GfxPrefSetting& setting) override;
  virtual mozilla::ipc::IPCResult RecvUpdateVar(
      const GfxVarUpdate& pref) override;
  virtual mozilla::ipc::IPCResult RecvOpenVRControllerActionPathToVR(
      const nsCString& aPath) override;
  virtual mozilla::ipc::IPCResult RecvOpenVRControllerManifestPathToVR(
      const OpenVRControllerType& aType, const nsCString& aPath) override;

 private:
  nsCString mOpenVRControllerAction;
  nsDataHashtable<nsUint32HashKey, nsCString> mOpenVRControllerManifest;
  RefPtr<VRGPUParent> mVRGPUParent;
  DISALLOW_COPY_AND_ASSIGN(VRParent);
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_PARENT_H