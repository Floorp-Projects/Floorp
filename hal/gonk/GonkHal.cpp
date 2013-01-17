/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/android_alarm.h>
#include <math.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <time.h>
#include <asm/page.h>

#include "mozilla/DebugOnly.h"

#include "android/log.h"
#include "cutils/properties.h"
#include "hardware/hardware.h"
#include "hardware/lights.h"
#include "hardware_legacy/uevent.h"
#include "hardware_legacy/vibrator.h"
#include "hardware_legacy/power.h"

#include "base/message_loop.h"

#include "Hal.h"
#include "HalImpl.h"
#include "mozilla/dom/battery/Constants.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Preferences.h"
#include "nsAlgorithm.h"
#include "nsPrintfCString.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIRecoveryService.h"
#include "nsIRunnable.h"
#include "nsScreenManagerGonk.h"
#include "nsThreadUtils.h"
#include "nsThreadUtils.h"
#include "nsIThread.h"
#include "nsXULAppAPI.h"
#include "OrientationObserver.h"
#include "UeventPoller.h"
#include <algorithm>

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#define NsecPerMsec  1000000LL
#define NsecPerSec   1000000000

// The header linux/oom.h is not available in bionic libc. We
// redefine some of its constants here.

#ifndef OOM_DISABLE
#define OOM_DISABLE  (-17)
#endif

#ifndef OOM_ADJUST_MIN
#define OOM_ADJUST_MIN  (-16)
#endif

#ifndef OOM_ADJUST_MAX
#define OOM_ADJUST_MAX  15
#endif

#ifndef OOM_SCORE_ADJ_MIN
#define OOM_SCORE_ADJ_MIN  (-1000)
#endif

#ifndef OOM_SCORE_ADJ_MAX
#define OOM_SCORE_ADJ_MAX  1000
#endif

#ifndef BATTERY_CHARGING_ARGB
#define BATTERY_CHARGING_ARGB 0x00FF0000
#endif
#ifndef BATTERY_FULL_ARGB
#define BATTERY_FULL_ARGB 0x0000FF00
#endif

using namespace mozilla;
using namespace mozilla::hal;

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
  {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (!os) {
      NS_WARNING("Could not get observer service!");
      return;
    }

    os->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  // Run on the main thread, not the vibrator thread.
  void Vibrate(const nsTArray<uint32_t> &pattern);
  void CancelVibrate();

  static bool ShuttingDown() { return sShuttingDown; }

private:
  Monitor mMonitor;

  // The currently-playing pattern.
  nsTArray<uint32_t> mPattern;

  // The index we're at in the currently-playing pattern.  If mIndex >=
  // mPattern.Length(), then we're not currently playing anything.
  uint32_t mIndex;

  // Set to true in our shutdown observer.  When this is true, we kill the
  // vibrator thread.
  static bool sShuttingDown;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(VibratorRunnable, nsIRunnable, nsIObserver);

bool VibratorRunnable::sShuttingDown = false;

static nsRefPtr<VibratorRunnable> sVibratorRunnable;

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

  while (!sShuttingDown) {
    if (mIndex < mPattern.Length()) {
      uint32_t duration = mPattern[mIndex];
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
  sVibratorRunnable = NULL;
  return NS_OK;
}

NS_IMETHODIMP
VibratorRunnable::Observe(nsISupports *subject, const char *topic,
                          const PRUnichar *data)
{
  MOZ_ASSERT(strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);
  MonitorAutoLock lock(mMonitor);
  sShuttingDown = true;
  mMonitor.Notify();

  return NS_OK;
}

void
VibratorRunnable::Vibrate(const nsTArray<uint32_t> &pattern)
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

void
EnsureVibratorThreadInitialized()
{
  if (sVibratorRunnable) {
    return;
  }

  sVibratorRunnable = new VibratorRunnable();
  nsCOMPtr<nsIThread> thread;
  NS_NewThread(getter_AddRefs(thread), sVibratorRunnable);
}

} // anonymous namespace

void
Vibrate(const nsTArray<uint32_t> &pattern, const hal::WindowIdentifier &)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (VibratorRunnable::ShuttingDown()) {
    return;
  }
  EnsureVibratorThreadInitialized();
  sVibratorRunnable->Vibrate(pattern);
}

