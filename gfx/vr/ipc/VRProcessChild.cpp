/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRProcessChild.h"

#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/GeckoArgs.h"
#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/ipc/ProcessUtils.h"
#include "mozilla/StaticPrefs_dom.h"

using namespace mozilla;
using namespace mozilla::gfx;
using mozilla::ipc::IOThreadChild;

StaticRefPtr<VRParent> sVRParent;

VRProcessChild::~VRProcessChild() { sVRParent = nullptr; }

/*static*/
VRParent* VRProcessChild::GetVRParent() {
  MOZ_ASSERT(sVRParent);
  return sVRParent;
}

bool VRProcessChild::Init(int aArgc, char* aArgv[]) {
  Maybe<const char*> parentBuildID =
      geckoargs::sParentBuildID.Get(aArgc, aArgv);
  if (parentBuildID.isNothing()) {
    return false;
  }

  if (!ProcessChild::InitPrefs(aArgc, aArgv)) {
    return false;
  }

  sVRParent = new VRParent();
  sVRParent->Init(TakeInitialEndpoint(), *parentBuildID);

  return true;
}

void VRProcessChild::CleanUp() {
  sVRParent = nullptr;
  NS_ShutdownXPCOM(nullptr);
}
