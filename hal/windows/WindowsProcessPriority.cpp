/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalLog.h"

#include <windows.h>

using namespace mozilla::hal;

namespace mozilla {
namespace hal_impl {

bool SetProcessPrioritySupported() { return true; }

void SetProcessPriority(int aPid, ProcessPriority aPriority) {
  HAL_LOG("WindowsProcessPriority - SetProcessPriority(%d, %s)\n", aPid,
          ProcessPriorityToString(aPriority));

  nsAutoHandle processHandle(
      ::OpenProcess(PROCESS_SET_INFORMATION, FALSE, aPid));
#ifdef DEBUG
  if (!processHandle) {
    printf_stderr("::OpenProcess() failed with error %#08x\n",
                  ::GetLastError());
  }
#endif  // DEBUG
  MOZ_ASSERT(processHandle);
  if (processHandle) {
    DWORD priority = NORMAL_PRIORITY_CLASS;
    if (aPriority == PROCESS_PRIORITY_BACKGROUND) {
      priority = IDLE_PRIORITY_CLASS;
    } else if (aPriority == PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE) {
      priority = BELOW_NORMAL_PRIORITY_CLASS;
    }
    ::SetPriorityClass(processHandle, priority);
  }

  HAL_LOG("WindowsProcessPriority - priority set to %d for pid %d\n", aPriority,
          aPid);
}

}  // namespace hal_impl
}  // namespace mozilla
