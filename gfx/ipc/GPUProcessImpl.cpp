/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GPUProcessImpl.h"
#include "mozilla/ipc/IOThreadChild.h"
#include "nsXPCOM.h"
#include "mozilla/ipc/ProcessUtils.h"

#if defined(OS_WIN) && defined(MOZ_SANDBOX)
#  include "mozilla/sandboxTarget.h"
#elif defined(__OpenBSD__) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#endif

namespace mozilla {
namespace gfx {

using namespace ipc;

GPUProcessImpl::GPUProcessImpl(ProcessId aParentPid)
    : ProcessChild(aParentPid) {}

GPUProcessImpl::~GPUProcessImpl() = default;

bool GPUProcessImpl::Init(int aArgc, char* aArgv[]) {
#if defined(MOZ_SANDBOX) && defined(OS_WIN)
  mozilla::SandboxTarget::Instance()->StartSandbox();
#elif defined(__OpenBSD__) && defined(MOZ_SANDBOX)
  StartOpenBSDSandbox(GeckoProcessType_GPU);
#endif
  char* parentBuildID = nullptr;
  char* prefsHandle = nullptr;
  char* prefMapHandle = nullptr;
  char* prefsLen = nullptr;
  char* prefMapSize = nullptr;
  for (int i = 1; i < aArgc; i++) {
    if (!aArgv[i]) {
      continue;
    }
    if (strcmp(aArgv[i], "-parentBuildID") == 0) {
      parentBuildID = aArgv[i + 1];

#ifdef XP_WIN
    } else if (strcmp(aArgv[i], "-prefsHandle") == 0) {
      if (++i == aArgc) {
        return false;
      }
      prefsHandle = aArgv[i];
    } else if (strcmp(aArgv[i], "-prefMapHandle") == 0) {
      if (++i == aArgc) {
        return false;
      }
      prefMapHandle = aArgv[i];
#endif
    } else if (strcmp(aArgv[i], "-prefsLen") == 0) {
      if (++i == aArgc) {
        return false;
      }
      prefsLen = aArgv[i];
    } else if (strcmp(aArgv[i], "-prefMapSize") == 0) {
      if (++i == aArgc) {
        return false;
      }
      prefMapSize = aArgv[i];
    }
  }

  SharedPreferenceDeserializer deserializer;
  if (!deserializer.DeserializeFromSharedMemory(prefsHandle, prefMapHandle,
                                                prefsLen, prefMapSize)) {
    return false;
  }

  return mGPU.Init(ParentPid(), parentBuildID,
                   IOThreadChild::TakeInitialPort());
}

void GPUProcessImpl::CleanUp() { NS_ShutdownXPCOM(nullptr); }

}  // namespace gfx
}  // namespace mozilla
