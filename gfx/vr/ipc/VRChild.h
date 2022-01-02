/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_CHILD_H
#define GFX_VR_CHILD_H

#include "mozilla/gfx/PVRChild.h"
#include "mozilla/gfx/gfxVarReceiver.h"
#include "mozilla/ipc/CrashReporterHelper.h"
#include "mozilla/VsyncDispatcher.h"
#include "moz_external_vr.h"

namespace mozilla {
namespace dom {
class MemoryReportRequestHost;
}  // namespace dom
namespace gfx {

class VRProcessParent;
class VRChild;

class VRChild final : public PVRChild,
                      public ipc::CrashReporterHelper<GeckoProcessType_VR>,
                      public gfxVarReceiver {
  typedef mozilla::dom::MemoryReportRequestHost MemoryReportRequestHost;
  friend class PVRChild;

 public:
  explicit VRChild(VRProcessParent* aHost);
  ~VRChild() = default;

  static void Destroy(UniquePtr<VRChild>&& aChild);
  void Init();
  bool EnsureVRReady();
  virtual void OnVarChanged(const GfxVarUpdate& aVar) override;
  bool SendRequestMemoryReport(const uint32_t& aGeneration,
                               const bool& aAnonymize,
                               const bool& aMinimizeMemoryUsage,
                               const Maybe<ipc::FileDescriptor>& aDMDFile);

 protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult RecvOpenVRControllerActionPathToParent(
      const nsCString& aPath);
  mozilla::ipc::IPCResult RecvOpenVRControllerManifestPathToParent(
      const VRControllerType& aType, const nsCString& aPath);
  mozilla::ipc::IPCResult RecvInitComplete();

  mozilla::ipc::IPCResult RecvAddMemoryReport(const MemoryReport& aReport);

 private:
  VRProcessParent* mHost;
  UniquePtr<MemoryReportRequestHost> mMemoryReportRequest;
  bool mVRReady;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_CHILD_H
