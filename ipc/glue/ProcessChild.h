/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ProcessChild_h
#define mozilla_ipc_ProcessChild_h

#include "base/message_loop.h"
#include "base/process.h"

#include "chrome/common/child_process.h"

// ProcessChild is the base class for all subprocesses of the main
// browser process.  Its code runs on the thread that started in
// main().

namespace mozilla {
namespace ipc {

class ProcessChild : public ChildProcess {
protected:
  typedef base::ProcessId ProcessId;

public:
  explicit ProcessChild(ProcessId aParentPid);
  virtual ~ProcessChild();

  virtual bool Init() = 0;
  virtual void CleanUp()
  { }

  static MessageLoop* message_loop() {
    return gProcessChild->mUILoop;
  }

    /**
   * Exit *now*.  Do not shut down XPCOM, do not pass Go, do not run
   * static destructors, do not collect $200.
   */
  static void QuickExit();

protected:
  static ProcessChild* current() {
    return gProcessChild;
  }

  ProcessId ParentPid() {
    return mParentPid;
  }

private:
  static ProcessChild* gProcessChild;

  MessageLoop* mUILoop;
  ProcessId mParentPid;

  DISALLOW_EVIL_CONSTRUCTORS(ProcessChild);
};

} // namespace ipc
} // namespace mozilla


#endif // ifndef mozilla_ipc_ProcessChild_h
