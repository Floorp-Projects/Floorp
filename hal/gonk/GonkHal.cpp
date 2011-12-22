/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *   Michael Wu <mwu@mozilla.com>
 *   Justin Lebar <justin.lebar@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "hardware_legacy/uevent.h"
#include "Hal.h"
#include "mozilla/dom/battery/Constants.h"
#include "mozilla/FileUtils.h"
#include "nsAlgorithm.h"
#include "nsThreadUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/Services.h"
#include "mozilla/FileUtils.h"
#include "nsThreadUtils.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "hardware_legacy/vibrator.h"
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>

using mozilla::hal::WindowIdentifier;

namespace mozilla {
namespace hal_impl {

namespace {

/**
 * This runnable runs for the lifetime of the program, once started.  It's
 * responsible for "playing" vibration patterns.
 */
class VibratorRunnable
  : public nsIRunnable
  , public nsIObserver
{
public:
  VibratorRunnable()
    : mMonitor("VibratorRunnable")
    , mIndex(0)
    , mShuttingDown(false)
  {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (!os) {
      NS_WARNING("Could not get observer service!");
      return;
    }

    os->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, /* weak ref */ true);
  } 
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  // Run on the main thread, not the vibrator thread.
  void Vibrate(const nsTArray<uint32> &pattern);
  void CancelVibrate();

private:
  Monitor mMonitor;

  // The currently-playing pattern.
  nsTArray<uint32> mPattern;

  // The index we're at in the currently-playing pattern.  If mIndex >=
  // mPattern.Length(), then we're not currently playing anything.
  uint32 mIndex;

