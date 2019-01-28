/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_CHILD_H
#define GFX_VR_CHILD_H

#include "mozilla/gfx/PVRChild.h"
#include "mozilla/gfx/gfxVarReceiver.h"
#include "mozilla/VsyncDispatcher.h"
#include "gfxVR.h"

namespace mozilla {
namespace ipc {
class CrashReporterHost;
}  // namespace ipc
namespace gfx {

class VRProcessParent;
class VRChild;

class VRChild final : public PVRChild, public gfxVarReceiver {
 public:
  explicit VRChild(VRProcessParent* aHost);
  ~VRChild() = default;

  static void Destroy(UniquePtr<VRChild>&& aChild);
  void Init();
  virtual void OnVarChanged(const GfxVarUpdate& aVar) override;

 protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual mozilla::ipc::IPCResult RecvOpenVRControllerActionPathToParent(
      const nsCString& aPath) override;
  virtual mozilla::ipc::IPCResult RecvOpenVRControllerManifestPathToParent(
      const OpenVRControllerType& aType, const nsCString& aPath) override;
  virtual mozilla::ipc::IPCResult RecvInitCrashReporter(
      Shmem&& shmem, const NativeThreadId& aThreadId) override;

 private:
  VRProcessParent* mHost;
  UniquePtr<ipc::CrashReporterHost> mCrashReporter;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_CHILD_H