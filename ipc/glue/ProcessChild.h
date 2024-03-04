/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ProcessChild_h
#define mozilla_ipc_ProcessChild_h

#include "Endpoint.h"
#include "base/message_loop.h"
#include "base/process.h"

#include "chrome/common/child_process.h"

#include "mozilla/ipc/ProcessUtils.h"

// ProcessChild is the base class for all subprocesses of the main
// browser process.  Its code runs on the thread that started in
// main().

namespace mozilla {
namespace ipc {

class ProcessChild : public ChildProcess {
 protected:
  typedef base::ProcessId ProcessId;

 public:
  explicit ProcessChild(ProcessId aParentPid, const nsID& aMessageChannelId);

  ProcessChild(const ProcessChild&) = delete;
  ProcessChild& operator=(const ProcessChild&) = delete;

  virtual ~ProcessChild();

  virtual bool Init(int aArgc, char* aArgv[]) = 0;

  static void AddPlatformBuildID(std::vector<std::string>& aExtraArgs);

  static bool InitPrefs(int aArgc, char* aArgv[]);

  virtual void CleanUp() {}

  static MessageLoop* message_loop() { return gProcessChild->mUILoop; }

  static void NotifiedImpendingShutdown();

  static bool ExpectingShutdown();

  static void AppendToIPCShutdownStateAnnotation(const nsCString& aStr) {
    StaticMutexAutoLock lock(gIPCShutdownStateLock);
    gIPCShutdownStateAnnotation.Append(" - "_ns);
    gIPCShutdownStateAnnotation.Append(aStr);
  }

  /**
   * Exit *now*.  Do not shut down XPCOM, do not pass Go, do not run
   * static destructors, do not collect $200.
   */
  static void QuickExit();

 protected:
  static ProcessChild* current() { return gProcessChild; }

  ProcessId ParentPid() { return mParentPid; }

  UntypedEndpoint TakeInitialEndpoint();

 private:
  static ProcessChild* gProcessChild;
  static StaticMutex gIPCShutdownStateLock;
  static nsCString gIPCShutdownStateAnnotation
      MOZ_GUARDED_BY(gIPCShutdownStateLock);

  MessageLoop* mUILoop;
  ProcessId mParentPid;
  nsID mMessageChannelId;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // ifndef mozilla_ipc_ProcessChild_h
