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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/android_alarm.h>
#include <math.h>
#include <regex.h>
#include <sched.h>
#include <stdio.h>
#include <sys/klog.h>
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
#include "libdisplay/GonkDisplay.h"
#include "utils/threads.h"

#include "base/message_loop.h"

#include "Hal.h"
#include "HalImpl.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/battery/Constants.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"
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
#include "nsIWritablePropertyBag2.h"
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
using namespace mozilla::dom;

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

  NS_DECL_THREADSAFE_ISUPPORTS
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

NS_IMPL_ISUPPORTS(VibratorRunnable, nsIRunnable, nsIObserver);

bool VibratorRunnable::sShuttingDown = false;

static StaticRefPtr<VibratorRunnable> sVibratorRunnable;

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
  sVibratorRunnable = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
VibratorRunnable::Observe(nsISupports *subject, const char *topic,
                          const char16_t *data)
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

    {
      // bug 975667
      // Gecko gonk hal is required to emit battery charging/level notification via nsIObserverService.
      // This is useful for XPCOM components that are not statically linked to Gecko and cannot call
      // hal::EnableBatteryNotifications
      nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
      nsCOMPtr<nsIWritablePropertyBag2> propbag =
        do_CreateInstance("@mozilla.org/hash-property-bag;1");
      if (obsService && propbag) {
        propbag->SetPropertyAsBool(NS_LITERAL_STRING("charging"),
                                   info.charging());
        propbag->SetPropertyAsDouble(NS_LITERAL_STRING("level"),
                                   info.level());

        obsService->NotifyObservers(propbag, "gonkhal-battery-notifier", nullptr);
      }
    }

    return NS_OK;
  }
};

} // anonymous namespace

class BatteryObserver : public IUeventObserver
{
public:
  NS_INLINE_DECL_REFCOUNTING(BatteryObserver)

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
  sBatteryObserver = nullptr;
}