void
CancelVibrate(const hal::WindowIdentifier &)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (VibratorRunnable::ShuttingDown()) {
    return;
  }
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

    // Control the battery indicator (led light) here using BatteryInformation
    // we just retrieved.
    uint32_t color = 0; // Format: 0x00rrggbb.
    if (info.charging() && (info.level() == 1)) {
      // Charging and battery full.
      color = BATTERY_FULL_ARGB;
    } else if (info.charging() && (info.level() < 1)) {
      // Charging but not full.
      color = BATTERY_CHARGING_ARGB;
    } // else turn off battery indicator.

    hal::LightConfiguration aConfig(hal::eHalLightID_Battery,
                                    hal::eHalLightMode_User,
                                    hal::eHalLightFlash_None,
                                    0,
                                    0,
                                    color);
    hal_impl::SetLight(hal::eHalLightID_Battery, aConfig);

    hal::NotifyBatteryChange(info);
    return NS_OK;
  }
};

} // anonymous namespace

class BatteryObserver : public IUeventObserver,
                        public RefCounted<BatteryObserver>
{
public:
  BatteryObserver()
    :mUpdater(new BatteryUpdater())
  {
  }

  virtual void Notify(const NetlinkEvent &aEvent)
  {
    // this will run on IO thread
    NetlinkEvent *event = const_cast<NetlinkEvent*>(&aEvent);
    const char *subsystem = event->getSubsystem();
    // e.g. DEVPATH=/devices/platform/sec-battery/power_supply/battery
    const char *devpath = event->findParam("DEVPATH");
    if (strcmp(subsystem, "power_supply") == 0 &&
        strstr(devpath, "battery")) {
      // aEvent will be valid only in this method.
      NS_DispatchToMainThread(mUpdater);
    }
  }

private:
  nsRefPtr<BatteryUpdater> mUpdater;
};

// sBatteryObserver is owned by the IO thread. Only the IO thread may
// create or destroy it.
static StaticRefPtr<BatteryObserver> sBatteryObserver;

static void
RegisterBatteryObserverIOThread()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(!sBatteryObserver);

  sBatteryObserver = new BatteryObserver();
  RegisterUeventListener(sBatteryObserver);
}

void
EnableBatteryNotifications()
{
  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(RegisterBatteryObserverIOThread));
}

static void
UnregisterBatteryObserverIOThread()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(sBatteryObserver);

  UnregisterUeventListener(sBatteryObserver);
  sBatteryObserver = NULL;
}

void
DisableBatteryNotifications()
{
  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(UnregisterBatteryObserverIOThread));
}

// See bug 819016 about moving the ReadSysFile functions to a
// central location.
static bool
ReadSysFile(const char *aFilename, char *aBuf, size_t aBufSize,
            size_t *aBytesRead = NULL)
{
  int fd = TEMP_FAILURE_RETRY(open(aFilename, O_RDONLY));
  if (fd < 0) {
    HAL_LOG(("Unable to open file '%s' for reading", aFilename));
    return false;
  }
  ScopedClose autoClose(fd);
  ssize_t bytesRead = TEMP_FAILURE_RETRY(read(fd, aBuf, aBufSize - 1));
  if (bytesRead < 0) {
    HAL_LOG(("Unable to read from file '%s'", aFilename));
    return false;
  }
  if (bytesRead && (aBuf[bytesRead - 1] == '\n')) {
    bytesRead--;
  }
  aBuf[bytesRead] = '\0';
  if (aBytesRead) {
    *aBytesRead = bytesRead;
  }
  return true;
}

static bool
ReadSysFile(const char *aFilename, int *aVal)
{
  char valBuf[20];
  if (!ReadSysFile(aFilename, valBuf, sizeof(valBuf))) {
    return false;
  }
  return sscanf(valBuf, "%d", aVal) == 1;
}

