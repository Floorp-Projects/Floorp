/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "UtilityProcessImpl.h"

#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/GeckoArgs.h"

#if defined(OS_WIN) && defined(MOZ_SANDBOX)
#  include "mozilla/sandboxTarget.h"
#endif

namespace mozilla::ipc {

UtilityProcessImpl::UtilityProcessImpl(ProcessId aParentPid)
    : ProcessChild(aParentPid) {
  mUtility = new UtilityProcessChild();
}

UtilityProcessImpl::~UtilityProcessImpl() { mUtility = nullptr; }

bool UtilityProcessImpl::Init(int aArgc, char* aArgv[]) {
#if defined(MOZ_SANDBOX) && defined(OS_WIN)
  mozilla::SandboxTarget::Instance()->StartSandbox();
#endif

  Maybe<const char*> parentBuildID =
      geckoargs::sParentBuildID.Get(aArgc, aArgv);
  if (parentBuildID.isNothing()) {
    return false;
  }

  Maybe<uint64_t> sandboxingKind = geckoargs::sSandboxingKind.Get(aArgc, aArgv);
  if (sandboxingKind.isNothing()) {
    return false;
  }

  // This checks needs to be kept in sync with SandboxingKind enum living in
  // ipc/glue/UtilityProcessSandboxing.h
  if (*sandboxingKind < SandboxingKind::GENERIC_UTILITY ||
      *sandboxingKind > SandboxingKind::GENERIC_UTILITY) {
    return false;
  }

  if (!ProcessChild::InitPrefs(aArgc, aArgv)) {
    return false;
  }

  return mUtility->Init(ParentPid(), nsCString(*parentBuildID), *sandboxingKind,
                        IOThreadChild::TakeInitialPort());
}

void UtilityProcessImpl::CleanUp() { NS_ShutdownXPCOM(nullptr); }

}  // namespace mozilla::ipc
