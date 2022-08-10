/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GPUProcessImpl.h"
#include "mozilla/ipc/IOThreadChild.h"
#include "nsXPCOM.h"
#include "mozilla/ipc/ProcessUtils.h"
#include "mozilla/GeckoArgs.h"

#if defined(OS_WIN) && defined(MOZ_SANDBOX)
#  include "mozilla/sandboxTarget.h"
#elif defined(__OpenBSD__) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#endif

namespace mozilla {
namespace gfx {

using namespace ipc;

GPUProcessImpl::~GPUProcessImpl() = default;

bool GPUProcessImpl::Init(int aArgc, char* aArgv[]) {
#if defined(MOZ_SANDBOX) && defined(OS_WIN)
  mozilla::SandboxTarget::Instance()->StartSandbox();
#elif defined(__OpenBSD__) && defined(MOZ_SANDBOX)
  StartOpenBSDSandbox(GeckoProcessType_GPU);
#endif

  Maybe<const char*> parentBuildID =
      geckoargs::sParentBuildID.Get(aArgc, aArgv);
  if (parentBuildID.isNothing()) {
    return false;
  }

  if (!ProcessChild::InitPrefs(aArgc, aArgv)) {
    return false;
  }

  return mGPU.Init(TakeInitialEndpoint(), *parentBuildID);
}

void GPUProcessImpl::CleanUp() { NS_ShutdownXPCOM(nullptr); }

}  // namespace gfx
}  // namespace mozilla