static bool
GetCurrentBatteryCharge(int* aCharge)
{
  bool success = ReadSysFile("/sys/class/power_supply/battery/capacity",
                             aCharge);
  if (!success) {
    return false;
  }

  #ifdef DEBUG
  if ((*aCharge < 0) || (*aCharge > 100)) {
    HAL_LOG(("charge level containes unknown value: %d", *aCharge));
  }
  #endif

  return (*aCharge >= 0) && (*aCharge <= 100);
}

static bool
GetCurrentBatteryCharging(int* aCharging)
{
  static const int BATTERY_NOT_CHARGING = 0;
  static const int BATTERY_CHARGING_USB = 1;
  static const int BATTERY_CHARGING_AC  = 2;

  // Generic device support

  int chargingSrc;
  bool success =
    ReadSysFile("/sys/class/power_supply/battery/charging_source", &chargingSrc);

  if (success) {
    #ifdef DEBUG
    if (chargingSrc != BATTERY_NOT_CHARGING &&
        chargingSrc != BATTERY_CHARGING_USB &&
        chargingSrc != BATTERY_CHARGING_AC) {
      HAL_LOG(("charging_source contained unknown value: %d", chargingSrc));
    }
    #endif

    *aCharging = (chargingSrc == BATTERY_CHARGING_USB ||
                  chargingSrc == BATTERY_CHARGING_AC);
    return true;
  }

  // Otoro device support

  char chargingSrcString[16];
  size_t chargingSrcLen;

  success = ReadSysFile("/sys/class/power_supply/battery/status",
                        chargingSrcString, sizeof(chargingSrcString),
                        &chargingSrcLen);
  if (success) {
    *aCharging = !memcmp(chargingSrcString, "Charging", chargingSrcLen) ||
                 !memcmp(chargingSrcString, "Full", chargingSrcLen);
    return true;
  }

  return false;
}

void
GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo)
{
  int charge;

  if (GetCurrentBatteryCharge(&charge)) {
    aBatteryInfo->level() = (double)charge / 100.0;
  } else {
    aBatteryInfo->level() = dom::battery::kDefaultLevel;
  }

  int charging;

  if (GetCurrentBatteryCharging(&charging)) {
    aBatteryInfo->charging() = charging;
  } else {
    aBatteryInfo->charging() = true;
  }

  if (aBatteryInfo->charging() && (aBatteryInfo->level() < 1.0)) {
    aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
  } else {
    aBatteryInfo->remainingTime() = dom::battery::kDefaultRemainingTime;
  }
}

namespace {

/**
 * RAII class to help us remember to close file descriptors.
 */
const char *wakeLockFilename = "/sys/power/wake_lock";
const char *wakeUnlockFilename = "/sys/power/wake_unlock";

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

  buf[std::min(numRead, n - 1)] = '\0';
  return true;
}

bool WriteToFile(const char *filename, const char *toWrite)
{
  int fd = open(filename, O_WRONLY);
  ScopedClose autoClose(fd);
  if (fd < 0) {
    HAL_LOG(("Unable to open file %s.", filename));
    return false;
  }

  if (write(fd, toWrite, strlen(toWrite)) < 0) {
    HAL_LOG(("Unable to write to file %s.", filename));
    return false;
  }

  return true;
}

// We can write to screenEnabledFilename to enable/disable the screen, but when
// we read, we always get "mem"!  So we have to keep track ourselves whether
// the screen is on or not.
bool sScreenEnabled = true;

// We can read wakeLockFilename to find out whether the cpu wake lock
// is already acquired, but reading and parsing it is a lot more work
// than tracking it ourselves, and it won't be accurate anyway (kernel
// internal wake locks aren't counted here.)
bool sCpuSleepAllowed = true;

} // anonymous namespace

bool
GetScreenEnabled()
{
  return sScreenEnabled;
}

void
SetScreenEnabled(bool enabled)
{
  set_screen_state(enabled);
  sScreenEnabled = enabled;
}

