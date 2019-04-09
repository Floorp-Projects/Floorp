/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RDDProcessImpl.h"

#include "mozilla/ipc/IOThreadChild.h"

#if defined(OS_WIN) && defined(MOZ_SANDBOX)
#  include "mozilla/sandboxTarget.h"
#endif

#include "ProcessUtils.h"

namespace mozilla {

using namespace ipc;

RDDProcessImpl::RDDProcessImpl(ProcessId aParentPid)
    : ProcessChild(aParentPid) {}

RDDProcessImpl::~RDDProcessImpl() {}

bool RDDProcessImpl::Init(int aArgc, char* aArgv[]) {
#if defined(MOZ_SANDBOX) && defined(OS_WIN)
  mozilla::SandboxTarget::Instance()->StartSandbox();
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

  return mRDD.Init(ParentPid(), parentBuildID, IOThreadChild::message_loop(),
                   IOThreadChild::channel());
}

void RDDProcessImpl::CleanUp() { NS_ShutdownXPCOM(nullptr); }

}  // namespace mozilla
