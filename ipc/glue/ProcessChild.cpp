/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/ProcessChild.h"

#include "Endpoint.h"
#include "nsDebug.h"

#ifdef XP_WIN
#  include <stdlib.h>  // for _exit()
#else
#  include <unistd.h>  // for _exit()
#endif

#include "nsAppRunner.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/GeckoArgs.h"

namespace mozilla {
namespace ipc {

ProcessChild* ProcessChild::gProcessChild;

static Atomic<bool> sExpectingShutdown(false);

ProcessChild::ProcessChild(ProcessId aParentPid, const nsID& aMessageChannelId)
    : ChildProcess(new IOThreadChild()),
      mUILoop(MessageLoop::current()),
      mParentPid(aParentPid),
      mMessageChannelId(aMessageChannelId) {
  MOZ_ASSERT(mUILoop, "UILoop should be created by now");
  MOZ_ASSERT(!gProcessChild, "should only be one ProcessChild");
  gProcessChild = this;
}

/* static */
void ProcessChild::AddPlatformBuildID(std::vector<std::string>& aExtraArgs) {
  nsCString parentBuildID(mozilla::PlatformBuildID());
  geckoargs::sParentBuildID.Put(parentBuildID.get(), aExtraArgs);
}

/* static */
bool ProcessChild::InitPrefs(int aArgc, char* aArgv[]) {
  Maybe<uint64_t> prefsHandle = Some(0);
  Maybe<uint64_t> prefMapHandle = Some(0);
  Maybe<uint64_t> prefsLen = geckoargs::sPrefsLen.Get(aArgc, aArgv);
  Maybe<uint64_t> prefMapSize = geckoargs::sPrefMapSize.Get(aArgc, aArgv);

  if (prefsLen.isNothing() || prefMapSize.isNothing()) {
    return false;
  }

#ifdef XP_WIN
  prefsHandle = geckoargs::sPrefsHandle.Get(aArgc, aArgv);
  prefMapHandle = geckoargs::sPrefMapHandle.Get(aArgc, aArgv);

  if (prefsHandle.isNothing() || prefMapHandle.isNothing()) {
    return false;
  }
#endif

  SharedPreferenceDeserializer deserializer;
  return deserializer.DeserializeFromSharedMemory(*prefsHandle, *prefMapHandle,
                                                  *prefsLen, *prefMapSize);
}

ProcessChild::~ProcessChild() { gProcessChild = nullptr; }

/* static */
void ProcessChild::NotifiedImpendingShutdown() {
  sExpectingShutdown = true;
  CrashReporter::AppendToCrashReportAnnotation(
      CrashReporter::Annotation::IPCShutdownState,
      "NotifiedImpendingShutdown"_ns);
}

/* static */
bool ProcessChild::ExpectingShutdown() { return sExpectingShutdown; }

/* static */
void ProcessChild::QuickExit() { AppShutdown::DoImmediateExit(); }

UntypedEndpoint ProcessChild::TakeInitialEndpoint() {
  return UntypedEndpoint{PrivateIPDLInterface{},
                         child_thread()->TakeInitialPort(), mMessageChannelId,
                         base::GetCurrentProcId(), mParentPid};
}

}  // namespace ipc
}  // namespace mozilla