double
GetScreenBrightness()
{
  hal::LightConfiguration aConfig;
  hal::LightType light = hal::eHalLightID_Backlight;

  hal::GetLight(light, &aConfig);
  // backlight is brightness only, so using one of the RGB elements as value.
  int brightness = aConfig.color() & 0xFF;
  return brightness / 255.0;
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

  // Convert the value in [0, 1] to an int between 0 and 255 and convert to a color
  // note that the high byte is FF, corresponding to the alpha channel.
  int val = static_cast<int>(round(brightness * 255));
  uint32_t color = (0xff<<24) + (val<<16) + (val<<8) + val;

  hal::LightConfiguration aConfig;
  aConfig.mode() = hal::eHalLightMode_User;
  aConfig.flash() = hal::eHalLightFlash_None;
  aConfig.flashOnMS() = aConfig.flashOffMS() = 0;
  aConfig.color() = color;
  hal::SetLight(hal::eHalLightID_Backlight, aConfig);
  hal::SetLight(hal::eHalLightID_Buttons, aConfig);
}

bool
GetCpuSleepAllowed()
{
  return sCpuSleepAllowed;
}

void
SetCpuSleepAllowed(bool aAllowed)
{
  WriteToFile(aAllowed ? wakeUnlockFilename : wakeLockFilename, "gecko");
  sCpuSleepAllowed = aAllowed;
}

static light_device_t* sLights[hal::eHalLightID_Count];	// will be initialized to NULL

light_device_t* GetDevice(hw_module_t* module, char const* name)
{
  int err;
  hw_device_t* device;
  err = module->methods->open(module, name, &device);
  if (err == 0) {
    return (light_device_t*)device;
  } else {
    return NULL;
  }
}

void
InitLights()
{
  // assume that if backlight is NULL, nothing has been set yet
  // if this is not true, the initialization will occur everytime a light is read or set!
  if (!sLights[hal::eHalLightID_Backlight]) {
    int err;
    hw_module_t* module;

    err = hw_get_module(LIGHTS_HARDWARE_MODULE_ID, (hw_module_t const**)&module);
    if (err == 0) {
      sLights[hal::eHalLightID_Backlight]
             = GetDevice(module, LIGHT_ID_BACKLIGHT);
      sLights[hal::eHalLightID_Keyboard]
             = GetDevice(module, LIGHT_ID_KEYBOARD);
      sLights[hal::eHalLightID_Buttons]
             = GetDevice(module, LIGHT_ID_BUTTONS);
      sLights[hal::eHalLightID_Battery]
             = GetDevice(module, LIGHT_ID_BATTERY);
      sLights[hal::eHalLightID_Notifications]
             = GetDevice(module, LIGHT_ID_NOTIFICATIONS);
      sLights[hal::eHalLightID_Attention]
             = GetDevice(module, LIGHT_ID_ATTENTION);
      sLights[hal::eHalLightID_Bluetooth]
             = GetDevice(module, LIGHT_ID_BLUETOOTH);
      sLights[hal::eHalLightID_Wifi]
             = GetDevice(module, LIGHT_ID_WIFI);
        }
    }
}

/**
 * The state last set for the lights until liblights supports
 * getting the light state.
 */
static light_state_t sStoredLightState[hal::eHalLightID_Count];

bool
SetLight(hal::LightType light, const hal::LightConfiguration& aConfig)
{
  light_state_t state;

  InitLights();

  if (light < 0 || light >= hal::eHalLightID_Count || sLights[light] == NULL) {
    return false;
  }

  memset(&state, 0, sizeof(light_state_t));
  state.color = aConfig.color();
  state.flashMode = aConfig.flash();
  state.flashOnMS = aConfig.flashOnMS();
  state.flashOffMS = aConfig.flashOffMS();
  state.brightnessMode = aConfig.mode();

  sLights[light]->set_light(sLights[light], &state);
  sStoredLightState[light] = state;
  return true;
}

