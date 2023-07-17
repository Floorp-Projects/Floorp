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
#  include <synchapi.h>
#else
#  include <unistd.h>  // for _exit()
#  include <time.h>
#  include "base/eintr_wrapper.h"
#  include "prenv.h"
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
    : ChildProcess(new IOThreadChild(aParentPid)),
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

#ifdef ENABLE_TESTS
// Allow tests to cause a synthetic delay/"hang" during child process
// shutdown by setting environment variables.
#  ifdef XP_UNIX
static void ReallySleep(int aSeconds) {
  struct ::timespec snooze = {aSeconds, 0};
  HANDLE_EINTR(nanosleep(&snooze, &snooze));
}
#  else
static void ReallySleep(int aSeconds) { ::Sleep(aSeconds * 1000); }
#  endif  // Unix/Win
static void SleepIfEnv(const char* aName) {
  if (auto* value = PR_GetEnv(aName)) {
    ReallySleep(atoi(value));
  }
}
#else  // not tests
static void SleepIfEnv(const char* aName) {}
#endif

ProcessChild::~ProcessChild() {
#ifdef NS_FREE_PERMANENT_DATA
  // In this case, we won't early-exit and we'll wait indefinitely for
  // child processes to terminate.  This sleep is late enough that, in
  // content processes, it won't block parent process shutdown, so
  // we'll get into late IPC shutdown with processes still running.
  SleepIfEnv("MOZ_TEST_CHILD_EXIT_HANG");
#endif
  gProcessChild = nullptr;
}

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
void ProcessChild::QuickExit() {
#ifndef NS_FREE_PERMANENT_DATA
  // In this case, we're going to terminate the child process before
  // we get to ~ProcessChild above (and terminate the parent process
  // before the shutdown hook in ProcessWatcher).  Instead, blocking
  // earlier will let us exercise ProcessWatcher's kill timer.
  SleepIfEnv("MOZ_TEST_CHILD_EXIT_HANG");
#endif
  AppShutdown::DoImmediateExit();
}

UntypedEndpoint ProcessChild::TakeInitialEndpoint() {
  return UntypedEndpoint{PrivateIPDLInterface{},
                         child_thread()->TakeInitialPort(), mMessageChannelId,
                         base::GetCurrentProcId(), mParentPid};
}

}  // namespace ipc
}  // namespace mozilla
