/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalLog.h"
#include "nsWindowsHelpers.h"  // for nsAutoHandle and nsModuleHandle
#include "mozilla/StaticPrefs_dom.h"

#include <windows.h>

using namespace mozilla::hal;

namespace mozilla {
namespace hal_impl {

void SetProcessPriority(int aPid, ProcessPriority aPriority) {
  HAL_LOG("WindowsProcessPriority - SetProcessPriority(%d, %s)\n", aPid,
          ProcessPriorityToString(aPriority));

  nsAutoHandle processHandle(
      ::OpenProcess(PROCESS_SET_INFORMATION, FALSE, aPid));
  if (processHandle) {
    DWORD priority = NORMAL_PRIORITY_CLASS;
    if (aPriority == PROCESS_PRIORITY_BACKGROUND) {
      priority = IDLE_PRIORITY_CLASS;
    } else if (aPriority == PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE) {
      priority = BELOW_NORMAL_PRIORITY_CLASS;
    }

    if (::SetPriorityClass(processHandle, priority)) {
      HAL_LOG("WindowsProcessPriority - priority set to %d for pid %d\n",
              aPriority, aPid);
    }

    // Set the process into or out of EcoQoS.
    static bool alreadyInitialized = false;
    static decltype(::SetProcessInformation)* setProcessInformation = nullptr;
    if (!alreadyInitialized) {
      if (aPriority == PROCESS_PRIORITY_PARENT_PROCESS ||
          !StaticPrefs::dom_ipc_processPriorityManager_backgroundUsesEcoQoS()) {
        return;
      }

      alreadyInitialized = true;
      // SetProcessInformation only exists on Windows 8 and later.
      nsModuleHandle module(LoadLibrary(L"Kernel32.dll"));
      if (module) {
        setProcessInformation =
            (decltype(::SetProcessInformation)*)GetProcAddress(
                module, "SetProcessInformation");
      }
    }
    if (!setProcessInformation) {
      return;
    }

    PROCESS_POWER_THROTTLING_STATE PowerThrottling;
    RtlZeroMemory(&PowerThrottling, sizeof(PowerThrottling));
    PowerThrottling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    PowerThrottling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    PowerThrottling.StateMask =
        (aPriority == PROCESS_PRIORITY_BACKGROUND) &&
                StaticPrefs::
                    dom_ipc_processPriorityManager_backgroundUsesEcoQoS()
            ? PROCESS_POWER_THROTTLING_EXECUTION_SPEED
            : 0;
    if (setProcessInformation(processHandle, ProcessPowerThrottling,
                              &PowerThrottling, sizeof(PowerThrottling))) {
      HAL_LOG("SetProcessInformation(%d, %s)\n", aPid,
              aPriority == PROCESS_PRIORITY_BACKGROUND ? "eco" : "normal");
    } else {
      HAL_LOG("SetProcessInformation failed for %d\n", aPid);
    }
  }
}

}  // namespace hal_impl
}  // namespace mozilla