bool
GetLight(hal::LightType light, hal::LightConfiguration* aConfig)
{
  light_state_t state;

#ifdef HAVEGETLIGHT
  InitLights();
#endif

  if (light < 0 || light >= hal::eHalLightID_Count || sLights[light] == NULL) {
    return false;
  }

  memset(&state, 0, sizeof(light_state_t));

#ifdef HAVEGETLIGHT
  sLights[light]->get_light(sLights[light], &state);
#else
  state = sStoredLightState[light];
#endif

  aConfig->light() = light;
  aConfig->color() = state.color;
  aConfig->flash() = hal::FlashMode(state.flashMode);
  aConfig->flashOnMS() = state.flashOnMS;
  aConfig->flashOffMS() = state.flashOffMS;
  aConfig->mode() = hal::LightMode(state.brightnessMode);

  return true;
}

void
AdjustSystemClock(int64_t aDeltaMilliseconds)
{
  int fd;
  struct timespec now;

  if (aDeltaMilliseconds == 0) {
    return;
  }

  // Preventing context switch before setting system clock
  sched_yield();
  clock_gettime(CLOCK_REALTIME, &now);
  now.tv_sec += (time_t)(aDeltaMilliseconds / 1000LL);
  now.tv_nsec += (long)((aDeltaMilliseconds % 1000LL) * NsecPerMsec);
  if (now.tv_nsec >= NsecPerSec) {
    now.tv_sec += 1;
    now.tv_nsec -= NsecPerSec;
  }

  if (now.tv_nsec < 0) {
    now.tv_nsec += NsecPerSec;
    now.tv_sec -= 1;
  }

  do {
    fd = open("/dev/alarm", O_RDWR);
  } while (fd == -1 && errno == EINTR);
  ScopedClose autoClose(fd);
  if (fd < 0) {
    HAL_LOG(("Failed to open /dev/alarm: %s", strerror(errno)));
    return;
  }

  if (ioctl(fd, ANDROID_ALARM_SET_RTC, &now) < 0) {
    HAL_LOG(("ANDROID_ALARM_SET_RTC failed: %s", strerror(errno)));
    return;
  }

  hal::NotifySystemClockChange(aDeltaMilliseconds);
}

static int32_t
GetTimezoneOffset()
{
  PRExplodedTime prTime;
  PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &prTime);

  // Daylight saving time (DST) will be taken into account.
  int32_t offset = prTime.tm_params.tp_gmt_offset;
  offset += prTime.tm_params.tp_dst_offset;

  // Returns the timezone offset relative to UTC in minutes.
  return -(offset / 60);
}

void
SetTimezone(const nsCString& aTimezoneSpec)
{
  if (aTimezoneSpec.Equals(GetTimezone())) {
    return;
  }

  int32_t oldTimezoneOffsetMinutes = GetTimezoneOffset();
  property_set("persist.sys.timezone", aTimezoneSpec.get());
  // this function is automatically called by the other time conversion
  // functions that depend on the timezone. To be safe, we call it manually.
  tzset();
  int32_t newTimezoneOffsetMinutes = GetTimezoneOffset();
  hal::NotifySystemTimezoneChange(
    hal::SystemTimezoneChangeInformation(
      oldTimezoneOffsetMinutes, newTimezoneOffsetMinutes));
}

nsCString
GetTimezone()
{
  char timezone[32];
  property_get("persist.sys.timezone", timezone, "");
  return nsCString(timezone);
}

void
EnableSystemClockChangeNotifications()
{
}

void
DisableSystemClockChangeNotifications()
{
}

void
EnableSystemTimezoneChangeNotifications()
{
}

void
DisableSystemTimezoneChangeNotifications()
{
}

// Nothing to do here.  Gonk widgetry always listens for screen
// orientation changes.
void
EnableScreenConfigurationNotifications()
{
}

void
DisableScreenConfigurationNotifications()
{
}

void
GetCurrentScreenConfiguration(hal::ScreenConfiguration* aScreenConfiguration)
{
  *aScreenConfiguration = nsScreenGonk::GetConfiguration();
}

bool
LockScreenOrientation(const dom::ScreenOrientation& aOrientation)
{
  return OrientationObserver::GetInstance()->LockScreenOrientation(aOrientation);
}

void
UnlockScreenOrientation()
{
  OrientationObserver::GetInstance()->UnlockScreenOrientation();
}


static pthread_t sAlarmFireWatcherThread;