void
DisableBatteryNotifications()
{
  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(UnregisterBatteryObserverIOThread));
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
    HAL_LOG(("charge level contains unknown value: %d", *aCharge));
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

  success = ReadSysFile("/sys/class/power_supply/battery/status",
                        chargingSrcString, sizeof(chargingSrcString));
  if (success) {
    *aCharging = strcmp(chargingSrcString, "Charging") == 0 ||
                 strcmp(chargingSrcString, "Full") == 0;
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

  if (!aBatteryInfo->charging() || (aBatteryInfo->level() < 1.0)) {
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

// Some CPU wake locks may be acquired internally in HAL. We use a counter to
// keep track of these needs. Note we have to hold |sInternalLockCpuMonitor|
// when reading or writing this variable to ensure thread-safe.
int32_t sInternalLockCpuCount = 0;

} // anonymous namespace

bool
GetScreenEnabled()
{
  return sScreenEnabled;
}

void
SetScreenEnabled(bool aEnabled)
{
  GetGonkDisplay()->SetEnabled(aEnabled);
  sScreenEnabled = aEnabled;
}

bool
GetKeyLightEnabled()
{
  hal::LightConfiguration config;
  hal_impl::GetLight(hal::eHalLightID_Buttons, &config);
  return (config.color() != 0x00000000);
}

void
SetKeyLightEnabled(bool aEnabled)
{
  hal::LightConfiguration config;
  config.mode() = hal::eHalLightMode_User;
  config.flash() = hal::eHalLightFlash_None;
  config.flashOnMS() = config.flashOffMS() = 0;
  config.color() = 0x00000000;

  if (aEnabled) {
    // Convert the value in [0, 1] to an int between 0 and 255 and then convert
    // it to a color. Note that the high byte is FF, corresponding to the alpha
    // channel.
    double brightness = GetScreenBrightness();
    uint32_t val = static_cast<int>(round(brightness * 255.0));
    uint32_t color = (0xff<<24) + (val<<16) + (val<<8) + val;

    config.color() = color;
  }

  hal_impl::SetLight(hal::eHalLightID_Buttons, config);
  hal_impl::SetLight(hal::eHalLightID_Keyboard, config);
}

double
GetScreenBrightness()
{
  hal::LightConfiguration config;
  hal::LightType light = hal::eHalLightID_Backlight;

  hal_impl::GetLight(light, &config);
  // backlight is brightness only, so using one of the RGB elements as value.
  int brightness = config.color() & 0xFF;
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
  uint32_t val = static_cast<int>(round(brightness * 255.0));
  uint32_t color = (0xff<<24) + (val<<16) + (val<<8) + val;

  hal::LightConfiguration config;
  config.mode() = hal::eHalLightMode_User;
  config.flash() = hal::eHalLightFlash_None;
  config.flashOnMS() = config.flashOffMS() = 0;
  config.color() = color;
  hal_impl::SetLight(hal::eHalLightID_Backlight, config);
  if (GetKeyLightEnabled()) {
    hal_impl::SetLight(hal::eHalLightID_Buttons, config);
    hal_impl::SetLight(hal::eHalLightID_Keyboard, config);
  }
}

static Monitor* sInternalLockCpuMonitor = nullptr;

static void
UpdateCpuSleepState()
{
  sInternalLockCpuMonitor->AssertCurrentThreadOwns();
  bool allowed = sCpuSleepAllowed && !sInternalLockCpuCount;
  WriteToFile(allowed ? wakeUnlockFilename : wakeLockFilename, "gecko");
}

static void
InternalLockCpu() {
  MonitorAutoLock monitor(*sInternalLockCpuMonitor);
  ++sInternalLockCpuCount;
  UpdateCpuSleepState();
}

static void
InternalUnlockCpu() {
  MonitorAutoLock monitor(*sInternalLockCpuMonitor);
  --sInternalLockCpuCount;
  UpdateCpuSleepState();
}

bool
GetCpuSleepAllowed()
{
  return sCpuSleepAllowed;
}

void
SetCpuSleepAllowed(bool aAllowed)
{
  MonitorAutoLock monitor(*sInternalLockCpuMonitor);
  sCpuSleepAllowed = aAllowed;
  UpdateCpuSleepState();
}

static light_device_t* sLights[hal::eHalLightID_Count];	// will be initialized to nullptr

light_device_t* GetDevice(hw_module_t* module, char const* name)
{
  int err;
  hw_device_t* device;
  err = module->methods->open(module, name, &device);
  if (err == 0) {
    return (light_device_t*)device;
  } else {
    return nullptr;
  }
}

void
InitLights()
{
  // assume that if backlight is nullptr, nothing has been set yet
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

  if (light < 0 || light >= hal::eHalLightID_Count ||
      sLights[light] == nullptr) {
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

  if (light < 0 || light >= hal::eHalLightID_Count ||
      sLights[light] == nullptr) {
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
  }

  hal::NotifySystemClockChange(aDeltaMilliseconds);
}

int32_t
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

static int32_t sKernelTimezoneOffset = 0;

static void
UpdateKernelTimezone(int32_t timezoneOffset)
{
  if (sKernelTimezoneOffset == timezoneOffset) {
    return;
  }

  // Tell the kernel about the new time zone as well, so that FAT filesystems
  // will get local timestamps rather than UTC timestamps.
  //
  // We assume that /init.rc has a sysclktz entry so that settimeofday has
  // already been called once before we call it (there is a side-effect in
  // the kernel the very first time settimeofday is called where it does some
  // special processing if you only set the timezone).
  struct timezone tz;
  memset(&tz, 0, sizeof(tz));
  tz.tz_minuteswest = timezoneOffset;
  settimeofday(nullptr, &tz);
  sKernelTimezoneOffset = timezoneOffset;
}

void
SetTimezone(const nsCString& aTimezoneSpec)
{
  if (aTimezoneSpec.Equals(GetTimezone())) {
    // Even though the timezone hasn't changed, we still need to tell the
    // kernel what the current timezone is. The timezone is persisted in
    // a property and doesn't change across reboots, but the kernel still
    // needs to be updated on every boot.
    UpdateKernelTimezone(GetTimezoneOffset());
    return;
  }

  int32_t oldTimezoneOffsetMinutes = GetTimezoneOffset();
  property_set("persist.sys.timezone", aTimezoneSpec.get());
  // This function is automatically called by the other time conversion
  // functions that depend on the timezone. To be safe, we call it manually.
  tzset();
  int32_t newTimezoneOffsetMinutes = GetTimezoneOffset();
  UpdateKernelTimezone(newTimezoneOffsetMinutes);
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

// This thread will wait for the alarm firing by a blocking IO.
static pthread_t sAlarmFireWatcherThread;

// If |sAlarmData| is non-null, it's owned by the alarm-watcher thread.
struct AlarmData {
public:
  AlarmData(int aFd) : mFd(aFd),
                       mGeneration(sNextGeneration++),
                       mShuttingDown(false) {}
  ScopedClose mFd;
  int mGeneration;
  bool mShuttingDown;

  static int sNextGeneration;

};

int AlarmData::sNextGeneration = 0;

AlarmData* sAlarmData = nullptr;

class AlarmFiredEvent : public nsRunnable {
public:
  AlarmFiredEvent(int aGeneration) : mGeneration(aGeneration) {}

  NS_IMETHOD Run() {
    // Guard against spurious notifications caused by an alarm firing
    // concurrently with it being disabled.
    if (sAlarmData && !sAlarmData->mShuttingDown &&
        mGeneration == sAlarmData->mGeneration) {
      hal::NotifyAlarmFired();
    }
    // The fired alarm event has been delivered to the observer (if needed);
    // we can now release a CPU wake lock.
    InternalUnlockCpu();
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
  if (aSigno == SIGUSR1 && sAlarmData) {
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
    } while (alarmTypeFlags < 0 && errno == EINTR &&
             !alarmData->mShuttingDown);

    if (!alarmData->mShuttingDown && alarmTypeFlags >= 0 &&
        (alarmTypeFlags & ANDROID_ALARM_RTC_WAKEUP_MASK)) {
      // To make sure the observer can get the alarm firing notification
      // *on time* (the system won't sleep during the process in any way),
      // we need to acquire a CPU wake lock before firing the alarm event.
      InternalLockCpu();
      nsRefPtr<AlarmFiredEvent> event =
        new AlarmFiredEvent(alarmData->mGeneration);
      NS_DispatchToMainThread(event);
    }
  }

  pthread_cleanup_pop(1);
  return nullptr;
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
  if (sigaction(SIGUSR1, &actions, nullptr)) {
    HAL_LOG(("Failed to set SIGUSR1 signal for alarm-watcher thread."));
    return false;
  }

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  // Initialize the monitor for internally locking CPU to ensure thread-safe
  // before running the alarm-watcher thread.
  sInternalLockCpuMonitor = new Monitor("sInternalLockCpuMonitor");
  int status = pthread_create(&sAlarmFireWatcherThread, &attr, WaitForAlarm,
                              alarmData.get());
  if (status) {
    alarmData = nullptr;
    delete sInternalLockCpuMonitor;
    HAL_LOG(("Failed to create alarm-watcher thread. Status: %d.", status));
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
  sAlarmData = nullptr;

  // The cancel will interrupt the thread and destroy it, freeing the
  // data pointed at by sAlarmData.
  DebugOnly<int> err = pthread_kill(sAlarmFireWatcherThread, SIGUSR1);
  MOZ_ASSERT(!err);

  delete sInternalLockCpuMonitor;
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

  // Currently we only support RTC wakeup alarm type.
  const int result = ioctl(sAlarmData->mFd,
                           ANDROID_ALARM_SET(ANDROID_ALARM_RTC_WAKEUP), &ts);

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
RoundOomScoreAdjUpWithBackroundLRU(int& aOomScoreAdj, uint32_t aBackgroundLRU)
{
  // We want to add minimum value to round OomScoreAdj up according to
  // the steps by aBackgroundLRU.
  aOomScoreAdj +=
    ceil(((float)OOM_SCORE_ADJ_MAX / OOM_ADJUST_MAX) * aBackgroundLRU);
}

#define OOM_LOG(level, args...) __android_log_print(level, "OomLogger", ##args)
class OomVictimLogger MOZ_FINAL
  : public nsIObserver
{
public:
  OomVictimLogger()
    : mLastLineChecked(-1.0),
      mRegexes(nullptr)
  {
    // Enable timestamps in kernel's printk
    WriteToFile("/sys/module/printk/parameters/time", "Y");
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
private:
  double mLastLineChecked;
  ScopedFreePtr<regex_t> mRegexes;
};
NS_IMPL_ISUPPORTS(OomVictimLogger, nsIObserver);

NS_IMETHODIMP
OomVictimLogger::Observe(
  nsISupports* aSubject,
  const char* aTopic,
  const char16_t* aData)
{
  nsDependentCString event_type(aTopic);
  if (!event_type.EqualsLiteral("ipc:content-shutdown")) {
    return NS_OK;
  }

  // OOM message finding regexes
  const char* const regexes_raw[] = {
    ".*select.*to kill.*",
    ".*send sigkill to.*",
    ".*lowmem_shrink.*",
    ".*[Oo]ut of [Mm]emory.*",
    ".*[Kk]ill [Pp]rocess.*",
    ".*[Kk]illed [Pp]rocess.*",
    ".*oom-killer.*",
    // The regexes below are for the output of dump_task from oom_kill.c
    // 1st - title 2nd - body lines (8 ints and a string)
    // oom_adj and oom_score_adj can be negative
    "\\[ pid \\]   uid  tgid total_vm      rss cpu oom_adj oom_score_adj name",
    "\\[.*[0-9][0-9]*\\][ ]*[0-9][0-9]*[ ]*[0-9][0-9]*[ ]*[0-9][0-9]*[ ]*[0-9][0-9]*[ ]*[0-9][0-9]*[ ]*.[0-9][0-9]*[ ]*.[0-9][0-9]*.*"
  };
  const size_t regex_count = ArrayLength(regexes_raw);

  // Compile our regex just in time
  if (!mRegexes) {
    mRegexes = static_cast<regex_t*>(malloc(sizeof(regex_t) * regex_count));
    for (size_t i = 0; i < regex_count; i++) {
      int compilation_err = regcomp(&(mRegexes[i]), regexes_raw[i], REG_NOSUB);
      if (compilation_err) {
        OOM_LOG(ANDROID_LOG_ERROR, "Cannot compile regex \"%s\"\n", regexes_raw[i]);
        return NS_OK;
      }
    }
  }

#ifndef KLOG_SIZE_BUFFER
  // Upstream bionic in commit
  // e249b059637b49a285ed9f58a2a18bfd054e5d95
  // deprecated the old klog defs.
  // Our current bionic does not hit this
  // change yet so handle the future change.
  #define KLOG_SIZE_BUFFER KLOG_WRITE
#else
  // Once the change hits our bionic this ifndef
  // can be removed.
  #warning "Please remove KLOG_UNREAD_SIZE compatability def"
#endif
  // Retreive kernel log
  int msg_buf_size = klogctl(KLOG_SIZE_BUFFER, NULL, 0);
  ScopedFreePtr<char> msg_buf(static_cast<char *>(malloc(msg_buf_size + 1)));
  int read_size = klogctl(KLOG_READ_ALL, msg_buf.rwget(), msg_buf_size);

  // Turn buffer into cstring
  read_size = read_size > msg_buf_size ? msg_buf_size : read_size;
  msg_buf.rwget()[read_size] = '\0';

  // Foreach line
  char* line_end;
  char* line_begin = msg_buf.rwget();
  for (; (line_end = strchr(line_begin, '\n')); line_begin = line_end + 1) {
    // make line into cstring
    *line_end = '\0';

    // Note: Kernel messages look like:
    // <5>[63648.286409] sd 35:0:0:0: Attached scsi generic sg1 type 0
    // 5 is the loging level
    // [*] is the time timestamp, seconds since boot
    // last comes the logged message

    // Since the logging level can be a string we must
    // skip it since scanf lacks wildcard matching
    char*  timestamp_begin = strchr(line_begin, '[');
    char   after_float;
    double lineTimestamp = -1;
    bool   lineTimestampFound = false;
    if (timestamp_begin &&
         // Note: scanf treats a ' ' as [ ]*
         // Note: scanf treats [ %lf] as [ %lf thus we must check
         // for the closing bracket outselves.
         2 == sscanf(timestamp_begin, "[ %lf%c", &lineTimestamp, &after_float) &&
         after_float == ']') {
      if (lineTimestamp <= mLastLineChecked) {
        continue;
      }

      lineTimestampFound = true;
      mLastLineChecked = lineTimestamp;
    }
      

    // Log interesting lines
    for (size_t i = 0; i < regex_count; i++) {
      int matching = !regexec(&(mRegexes[i]), line_begin, 0, NULL, 0);
      if (matching) {
        // Log content of kernel message. We try to skip the ], but if for
        // some reason (most likely due to buffer overflow/wraparound), we
        // can't find the ] then we just log the entire line.
        char* endOfTimestamp = strchr(line_begin, ']');
        if (endOfTimestamp && endOfTimestamp[1] == ' ') {
          // skip the ] and the space that follows it
          line_begin = endOfTimestamp + 2;
        }
        if (!lineTimestampFound) {
          OOM_LOG(ANDROID_LOG_WARN, "following kill message may be a duplicate");
        }
        OOM_LOG(ANDROID_LOG_ERROR, "[Kill]: %s\n", line_begin);
        break;
      }
    }
  }

  return NS_OK;
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

  int32_t lowerBoundOfNextOomScoreAdj = OOM_SCORE_ADJ_MIN - 1;
  int32_t lowerBoundOfNextKillUnderKB = 0;
  int32_t countOfLowmemorykillerParametersSets = 0;

  for (int i = NUM_PROCESS_PRIORITY - 1; i >= 0; i--) {
    // The system doesn't function correctly if we're missing these prefs, so
    // crash loudly.

    ProcessPriority priority = static_cast<ProcessPriority>(i);

    int32_t oomScoreAdj;
    if (!NS_SUCCEEDED(Preferences::GetInt(
          nsPrintfCString("hal.processPriorityManager.gonk.%s.OomScoreAdjust",
                          ProcessPriorityToString(priority)).get(),
          &oomScoreAdj))) {
      MOZ_CRASH();
    }

    int32_t killUnderKB;
    if (!NS_SUCCEEDED(Preferences::GetInt(
          nsPrintfCString("hal.processPriorityManager.gonk.%s.KillUnderKB",
                          ProcessPriorityToString(priority)).get(),
          &killUnderKB))) {
      // ProcessPriority values like PROCESS_PRIORITY_FOREGROUND_KEYBOARD,
      // which has only OomScoreAdjust but lacks KillUnderMB value, will not
      // create new LMK parameters.
      continue;
    }

    // The LMK in kernel silently malfunctions if we assign the parameters
    // in non-increasing order, so we add this assertion here. See bug 887192.
    MOZ_ASSERT(oomScoreAdj > lowerBoundOfNextOomScoreAdj);
    MOZ_ASSERT(killUnderKB > lowerBoundOfNextKillUnderKB);

    // The LMK in kernel only accept 6 sets of LMK parameters. See bug 914728.
    MOZ_ASSERT(countOfLowmemorykillerParametersSets < 6);

    // adj is in oom_adj units.
    adjParams.AppendPrintf("%d,", OomAdjOfOomScoreAdj(oomScoreAdj));

    // minfree is in pages.
    minfreeParams.AppendPrintf("%d,", killUnderKB * 1024 / PAGE_SIZE);

    lowerBoundOfNextOomScoreAdj = oomScoreAdj;
    lowerBoundOfNextKillUnderKB = killUnderKB;
    countOfLowmemorykillerParametersSets++;
  }

  // Strip off trailing commas.
  adjParams.Cut(adjParams.Length() - 1, 1);
  minfreeParams.Cut(minfreeParams.Length() - 1, 1);
  if (!adjParams.IsEmpty() && !minfreeParams.IsEmpty()) {
    WriteToFile("/sys/module/lowmemorykiller/parameters/adj", adjParams.get());
    WriteToFile("/sys/module/lowmemorykiller/parameters/minfree", minfreeParams.get());
  }

  // Set the low-memory-notification threshold.
  int32_t lowMemNotifyThresholdKB;
  if (NS_SUCCEEDED(Preferences::GetInt(
        "hal.processPriorityManager.gonk.notifyLowMemUnderKB",
        &lowMemNotifyThresholdKB))) {

    // notify_trigger is in pages.
    WriteToFile("/sys/module/lowmemorykiller/parameters/notify_trigger",
      nsPrintfCString("%d", lowMemNotifyThresholdKB * 1024 / PAGE_SIZE).get());
  }

  // Ensure OOM events appear in logcat
  nsRefPtr<OomVictimLogger> oomLogger = new OomVictimLogger();
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->AddObserver(oomLogger, "ipc:content-shutdown", false);
  }
}

static void
SetNiceForPid(int aPid, int aNice)
{
  errno = 0;
  int origProcPriority = getpriority(PRIO_PROCESS, aPid);
  if (errno) {
    LOG("Unable to get nice for pid=%d; error %d.  SetNiceForPid bailing.",
        aPid, errno);
    return;
  }

  int rv = setpriority(PRIO_PROCESS, aPid, aNice);
  if (rv) {
    LOG("Unable to set nice for pid=%d; error %d.  SetNiceForPid bailing.",
        aPid, errno);
    return;
  }

  // On Linux, setpriority(aPid) modifies the priority only of the main
  // thread of that process.  We have to modify the priorities of all of the
  // process's threads as well, so iterate over all the threads and increase
  // each of their priorites by aNice - origProcPriority (and also ensure that
  // none of the tasks has a lower priority than the main thread).
  //
  // This is horribly racy.

  DIR* tasksDir = opendir(nsPrintfCString("/proc/%d/task/", aPid).get());
  if (!tasksDir) {
    LOG("Unable to open /proc/%d/task.  SetNiceForPid bailing.", aPid);
    return;
  }

  // Be careful not to leak tasksDir; after this point, we must call closedir().

  while (struct dirent* de = readdir(tasksDir)) {
    char* endptr = nullptr;
    long tidlong = strtol(de->d_name, &endptr, /* base */ 10);
    if (*endptr || tidlong < 0 || tidlong > INT32_MAX || tidlong == aPid) {
      // if dp->d_name was not an integer, was negative (?!) or too large, or
      // was the same as aPid, we're not interested.
      //
      // (The |tidlong == aPid| check is very important; without it, we'll
      // renice aPid twice, and the second renice will be relative to the
      // priority set by the first renice.)
      continue;
    }

    int tid = static_cast<int>(tidlong);

    // Do not set the priority of threads running with a real-time policy
    // as part of the bulk process adjustment.  These threads need to run
    // at their specified priority in order to meet timing guarantees.
    int schedPolicy = sched_getscheduler(tid);
    if (schedPolicy == SCHED_FIFO || schedPolicy == SCHED_RR) {
      continue;
    }

    errno = 0;
    // Get and set the task's new priority.
    int origtaskpriority = getpriority(PRIO_PROCESS, tid);
    if (errno) {
      LOG("Unable to get nice for tid=%d (pid=%d); error %d.  This isn't "
          "necessarily a problem; it could be a benign race condition.",
          tid, aPid, errno);
      continue;
    }

    int newtaskpriority =
      std::max(origtaskpriority - origProcPriority + aNice, aNice);

    // Do not reduce priority of threads already running at priorities greater
    // than normal.  These threads are likely special service threads that need
    // elevated priorities to process audio, display composition, etc.
    if (newtaskpriority > origtaskpriority &&
        origtaskpriority < ANDROID_PRIORITY_NORMAL) {
      continue;
    }

    rv = setpriority(PRIO_PROCESS, tid, newtaskpriority);

    if (rv) {
      LOG("Unable to set nice for tid=%d (pid=%d); error %d.  This isn't "
          "necessarily a problem; it could be a benign race condition.",
          tid, aPid, errno);
      continue;
    }
  }

  LOG("Changed nice for pid %d from %d to %d.",
      aPid, origProcPriority, aNice);

  closedir(tasksDir);
}

void
SetProcessPriority(int aPid,
                   ProcessPriority aPriority,
                   ProcessCPUPriority aCPUPriority,
                   uint32_t aBackgroundLRU)
{
  HAL_LOG(("SetProcessPriority(pid=%d, priority=%d, cpuPriority=%d, LRU=%u)",
           aPid, aPriority, aCPUPriority, aBackgroundLRU));

  // If this is the first time SetProcessPriority was called, set the kernel's
  // OOM parameters according to our prefs.
  //
  // We could/should do this on startup instead of waiting for the first
  // SetProcessPriorityCall.  But in practice, the master process needs to set
  // its priority early in the game, so we can reasonably rely on
  // SetProcessPriority being called early in startup.
  EnsureKernelLowMemKillerParamsSet();

  int32_t oomScoreAdj = 0;
  nsresult rv = Preferences::GetInt(nsPrintfCString(
    "hal.processPriorityManager.gonk.%s.OomScoreAdjust",
    ProcessPriorityToString(aPriority)).get(), &oomScoreAdj);

  RoundOomScoreAdjUpWithBackroundLRU(oomScoreAdj, aBackgroundLRU);

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
  } else {
    LOG("Unable to read oom_score_adj pref for priority %s; "
        "are the prefs messed up?",
        ProcessPriorityToString(aPriority));
    MOZ_ASSERT(false);
  }

  int32_t nice = 0;

  if (aCPUPriority == PROCESS_CPU_PRIORITY_NORMAL) {
    rv = Preferences::GetInt(
      nsPrintfCString("hal.processPriorityManager.gonk.%s.Nice",
                      ProcessPriorityToString(aPriority)).get(),
      &nice);
  } else if (aCPUPriority == PROCESS_CPU_PRIORITY_LOW) {
    rv = Preferences::GetInt("hal.processPriorityManager.gonk.LowCPUNice",
                             &nice);
  } else {
    LOG("Unable to read niceness pref for priority %s; "
        "are the prefs messed up?",
        ProcessPriorityToString(aPriority));
    MOZ_ASSERT(false);
    rv = NS_ERROR_FAILURE;
  }

  if (NS_SUCCEEDED(rv)) {
    LOG("Setting nice for pid %d to %d", aPid, nice);
    SetNiceForPid(aPid, nice);
  }
}

static bool
IsValidRealTimePriority(int aValue, int aSchedulePolicy)
{
  return (aValue >= sched_get_priority_min(aSchedulePolicy)) &&
         (aValue <= sched_get_priority_max(aSchedulePolicy));
}

static void
SetThreadNiceValue(pid_t aTid, ThreadPriority aThreadPriority, int aValue)
{
  MOZ_ASSERT(aThreadPriority < NUM_THREAD_PRIORITY);
  MOZ_ASSERT(aThreadPriority >= 0);

  LOG("Setting thread %d to priority level %s; nice level %d",
      aTid, ThreadPriorityToString(aThreadPriority), aValue);
  int rv = setpriority(PRIO_PROCESS, aTid, aValue);

  if (rv) {
    LOG("Failed to set thread %d to priority level %s; error %s",
        aTid, ThreadPriorityToString(aThreadPriority), strerror(errno));
  }
}

static void
SetRealTimeThreadPriority(pid_t aTid,
                          ThreadPriority aThreadPriority,
                          int aValue)
{
  int policy = SCHED_FIFO;

  MOZ_ASSERT(aThreadPriority < NUM_THREAD_PRIORITY);
  MOZ_ASSERT(aThreadPriority >= 0);
  MOZ_ASSERT(IsValidRealTimePriority(aValue, policy), "Invalid real time priority");

  // Setting real time priorities requires using sched_setscheduler
  LOG("Setting thread %d to priority level %s; Real Time priority %d, Schedule FIFO",
      aTid, ThreadPriorityToString(aThreadPriority), aValue);
  sched_param schedParam;
  schedParam.sched_priority = aValue;
  int rv = sched_setscheduler(aTid, policy, &schedParam);

  if (rv) {
    LOG("Failed to set thread %d to real time priority level %s; error %s",
        aTid, ThreadPriorityToString(aThreadPriority), strerror(errno));
  }
}

static void
SetThreadPriority(pid_t aTid, hal::ThreadPriority aThreadPriority)
{
  // See bug 999115, we can only read preferences on the main thread otherwise
  // we create a race condition in HAL
  MOZ_ASSERT(NS_IsMainThread(), "Can only set thread priorities on main thread");
  MOZ_ASSERT(aThreadPriority >= 0);

  const char* threadPriorityStr;
  switch (aThreadPriority) {
    case THREAD_PRIORITY_COMPOSITOR:
      threadPriorityStr = ThreadPriorityToString(aThreadPriority);
      break;
    default:
      LOG("Unrecognized thread priority %d; Doing nothing", aThreadPriority);
      return;
  }

  int realTimePriority = Preferences::GetInt(
    nsPrintfCString("hal.gonk.%s.rt_priority", threadPriorityStr).get());

  if (IsValidRealTimePriority(realTimePriority, SCHED_FIFO)) {
    SetRealTimeThreadPriority(aTid, aThreadPriority, realTimePriority);
    return;
  }

  int niceValue = Preferences::GetInt(
    nsPrintfCString("hal.gonk.%s.nice", threadPriorityStr).get());

  SetThreadNiceValue(aTid, aThreadPriority, niceValue);
}

namespace {

/**
 * This class sets the priority of threads given the kernel thread's id and a
 * value taken from hal::ThreadPriority.
 *
 * This runnable must always be dispatched to the main thread otherwise it will fail.
 * We have to run this from the main thread since preferences can only be read on
 * main thread.
 */
class SetThreadPriorityRunnable : public nsRunnable
{
public:
  SetThreadPriorityRunnable(pid_t aThreadId, hal::ThreadPriority aThreadPriority)
    : mThreadId(aThreadId)
    , mThreadPriority(aThreadPriority)
  { }

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Can only set thread priorities on main thread");
    hal_impl::SetThreadPriority(mThreadId, mThreadPriority);
    return NS_OK;
  }

private:
  pid_t mThreadId;
  hal::ThreadPriority mThreadPriority;
};

} // anonymous namespace

void
SetCurrentThreadPriority(ThreadPriority aThreadPriority)
{
  switch (aThreadPriority) {
    case THREAD_PRIORITY_COMPOSITOR: {
      pid_t threadId = gettid();
      nsCOMPtr<nsIRunnable> runnable =
        new SetThreadPriorityRunnable(threadId, aThreadPriority);
      NS_DispatchToMainThread(runnable);
      break;
    }
    default:
      LOG("Unrecognized thread priority %d; Doing nothing", aThreadPriority);
      return;
  }
}

void
FactoryReset(FactoryResetReason& aReason)
{
  nsCOMPtr<nsIRecoveryService> recoveryService =
    do_GetService("@mozilla.org/recovery-service;1");
  if (!recoveryService) {
    NS_WARNING("Could not get recovery service!");
    return;
  }

  if (aReason == FactoryResetReason::Wipe) {
    recoveryService->FactoryReset("wipe");
  } else {
    recoveryService->FactoryReset("normal");
  }
}

} // hal_impl
} // mozilla
