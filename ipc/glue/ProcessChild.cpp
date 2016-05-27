/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDebug.h"

#ifdef XP_WIN
#include <stdlib.h> // for _exit()
#else
#include <unistd.h> // for _exit()
#endif

#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/ipc/ProcessChild.h"

namespace mozilla {
namespace ipc {

ProcessChild* ProcessChild::gProcessChild;

ProcessChild::ProcessChild(ProcessId aParentPid)
  : ChildProcess(new IOThreadChild())
  , mUILoop(MessageLoop::current())
  , mParentPid(aParentPid)
{
  MOZ_ASSERT(mUILoop, "UILoop should be created by now");
  MOZ_ASSERT(!gProcessChild, "should only be one ProcessChild");
  gProcessChild = this;
}

ProcessChild::~ProcessChild()
{
  gProcessChild = nullptr;
}

/* static */ void
ProcessChild::QuickExit()
{
#ifdef XP_WIN
  // In bug 1254829, the destructor got called when dll got detached on windows,
  // switch to TerminateProcess to bypass dll detach handler during the process
  // termination.
  TerminateProcess(GetCurrentProcess(), 0);
#else
  _exit(0);
#endif
}

} // namespace ipc
} // namespace mozilla