// If |sAlarmData| is non-null, it's owned by the watcher thread.
typedef struct AlarmData {

public:
  AlarmData(int aFd) : mFd(aFd), mGeneration(sNextGeneration++), mShuttingDown(false) {}
  ScopedClose mFd;
  int mGeneration;
  bool mShuttingDown;

  static int sNextGeneration;

} AlarmData;

int AlarmData::sNextGeneration = 0;

AlarmData* sAlarmData = NULL;

class AlarmFiredEvent : public nsRunnable {

public:
  AlarmFiredEvent(int aGeneration) : mGeneration(aGeneration) {}

  NS_IMETHOD Run() {
    // Guard against spurious notifications caused by an alarm firing
    // concurrently with it being disabled.
    if (sAlarmData && !sAlarmData->mShuttingDown && mGeneration == sAlarmData->mGeneration) {
      hal::NotifyAlarmFired();
    }

    return NS_OK;
  }

private:
  int mGeneration;
};

// Runs on alarm-watcher thread.
static void
DestroyAlarmData(void* aData)
{
  AlarmData* alarmData = static_cast<AlarmData*>(aData);
  delete alarmData;
}

// Runs on alarm-watcher thread.
void ShutDownAlarm(int aSigno)
{
  if (aSigno == SIGUSR1) {
    sAlarmData->mShuttingDown = true;
  }
  return;
}

static void*
WaitForAlarm(void* aData)
{
  pthread_cleanup_push(DestroyAlarmData, aData);

  AlarmData* alarmData = static_cast<AlarmData*>(aData);

  while (!alarmData->mShuttingDown) {
    int alarmTypeFlags = 0;

    // ALARM_WAIT apparently will block even if an alarm hasn't been
    // programmed, although this behavior doesn't seem to be
    // documented.  We rely on that here to avoid spinning the CPU
    // while awaiting an alarm to be programmed.
    do {
      alarmTypeFlags = ioctl(alarmData->mFd, ANDROID_ALARM_WAIT);
    } while (alarmTypeFlags < 0 && errno == EINTR && !alarmData->mShuttingDown);

    if (!alarmData->mShuttingDown &&
        alarmTypeFlags >= 0 && (alarmTypeFlags & ANDROID_ALARM_RTC_WAKEUP_MASK)) {
      NS_DispatchToMainThread(new AlarmFiredEvent(alarmData->mGeneration));
    }
  }

  pthread_cleanup_pop(1);
  return NULL;
}

bool
EnableAlarm()
{
  MOZ_ASSERT(!sAlarmData);

  int alarmFd = open("/dev/alarm", O_RDWR);
  if (alarmFd < 0) {
    HAL_LOG(("Failed to open alarm device: %s.", strerror(errno)));
    return false;
  }

  nsAutoPtr<AlarmData> alarmData(new AlarmData(alarmFd));

  struct sigaction actions;
  memset(&actions, 0, sizeof(actions));
  sigemptyset(&actions.sa_mask);
  actions.sa_flags = 0;
  actions.sa_handler = ShutDownAlarm;
  if (sigaction(SIGUSR1, &actions, NULL)) {
    HAL_LOG(("Failed to set SIGUSR1 signal for alarm-watcher thread."));
    return false;
  }

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  int status = pthread_create(&sAlarmFireWatcherThread, &attr, WaitForAlarm, alarmData.get());
  if (status) {
    alarmData = NULL;
    HAL_LOG(("Failed to create alarm watcher thread. Status: %d.", status));
    return false;
  }

  pthread_attr_destroy(&attr);

  // The thread owns this now.  We only hold a pointer.
  sAlarmData = alarmData.forget();
  return true;
}

void
DisableAlarm()
{
  MOZ_ASSERT(sAlarmData);

  // NB: this must happen-before the thread cancellation.
  sAlarmData = NULL;

  // The cancel will interrupt the thread and destroy it, freeing the
  // data pointed at by sAlarmData.
  DebugOnly<int> err = pthread_kill(sAlarmFireWatcherThread, SIGUSR1);
  MOZ_ASSERT(!err);
}