  // Set to true in our shutdown observer.  When this is true, we kill the
  // vibrator thread.
  bool mShuttingDown;
};

NS_IMPL_ISUPPORTS2(VibratorRunnable, nsIRunnable, nsIObserver);

NS_IMETHODIMP
VibratorRunnable::Run()
{
  MonitorAutoLock lock(mMonitor);

  // We currently assume that mMonitor.Wait(X) waits for X milliseconds.  But in
  // reality, the kernel might not switch to this thread for some time after the
  // wait expires.  So there's potential for some inaccuracy here.
  //
  // This doesn't worry me too much.  Note that we don't even start vibrating
  // immediately when VibratorRunnable::Vibrate is called -- we go through a
  // condvar onto another thread.  Better just to be chill about small errors in
  // the timing here.

  while (!mShuttingDown) {
    if (mIndex < mPattern.Length()) {
      uint32 duration = mPattern[mIndex];
      if (mIndex % 2 == 0) {
        vibrator_on(duration);
      }
      mIndex++;
      mMonitor.Wait(PR_MillisecondsToInterval(duration));
    }
    else {
      mMonitor.Wait();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
VibratorRunnable::Observe(nsISupports *subject, const char *topic,
                          const PRUnichar *data)
{
  MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);
  MonitorAutoLock lock(mMonitor);
  mShuttingDown = true;
  mMonitor.Notify();
  return NS_OK;
}

void
VibratorRunnable::Vibrate(const nsTArray<uint32> &pattern)
{
  MonitorAutoLock lock(mMonitor);
  mPattern = pattern;
  mIndex = 0;
  mMonitor.Notify();
}

void
VibratorRunnable::CancelVibrate()
{
  MonitorAutoLock lock(mMonitor);
  mPattern.Clear();
  mPattern.AppendElement(0);
  mIndex = 0;
  mMonitor.Notify();
}

VibratorRunnable *sVibratorRunnable = NULL;

void
EnsureVibratorThreadInitialized()
{
  if (sVibratorRunnable) {
    return;
  }

  nsRefPtr<VibratorRunnable> runnable = new VibratorRunnable();
  sVibratorRunnable = runnable;
  nsCOMPtr<nsIThread> thread;
  NS_NewThread(getter_AddRefs(thread), sVibratorRunnable);
}

} // anonymous namespace

void
Vibrate(const nsTArray<uint32> &pattern, const hal::WindowIdentifier &)
{
  EnsureVibratorThreadInitialized();
  sVibratorRunnable->Vibrate(pattern);
}

void
CancelVibrate(const hal::WindowIdentifier &)
{
  EnsureVibratorThreadInitialized();
  sVibratorRunnable->CancelVibrate();
}

namespace {

class BatteryUpdater : public nsRunnable {
public:
  NS_IMETHOD Run()
  {
    hal::BatteryInformation info;
    hal_impl::GetCurrentBatteryInformation(&info);
    hal::NotifyBatteryChange(info);
    return NS_OK;
  }
};

class UEventWatcher : public nsRunnable {
public:
  UEventWatcher()
    : mUpdater(new BatteryUpdater())
    , mRunning(false)
  {
  }

  NS_IMETHOD Run()
  {
    while (mRunning) {
      char buf[1024];
      int count = uevent_next_event(buf, sizeof(buf) - 1);
      if (!count) {
        NS_WARNING("uevent_next_event() returned 0!");
        continue;
      }

      buf[sizeof(buf) - 1] = 0;
      if (strstr(buf, "battery"))
        NS_DispatchToMainThread(mUpdater);
    }
    return NS_OK;
  }

  bool mRunning;

private:
  nsRefPtr<BatteryUpdater> mUpdater;
};

} // anonymous namespace

static bool sUEventInitialized = false;
static UEventWatcher *sWatcher = NULL;
static nsIThread *sWatcherThread = NULL;

void
EnableBatteryNotifications()
{
  if (!sUEventInitialized)
    sUEventInitialized = uevent_init();
  if (!sUEventInitialized) {
    NS_WARNING("uevent_init() failed!");
    return;
  }

  if (!sWatcher)
    sWatcher = new UEventWatcher();
  NS_ADDREF(sWatcher);

  sWatcher->mRunning = true;
  nsresult rv = NS_NewThread(&sWatcherThread, sWatcher);
  if (NS_FAILED(rv))
    NS_WARNING("Failed to get new thread for uevent watching");
}

void
DisableBatteryNotifications()
{
  sWatcher->mRunning = false;
  sWatcherThread->Shutdown();
  NS_IF_RELEASE(sWatcherThread);
  delete sWatcher;
}

void
GetCurrentBatteryInformation(hal::BatteryInformation *aBatteryInfo)
{
  FILE *capacityFile = fopen("/sys/class/power_supply/battery/capacity", "r");
  double capacity = dom::battery::kDefaultLevel * 100;
  if (capacityFile)
    fscanf(capacityFile, "%lf", &capacity);
  fclose(capacityFile);

  FILE *chargingFile = fopen("/sys/class/power_supply/battery/charging_source", "r");
  int chargingSrc = 1;
  if (chargingFile)
    fscanf(chargingFile, "%d", &chargingSrc);
  fclose(chargingFile);

  aBatteryInfo->level() = capacity / 100;
  aBatteryInfo->charging() = chargingSrc == 1;
  aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
}

namespace {

/**
 * RAII class to help us remember to close file descriptors.
 */
const char *screenEnabledFilename = "/sys/power/state";
const char *screenBrightnessFilename = "/sys/class/backlight/pwm-backlight/brightness";

template<ssize_t n>
bool ReadFromFile(const char *filename, char (&buf)[n])
{
  int fd = open(filename, O_RDONLY);
  ScopedClose autoClose(fd);
  if (fd < 0) {
    HAL_LOG(("Unable to open file %s.", filename));
    return false;
  }

  ssize_t numRead = read(fd, buf, n);
  if (numRead < 0) {
    HAL_LOG(("Error reading from file %s.", filename));
    return false;
  }

  buf[PR_MIN(numRead, n - 1)] = '\0';
  return true;
}

void WriteToFile(const char *filename, const char *toWrite)
{
  int fd = open(filename, O_WRONLY);
  ScopedClose autoClose(fd);
  if (fd < 0) {
    HAL_LOG(("Unable to open file %s.", filename));
    return;
  }

  if (write(fd, toWrite, strlen(toWrite)) < 0) {
    HAL_LOG(("Unable to write to file %s.", filename));
    return;
  }
}

// We can write to screenEnabledFilename to enable/disable the screen, but when
// we read, we always get "mem"!  So we have to keep track ourselves whether
// the screen is on or not.
bool sScreenEnabled = true;

} // anonymous namespace

bool
GetScreenEnabled()
{
  return sScreenEnabled;
}

void
SetScreenEnabled(bool enabled)
{
  WriteToFile(screenEnabledFilename, enabled ? "on" : "mem");
  sScreenEnabled = enabled;
}

double
GetScreenBrightness()
{
  char buf[32];
  ReadFromFile(screenBrightnessFilename, buf);

  errno = 0;
  unsigned long val = strtoul(buf, NULL, 10);
  if (errno) {
    HAL_LOG(("Cannot parse contents of %s; expected an unsigned "
             "int, but contains \"%s\".",
             screenBrightnessFilename, buf));
    return 1;
  }

  if (val > 255) {
    HAL_LOG(("Got out-of-range brightness %d, truncating to 1.0", val));
    val = 255;
  }

  return val / 255.0;
}

void
SetScreenBrightness(double brightness)
{
  // Don't use De Morgan's law to push the ! into this expression; we want to
  // catch NaN too.
  if (!(0 <= brightness && brightness <= 1)) {
    HAL_LOG(("SetScreenBrightness: Dropping illegal brightness %f.",
             brightness));
    return;
  }

  // Convert the value in [0, 1] to an int between 0 and 255, then write to a
  // string.
  int val = static_cast<int>(round(brightness * 255));
  char str[4];
  DebugOnly<int> numChars = snprintf(str, sizeof(str), "%d", val);
  MOZ_ASSERT(numChars < static_cast<int>(sizeof(str)));

  WriteToFile(screenBrightnessFilename, str);
}

} // hal_impl
} // mozilla
