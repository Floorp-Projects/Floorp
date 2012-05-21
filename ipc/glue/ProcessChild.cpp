/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDebug.h"

#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/ipc/ProcessChild.h"

namespace mozilla {
namespace ipc {

ProcessChild* ProcessChild::gProcessChild;

ProcessChild::ProcessChild(ProcessHandle parentHandle)
  : ChildProcess(new IOThreadChild())
  , mUILoop(MessageLoop::current())
  , mParentHandle(parentHandle)
{
  NS_ABORT_IF_FALSE(mUILoop, "UILoop should be created by now");
  NS_ABORT_IF_FALSE(!gProcessChild, "should only be one ProcessChild");
  gProcessChild = this;
}

ProcessChild::~ProcessChild()
{
  gProcessChild = NULL;
}

} // namespace ipc
} // namespace mozilla