bool
SetAlarm(int32_t aSeconds, int32_t aNanoseconds)
{
  if (!sAlarmData) {
    HAL_LOG(("We should have enabled the alarm."));
    return false;
  }

  struct timespec ts;
  ts.tv_sec = aSeconds;
  ts.tv_nsec = aNanoseconds;

  // currently we only support RTC wakeup alarm type
  const int result = ioctl(sAlarmData->mFd, ANDROID_ALARM_SET(ANDROID_ALARM_RTC_WAKEUP), &ts);

  if (result < 0) {
    HAL_LOG(("Unable to set alarm: %s.", strerror(errno)));
    return false;
  }

  return true;
}

static int
OomAdjOfOomScoreAdj(int aOomScoreAdj)
{
  // Convert OOM adjustment from the domain of /proc/<pid>/oom_score_adj
  // to the domain of /proc/<pid>/oom_adj.

  int adj;

  if (aOomScoreAdj < 0) {
    adj = (OOM_DISABLE * aOomScoreAdj) / OOM_SCORE_ADJ_MIN;
  } else {
    adj = (OOM_ADJUST_MAX * aOomScoreAdj) / OOM_SCORE_ADJ_MAX;
  }

  return adj;
}

static void
EnsureKernelLowMemKillerParamsSet()
{
  static bool kernelLowMemKillerParamsSet;
  if (kernelLowMemKillerParamsSet) {
    return;
  }
  kernelLowMemKillerParamsSet = true;

  HAL_LOG(("Setting kernel's low-mem killer parameters."));

  // Set /sys/module/lowmemorykiller/parameters/{adj,minfree,notify_trigger}
  // according to our prefs.  These files let us tune when the kernel kills
  // processes when we're low on memory, and when it notifies us that we're
  // running low on available memory.
  //
  // adj and minfree are both comma-separated lists of integers.  If adj="A,B"
  // and minfree="X,Y", then the kernel will kill processes with oom_adj
  // A or higher once we have fewer than X pages of memory free, and will kill
  // processes with oom_adj B or higher once we have fewer than Y pages of
  // memory free.
  //
  // notify_trigger is a single integer.   If we set notify_trigger=Z, then
  // we'll get notified when there are fewer than Z pages of memory free.  (See
  // GonkMemoryPressureMonitoring.cpp.)

  // Build the adj and minfree strings.
  nsAutoCString adjParams;
  nsAutoCString minfreeParams;

  const char* priorityClasses[] = {
    "master",
    "foreground",
    "background",
    "backgroundHomescreen",
    "backgroundPerceivable"
  };
  for (size_t i = 0; i < NS_ARRAY_LENGTH(priorityClasses); i++) {
    int32_t oomScoreAdj;
    if (!NS_SUCCEEDED(Preferences::GetInt(nsPrintfCString(
          "hal.processPriorityManager.gonk.%sOomScoreAdjust",
          priorityClasses[i]).get(), &oomScoreAdj))) {
      continue;
    }

    int32_t killUnderMB;
    if (!NS_SUCCEEDED(Preferences::GetInt(nsPrintfCString(
          "hal.processPriorityManager.gonk.%sKillUnderMB",
          priorityClasses[i]).get(), &killUnderMB))) {
      continue;
    }

    // adj is in oom_adj units.
    adjParams.AppendPrintf("%d,", OomAdjOfOomScoreAdj(oomScoreAdj));

    // minfree is in pages.
    minfreeParams.AppendPrintf("%d,", killUnderMB * 1024 * 1024 / PAGE_SIZE);
  }

  // Strip off trailing commas.
  adjParams.Cut(adjParams.Length() - 1, 1);
  minfreeParams.Cut(minfreeParams.Length() - 1, 1);
  if (!adjParams.IsEmpty() && !minfreeParams.IsEmpty()) {
    WriteToFile("/sys/module/lowmemorykiller/parameters/adj", adjParams.get());
    WriteToFile("/sys/module/lowmemorykiller/parameters/minfree", minfreeParams.get());
  }

  // Set the low-memory-notification threshold.
  int32_t lowMemNotifyThresholdMB;
  if (NS_SUCCEEDED(Preferences::GetInt(
        "hal.processPriorityManager.gonk.notifyLowMemUnderMB",
        &lowMemNotifyThresholdMB))) {

    // notify_trigger is in pages.
    WriteToFile("/sys/module/lowmemorykiller/parameters/notify_trigger",
      nsPrintfCString("%d", lowMemNotifyThresholdMB * 1024 * 1024 / PAGE_SIZE).get());
  }
}

