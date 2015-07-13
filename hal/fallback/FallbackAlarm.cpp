/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

#include <algorithm>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace hal_impl {

static void
TimerCallbackFunc(nsITimer *aTimer, void *aClosure)
{
  hal::NotifyAlarmFired();
}

static StaticRefPtr<nsITimer> sTimer;

bool
EnableAlarm()
{
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    ClearOnShutdown(&sTimer);
  }

  nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
  sTimer = timer;
  MOZ_ASSERT(sTimer);
  return true;
}

void
DisableAlarm()
{
  /*
   * DisableAlarm() may be called after sTimer has been set to null by
   * ClearOnShutdown().
   */
  if (sTimer) {
    sTimer->Cancel();
  }
}

bool
SetAlarm(int32_t aSeconds, int32_t aNanoseconds)
{
  if (!sTimer) {
    MOZ_ASSERT(false, "We should have enabled the alarm");
    return false;
  }

  // Do the math to convert aSeconds and aNanoseconds into milliseconds since
  // the epoch.
  int64_t milliseconds = static_cast<int64_t>(aSeconds) * 1000 +
                         static_cast<int64_t>(aNanoseconds) / 1000000;

  // nsITimer expects relative milliseconds.
  int64_t relMilliseconds = milliseconds - PR_Now() / 1000;

  // If the alarm time is in the past relative to PR_Now(),
  // we choose to immediately fire the alarm. Passing 0 means nsITimer will
  // queue a timeout event immediately.
  sTimer->InitWithFuncCallback(TimerCallbackFunc, nullptr,
                               clamped<int64_t>(relMilliseconds, 0, INT32_MAX),
                               nsITimer::TYPE_ONE_SHOT);
  return true;
}

} // namespace hal_impl
} // namespace mozilla
