/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRProcessChild.h"

#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/ipc/IOThreadChild.h"

using namespace mozilla;
using namespace mozilla::gfx;
using mozilla::ipc::IOThreadChild;

VRProcessChild::VRProcessChild(ProcessId aParentPid)
    : ProcessChild(aParentPid)
#if defined(aParentPid)
      ,
      mVR(nullptr)
#endif
{
}

VRProcessChild::~VRProcessChild() {}

bool VRProcessChild::Init(int aArgc, char* aArgv[]) {
  BackgroundHangMonitor::Startup();

  char* parentBuildID = nullptr;
  for (int i = 1; i < aArgc; i++) {
    if (strcmp(aArgv[i], "-parentBuildID") == 0) {
      parentBuildID = aArgv[i + 1];
    }
  }

  mVR.Init(ParentPid(), parentBuildID, IOThreadChild::message_loop(),
           IOThreadChild::channel());

  return true;
}

void VRProcessChild::CleanUp() { NS_ShutdownXPCOM(nullptr); }