void
SetProcessPriority(int aPid, ProcessPriority aPriority)
{
  HAL_LOG(("SetProcessPriority(pid=%d, priority=%d)", aPid, aPriority));

  // If this is the first time SetProcessPriority was called, set the kernel's
  // OOM parameters according to our prefs.
  //
  // We could/should do this on startup instead of waiting for the first
  // SetProcessPriorityCall.  But in practice, the master process needs to set
  // its priority early in the game, so we can reasonably rely on
  // SetProcessPriority being called early in startup.
  EnsureKernelLowMemKillerParamsSet();

  const char* priorityStr = NULL;
  switch (aPriority) {
  case PROCESS_PRIORITY_BACKGROUND:
    priorityStr = "background";
    break;
  case PROCESS_PRIORITY_BACKGROUND_HOMESCREEN:
    priorityStr = "backgroundHomescreen";
    break;
  case PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE:
    priorityStr = "backgroundPerceivable";
    break;
  case PROCESS_PRIORITY_FOREGROUND:
    priorityStr = "foreground";
    break;
  case PROCESS_PRIORITY_MASTER:
    priorityStr = "master";
    break;
  default:
    MOZ_NOT_REACHED();
  }

  // Notice that you can disable oom_adj and renice by deleting the prefs
  // hal.processPriorityManager{foreground,background,master}{OomAdjust,Nice}.

  int32_t oomScoreAdj = 0;
  nsresult rv = Preferences::GetInt(nsPrintfCString(
    "hal.processPriorityManager.gonk.%sOomScoreAdjust",
    priorityStr).get(), &oomScoreAdj);

  if (NS_SUCCEEDED(rv)) {

    int clampedOomScoreAdj = clamped<int>(oomScoreAdj, OOM_SCORE_ADJ_MIN,
                                                       OOM_SCORE_ADJ_MAX);
    if(clampedOomScoreAdj != oomScoreAdj) {
      HAL_LOG(("Clamping OOM adjustment for pid %d to %d",
               aPid, clampedOomScoreAdj));
    } else {
      HAL_LOG(("Setting OOM adjustment for pid %d to %d",
               aPid, clampedOomScoreAdj));
    }

    // We try the newer interface first, and fall back to the older interface
    // on failure.

    if (!WriteToFile(nsPrintfCString("/proc/%d/oom_score_adj", aPid).get(),
                     nsPrintfCString("%d", clampedOomScoreAdj).get()))
    {
      int oomAdj = OomAdjOfOomScoreAdj(clampedOomScoreAdj);

      WriteToFile(nsPrintfCString("/proc/%d/oom_adj", aPid).get(),
                  nsPrintfCString("%d", oomAdj).get());
    }
  }

  int32_t nice = 0;
  rv = Preferences::GetInt(nsPrintfCString(
    "hal.processPriorityManager.gonk.%sNice", priorityStr).get(), &nice);
  if (NS_SUCCEEDED(rv)) {
    HAL_LOG(("Setting nice for pid %d to %d", aPid, nice));

    int success = setpriority(PRIO_PROCESS, aPid, nice);
    if (success != 0) {
      HAL_LOG(("Failed to set nice for pid %d to %d", aPid, nice));
    }
  }
}

void
FactoryReset()
{
  nsCOMPtr<nsIRecoveryService> recoveryService =
    do_GetService("@mozilla.org/recovery-service;1");
  if (!recoveryService) {
    NS_WARNING("Could not get recovery service!");
    return;
  }

  recoveryService->FactoryReset();
}

} // hal_impl
} // mozilla
