/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RDDProcessImpl.h"

#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/GeckoArgs.h"

#if defined(OS_WIN) && defined(MOZ_SANDBOX)
#  include "mozilla/sandboxTarget.h"
#elif defined(__OpenBSD__) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#  include "prlink.h"
#endif

namespace mozilla {

using namespace ipc;

RDDProcessImpl::~RDDProcessImpl() = default;

bool RDDProcessImpl::Init(int aArgc, char* aArgv[]) {
#if defined(MOZ_SANDBOX) && defined(OS_WIN)
  // Preload AV dlls so we can enable Binary Signature Policy
  // to restrict further dll loads.
  LoadLibraryW(L"mozavcodec.dll");
  LoadLibraryW(L"mozavutil.dll");
  mozilla::SandboxTarget::Instance()->StartSandbox();
#elif defined(__OpenBSD__) && defined(MOZ_SANDBOX)
  PR_LoadLibrary("libmozavcodec.so");
  PR_LoadLibrary("libmozavutil.so");
  StartOpenBSDSandbox(GeckoProcessType_RDD);
#endif
  Maybe<const char*> parentBuildID =
      geckoargs::sParentBuildID.Get(aArgc, aArgv);
  if (parentBuildID.isNothing()) {
    return false;
  }

  if (!ProcessChild::InitPrefs(aArgc, aArgv)) {
    return false;
  }

  return mRDD.Init(TakeInitialEndpoint(), *parentBuildID);
}

void RDDProcessImpl::CleanUp() { NS_ShutdownXPCOM(nullptr); }

}  // namespace mozilla
