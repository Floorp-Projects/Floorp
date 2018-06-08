/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalLog.h"

#include <Windows.h>

using namespace mozilla::hal;

namespace mozilla {
namespace hal_impl {

bool
SetProcessPrioritySupported()
{
  return true;
}

void
SetProcessPriority(int aPid, ProcessPriority aPriority)
{
  HAL_LOG("WindowsProcessPriority - SetProcessPriority(%d, %s)\n",
          aPid, ProcessPriorityToString(aPriority));

  HANDLE hProcess = ::OpenProcess(PROCESS_SET_INFORMATION, false, aPid);
  if (hProcess) {
    DWORD priority = NORMAL_PRIORITY_CLASS;
    if (aPriority == PROCESS_PRIORITY_BACKGROUND ||
        aPriority == PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE) {
      priority = IDLE_PRIORITY_CLASS;
    }
    ::SetPriorityClass(hProcess, priority);
    ::CloseHandle(hProcess);
  }

  HAL_LOG("WindowsProcessPriority - priority set to %d for pid %d\n",
          aPriority, aPid);
}

} // namespace hal_impl
} // namespace mozilla
