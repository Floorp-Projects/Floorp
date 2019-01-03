/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRChild.h"
#include "VRProcessParent.h"
#include "gfxConfig.h"

#include "mozilla/gfx/gfxVars.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/VsyncDispatcher.h"

namespace mozilla {
namespace gfx {

VRChild::VRChild(VRProcessParent* aHost) : mHost(aHost) {
  MOZ_ASSERT(XRE_IsParentProcess());
}

void VRChild::ActorDestroy(ActorDestroyReason aWhy) {
  gfxVars::RemoveReceiver(this);
  mHost->OnChannelClosed();
}

void VRChild::Init() {
  // Build a list of prefs the VR process will need. Note that because we
  // limit the VR process to prefs contained in gfxPrefs, we can simplify
  // the message in two ways: one, we only need to send its index in gfxPrefs
  // rather than its name, and two, we only need to send prefs that don't
  // have their default value.
  // Todo: Consider to make our own vrPrefs that we are interested in VR
  // process.
  nsTArray<GfxPrefSetting> prefs;
  for (auto pref : gfxPrefs::all()) {
    if (pref->HasDefaultValue()) {
      continue;
    }

    GfxPrefValue value;
    pref->GetCachedValue(&value);
    prefs.AppendElement(GfxPrefSetting(pref->Index(), value));
  }
  nsTArray<GfxVarUpdate> updates = gfxVars::FetchNonDefaultVars();

  DevicePrefs devicePrefs;
  devicePrefs.hwCompositing() = gfxConfig::GetValue(Feature::HW_COMPOSITING);
  devicePrefs.d3d11Compositing() =
      gfxConfig::GetValue(Feature::D3D11_COMPOSITING);
  devicePrefs.oglCompositing() =
      gfxConfig::GetValue(Feature::OPENGL_COMPOSITING);
  devicePrefs.advancedLayers() = gfxConfig::GetValue(Feature::ADVANCED_LAYERS);
  devicePrefs.useD2D1() = gfxConfig::GetValue(Feature::DIRECT2D);

  SendInit(prefs, updates, devicePrefs);
  gfxVars::AddReceiver(this);
}

void VRChild::OnVarChanged(const GfxVarUpdate& aVar) { SendUpdateVar(aVar); }

class DeferredDeleteVRChild : public Runnable {
 public:
  explicit DeferredDeleteVRChild(UniquePtr<VRChild>&& aChild)
      : Runnable("gfx::DeferredDeleteVRChild"), mChild(std::move(aChild)) {}

  NS_IMETHODIMP Run() override { return NS_OK; }

 private:
  UniquePtr<VRChild> mChild;
};

/* static */ void VRChild::Destroy(UniquePtr<VRChild>&& aChild) {
  NS_DispatchToMainThread(new DeferredDeleteVRChild(std::move(aChild)));
}

}  // namespace gfx
}  // namespace mozilla