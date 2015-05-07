/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalLog.h"

using namespace mozilla::hal;

namespace mozilla {
namespace hal_impl {

void
SetCurrentThreadPriority(ThreadPriority aPriority)
{
  HAL_LOG("FallbackThreadPriority - SetCurrentThreadPriority(%d)\n",
          ThreadPriorityToString(aPriority));
}

void
SetThreadPriority(PlatformThreadId aThreadId,
                  ThreadPriority aPriority)
{
  HAL_LOG("FallbackThreadPriority - SetThreadPriority(%d, %d)\n",
          aThreadId, ThreadPriorityToString(aPriority));
}

} // hal_impl
} // namespace mozilla
