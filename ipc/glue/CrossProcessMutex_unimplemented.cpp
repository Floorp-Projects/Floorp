/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrossProcessMutex.h"

#include "nsDebug.h"

namespace mozilla {

CrossProcessMutex::CrossProcessMutex(const char*)
{
  MOZ_CRASH("Cross-process mutices not allowed on this platform.");
}

CrossProcessMutex::CrossProcessMutex(CrossProcessMutexHandle)
{
  MOZ_CRASH("Cross-process mutices not allowed on this platform.");
}

CrossProcessMutex::~CrossProcessMutex()
{
  MOZ_CRASH("Cross-process mutices not allowed on this platform - woah! We should've aborted by now!");
}

void
CrossProcessMutex::Lock()
{
  MOZ_CRASH("Cross-process mutices not allowed on this platform - woah! We should've aborted by now!");
}

void
CrossProcessMutex::Unlock()
{
  MOZ_CRASH("Cross-process mutices not allowed on this platform - woah! We should've aborted by now!");
}

CrossProcessMutexHandle
CrossProcessMutex::ShareToProcess(base::ProcessId aTargetPid)
{
  MOZ_CRASH("Cross-process mutices not allowed on this platform - woah! We should've aborted by now!");
  return 0;
}

}
