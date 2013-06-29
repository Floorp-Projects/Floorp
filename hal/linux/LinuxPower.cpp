/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

#include <unistd.h>
#include <sys/reboot.h>
#include "nsIObserverService.h"

namespace mozilla {
namespace hal_impl {

void
Reboot()
{
  nsCOMPtr<nsIObserverService> obsServ = services::GetObserverService();
  if (obsServ) {
    obsServ->NotifyObservers(nullptr, "system-reboot", nullptr);
  }
  sync();
  reboot(RB_AUTOBOOT);
}

void
PowerOff()
{
  nsCOMPtr<nsIObserverService> obsServ = services::GetObserverService();
  if (obsServ) {
    obsServ->NotifyObservers(nullptr, "system-power-off", nullptr);
  }
  sync();
  reboot(RB_POWER_OFF);
}

// Structure to specify how watchdog pthread is going to work.
typedef struct watchdogParam
{
  hal::ShutdownMode mode; // Specify how to shutdown the system.
  int32_t timeoutSecs;    // Specify the delayed seconds to shutdown the system.

  watchdogParam(hal::ShutdownMode aMode, int32_t aTimeoutSecs)
    : mode(aMode), timeoutSecs(aTimeoutSecs) {}
} watchdogParam_t;

// Function to complusively shut down the system with a given mode.
static void
QuitHard(hal::ShutdownMode aMode)
{
  switch (aMode)
  {
    case hal::eHalShutdownMode_PowerOff:
      PowerOff();
      break;
    case hal::eHalShutdownMode_Reboot:
      Reboot();
      break;
    case hal::eHalShutdownMode_Restart:
      // Don't let signal handlers affect forced shutdown.
      kill(0, SIGKILL);
      // If we can't SIGKILL our process group, something is badly
      // wrong.  Trying to deliver a catch-able signal to ourselves can
      // invoke signal handlers and might cause problems.  So try
      // _exit() and hope we go away.
      _exit(1);
      break;
    default:
      MOZ_CRASH();
      break;
  }
  MOZ_CRASH();
}

// Function to complusively shut down the system with a given mode when timeout.
static void*
ForceQuitWatchdog(void* aParamPtr)
{
  watchdogParam_t* paramPtr = reinterpret_cast<watchdogParam_t*>(aParamPtr);
  if (paramPtr->timeoutSecs > 0 && paramPtr->timeoutSecs <= 30) {
    // If we shut down normally before the timeout, this thread will
    // be harmlessly reaped by the OS.
    TimeStamp deadline =
      (TimeStamp::Now() + TimeDuration::FromSeconds(paramPtr->timeoutSecs));
    while (true) {
      TimeDuration remaining = (deadline - TimeStamp::Now());
      int sleepSeconds = int(remaining.ToSeconds());
      if (sleepSeconds <= 0) {
        break;
      }
      sleep(sleepSeconds);
    }
  }
  hal::ShutdownMode mode = paramPtr->mode;
  delete paramPtr;
  QuitHard(mode);
  return nullptr;
}

void
StartForceQuitWatchdog(hal::ShutdownMode aMode, int32_t aTimeoutSecs)
{
  // Force-quits are intepreted a little more ferociously on Gonk,
  // because while Gecko is in the process of shutting down, the user
  // can't call 911, for example.  And if we hang on shutdown, bad
  // things happen.  So, make sure that doesn't happen.
  if (aTimeoutSecs <= 0) {
    return;
  }

  // Use a raw pthread here to insulate ourselves from bugs in other
  // Gecko code that we're trying to protect!
  // 
  // Note that we let the watchdog in charge of releasing |paramPtr|
  // if the pthread is successfully created.
  watchdogParam_t* paramPtr = new watchdogParam_t(aMode, aTimeoutSecs);
  pthread_t watchdog;
  if (pthread_create(&watchdog, nullptr,
                     ForceQuitWatchdog,
                     reinterpret_cast<void*>(paramPtr))) {
    // Better safe than sorry.
    delete paramPtr;
    QuitHard(aMode);
  }
  // The watchdog thread is off and running now.
}

} // hal_impl
} // mozilla
