/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
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
  typedef base::ProcessHandle ProcessHandle;

public:
  explicit ProcessChild(ProcessHandle parentHandle);
  virtual ~ProcessChild();

  virtual bool Init() = 0;
  virtual void CleanUp()
  { }

  static MessageLoop* message_loop() {
    return gProcessChild->mUILoop;
  }

protected:
  static ProcessChild* current() {
    return gProcessChild;
  }

  ProcessHandle ParentHandle() {
    return mParentHandle;
  }

private:
  static ProcessChild* gProcessChild;

  MessageLoop* mUILoop;
  ProcessHandle mParentHandle;

  DISALLOW_EVIL_CONSTRUCTORS(ProcessChild);
};

} // namespace ipc
} // namespace mozilla


#endif // ifndef mozilla_ipc_ProcessChild_h
