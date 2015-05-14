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
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

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
#include "HalLog.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/battery/Constants.h"
#include "mozilla/DebugOnly.h"
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

/**
 * These are defined by libhardware, specifically, hardware/libhardware/include/hardware/lights.h
 * in the gonk subsystem.
 * If these change and are exposed to JS, make sure nsIHal.idl is updated as well.
 */
enum LightType {
  eHalLightID_Backlight     = 0,
  eHalLightID_Keyboard      = 1,
  eHalLightID_Buttons       = 2,
  eHalLightID_Battery       = 3,
  eHalLightID_Notifications = 4,
  eHalLightID_Attention     = 5,
  eHalLightID_Bluetooth     = 6,
  eHalLightID_Wifi          = 7,
  eHalLightID_Count  // This should stay at the end
};
enum LightMode {
  eHalLightMode_User   = 0,  // brightness is managed by user setting
  eHalLightMode_Sensor = 1,  // brightness is managed by a light sensor
  eHalLightMode_Count
};
enum FlashMode {
  eHalLightFlash_None     = 0,
  eHalLightFlash_Timed    = 1,  // timed flashing.  Use flashOnMS and flashOffMS for timing
  eHalLightFlash_Hardware = 2,  // hardware assisted flashing
  eHalLightFlash_Count
};

struct LightConfiguration {
  LightType light;
  LightMode mode;
  FlashMode flash;
  uint32_t flashOnMS;
  uint32_t flashOffMS;
  uint32_t color;
};

static light_device_t* sLights[eHalLightID_Count]; // will be initialized to nullptr

static light_device_t*
GetDevice(hw_module_t* module, char const* name)
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

static void
InitLights()
{
  // assume that if backlight is nullptr, nothing has been set yet
  // if this is not true, the initialization will occur everytime a light is read or set!
  if (!sLights[eHalLightID_Backlight]) {
    int err;
    hw_module_t* module;

    err = hw_get_module(LIGHTS_HARDWARE_MODULE_ID, (hw_module_t const**)&module);
    if (err == 0) {
      sLights[eHalLightID_Backlight]
             = GetDevice(module, LIGHT_ID_BACKLIGHT);
      sLights[eHalLightID_Keyboard]
             = GetDevice(module, LIGHT_ID_KEYBOARD);
      sLights[eHalLightID_Buttons]
             = GetDevice(module, LIGHT_ID_BUTTONS);
      sLights[eHalLightID_Battery]
             = GetDevice(module, LIGHT_ID_BATTERY);
      sLights[eHalLightID_Notifications]
             = GetDevice(module, LIGHT_ID_NOTIFICATIONS);
      sLights[eHalLightID_Attention]
             = GetDevice(module, LIGHT_ID_ATTENTION);
      sLights[eHalLightID_Bluetooth]
             = GetDevice(module, LIGHT_ID_BLUETOOTH);
      sLights[eHalLightID_Wifi]
             = GetDevice(module, LIGHT_ID_WIFI);
        }
    }
}

/**
 * The state last set for the lights until liblights supports
 * getting the light state.
 */
static light_state_t sStoredLightState[eHalLightID_Count];

/**
* Set the value of a light to a particular color, with a specific flash pattern.
* light specifices which light.  See Hal.idl for the list of constants
* mode specifies user set or based on ambient light sensor
* flash specifies whether or how to flash the light
* flashOnMS and flashOffMS specify the pattern for XXX flash mode
* color specifies the color.  If the light doesn't support color, the given color is
* transformed into a brightness, or just an on/off if that is all the light is capable of.
* returns true if successful and false if failed.
*/
static bool
SetLight(LightType light, const LightConfiguration& aConfig)
{
  light_state_t state;

  InitLights();

  if (light < 0 || light >= eHalLightID_Count ||
      sLights[light] == nullptr) {
    return false;
  }

  memset(&state, 0, sizeof(light_state_t));
  state.color = aConfig.color;
  state.flashMode = aConfig.flash;
  state.flashOnMS = aConfig.flashOnMS;
  state.flashOffMS = aConfig.flashOffMS;
  state.brightnessMode = aConfig.mode;

  sLights[light]->set_light(sLights[light], &state);
  sStoredLightState[light] = state;
  return true;
}

/**
* GET the value of a light returning a particular color, with a specific flash pattern.
* returns true if successful and false if failed.
*/
static bool
GetLight(LightType light, LightConfiguration* aConfig)
{
  light_state_t state;

  if (light < 0 || light >= eHalLightID_Count ||
      sLights[light] == nullptr) {
    return false;
  }

  memset(&state, 0, sizeof(light_state_t));
  state = sStoredLightState[light];

  aConfig->light = light;
  aConfig->color = state.color;
  aConfig->flash = FlashMode(state.flashMode);
  aConfig->flashOnMS = state.flashOnMS;
  aConfig->flashOffMS = state.flashOffMS;
  aConfig->mode = LightMode(state.brightnessMode);

  return true;
}

namespace {

/**
 * This runnable runs for the lifetime of the program, once started.  It's
 * responsible for "playing" vibration patterns.
 */
class VibratorRunnable final
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

protected:
  ~VibratorRunnable() {}

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

    LightConfiguration aConfig;
    aConfig.light = eHalLightID_Battery;
    aConfig.mode = eHalLightMode_User;
    aConfig.flash = eHalLightFlash_None;
    aConfig.flashOnMS = aConfig.flashOffMS = 0;
    aConfig.color = color;

    SetLight(eHalLightID_Battery, aConfig);

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

class BatteryObserver final : public IUeventObserver
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

protected:
  ~BatteryObserver() {}

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
    HAL_LOG("charge level contains unknown value: %d", *aCharge);
  }
  #endif

  return (*aCharge >= 0) && (*aCharge <= 100);
}

static bool
GetCurrentBatteryCharging(int* aCharging)
{
  static const DebugOnly<int> BATTERY_NOT_CHARGING = 0;
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
      HAL_LOG("charging_source contained unknown value: %d", chargingSrc);
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
  static bool previousCharging = false;
  static double previousLevel = 0.0, remainingTime = 0.0;
  static struct timespec lastLevelChange;
  struct timespec now;
  double dtime, dlevel;

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

  if (aBatteryInfo->charging() != previousCharging){
    aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
    memset(&lastLevelChange, 0, sizeof(struct timespec));
    remainingTime = 0.0;
  }

  if (aBatteryInfo->charging()) {
    if (aBatteryInfo->level() == 1.0) {
      aBatteryInfo->remainingTime() = dom::battery::kDefaultRemainingTime;
    } else if (aBatteryInfo->level() != previousLevel){
      if (lastLevelChange.tv_sec != 0) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        dtime = now.tv_sec - lastLevelChange.tv_sec;
        dlevel = aBatteryInfo->level() - previousLevel;

        if (dlevel <= 0.0) {
          aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
        } else {
          remainingTime = (double) round(dtime / dlevel * (1.0 - aBatteryInfo->level()));
          aBatteryInfo->remainingTime() = remainingTime;
        }

        lastLevelChange = now;
      } else { // lastLevelChange.tv_sec == 0
        clock_gettime(CLOCK_MONOTONIC, &lastLevelChange);
        aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
      }

    } else {
      clock_gettime(CLOCK_MONOTONIC, &now);
      dtime = now.tv_sec - lastLevelChange.tv_sec;
      if (dtime < remainingTime) {
        aBatteryInfo->remainingTime() = round(remainingTime - dtime);
      } else {
        aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
      }

    }

  } else {
    if (aBatteryInfo->level() == 0.0) {
      aBatteryInfo->remainingTime() = dom::battery::kDefaultRemainingTime;
    } else if (aBatteryInfo->level() != previousLevel){
      if (lastLevelChange.tv_sec != 0) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        dtime = now.tv_sec - lastLevelChange.tv_sec;
        dlevel = previousLevel - aBatteryInfo->level();

        if (dlevel <= 0.0) {
          aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
        } else {
          remainingTime = (double) round(dtime / dlevel * aBatteryInfo->level());
          aBatteryInfo->remainingTime() = remainingTime;
        }

        lastLevelChange = now;
      } else { // lastLevelChange.tv_sec == 0
        clock_gettime(CLOCK_MONOTONIC, &lastLevelChange);
        aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
      }

    } else {
      clock_gettime(CLOCK_MONOTONIC, &now);
      dtime = now.tv_sec - lastLevelChange.tv_sec;
      if (dtime < remainingTime) {
        aBatteryInfo->remainingTime() = round(remainingTime - dtime);
      } else {
        aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
      }

    }
  }

  previousCharging = aBatteryInfo->charging();
  previousLevel = aBatteryInfo->level();
}

namespace {

/**
 * RAII class to help us remember to close file descriptors.
 */

bool WriteToFile(const char *filename, const char *toWrite)
{
  int fd = open(filename, O_WRONLY);
  ScopedClose autoClose(fd);
  if (fd < 0) {
    HAL_LOG("Unable to open file %s.", filename);
    return false;
  }

  if (write(fd, toWrite, strlen(toWrite)) < 0) {
    HAL_LOG("Unable to write to file %s.", filename);
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
  LightConfiguration config;
  GetLight(eHalLightID_Buttons, &config);
  return (config.color != 0x00000000);
}

void
SetKeyLightEnabled(bool aEnabled)
{
  LightConfiguration config;
  config.mode = eHalLightMode_User;
  config.flash = eHalLightFlash_None;
  config.flashOnMS = config.flashOffMS = 0;
  config.color = 0x00000000;

  if (aEnabled) {
    // Convert the value in [0, 1] to an int between 0 and 255 and then convert
    // it to a color. Note that the high byte is FF, corresponding to the alpha
    // channel.
    double brightness = GetScreenBrightness();
    uint32_t val = static_cast<int>(round(brightness * 255.0));
    uint32_t color = (0xff<<24) + (val<<16) + (val<<8) + val;

    config.color = color;
  }

  SetLight(eHalLightID_Buttons, config);
  SetLight(eHalLightID_Keyboard, config);
}

double
GetScreenBrightness()
{
  LightConfiguration config;
  LightType light = eHalLightID_Backlight;

  GetLight(light, &config);
  // backlight is brightness only, so using one of the RGB elements as value.
  int brightness = config.color & 0xFF;
  return brightness / 255.0;
}

void
SetScreenBrightness(double brightness)
{
  // Don't use De Morgan's law to push the ! into this expression; we want to
  // catch NaN too.
  if (!(0 <= brightness && brightness <= 1)) {
    HAL_LOG("SetScreenBrightness: Dropping illegal brightness %f.", brightness);
    return;
  }

  // Convert the value in [0, 1] to an int between 0 and 255 and convert to a color
  // note that the high byte is FF, corresponding to the alpha channel.
  uint32_t val = static_cast<int>(round(brightness * 255.0));
  uint32_t color = (0xff<<24) + (val<<16) + (val<<8) + val;

  LightConfiguration config;
  config.mode = eHalLightMode_User;
  config.flash = eHalLightFlash_None;
  config.flashOnMS = config.flashOffMS = 0;
  config.color = color;
  SetLight(eHalLightID_Backlight, config);
  if (GetKeyLightEnabled()) {
    SetLight(eHalLightID_Buttons, config);
    SetLight(eHalLightID_Keyboard, config);
  }
}

static Monitor* sInternalLockCpuMonitor = nullptr;

static void
UpdateCpuSleepState()
{
  const char *wakeLockFilename = "/sys/power/wake_lock";
  const char *wakeUnlockFilename = "/sys/power/wake_unlock";

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
    HAL_LOG("Failed to open /dev/alarm: %s", strerror(errno));
    return;
  }

  if (ioctl(fd, ANDROID_ALARM_SET_RTC, &now) < 0) {
    HAL_LOG("ANDROID_ALARM_SET_RTC failed: %s", strerror(errno));
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
  nsRefPtr<nsScreenGonk> screen = nsScreenManagerGonk::GetPrimaryScreen();
  *aScreenConfiguration = screen->GetConfiguration();
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
    HAL_LOG("Failed to open alarm device: %s.", strerror(errno));
    return false;
  }

  nsAutoPtr<AlarmData> alarmData(new AlarmData(alarmFd));

  struct sigaction actions;
  memset(&actions, 0, sizeof(actions));
  sigemptyset(&actions.sa_mask);
  actions.sa_flags = 0;
  actions.sa_handler = ShutDownAlarm;
  if (sigaction(SIGUSR1, &actions, nullptr)) {
    HAL_LOG("Failed to set SIGUSR1 signal for alarm-watcher thread.");
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
    HAL_LOG("Failed to create alarm-watcher thread. Status: %d.", status);
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
    HAL_LOG("We should have enabled the alarm.");
    return false;
  }

  struct timespec ts;
  ts.tv_sec = aSeconds;
  ts.tv_nsec = aNanoseconds;

  // Currently we only support RTC wakeup alarm type.
  const int result = ioctl(sAlarmData->mFd,
                           ANDROID_ALARM_SET(ANDROID_ALARM_RTC_WAKEUP), &ts);

  if (result < 0) {
    HAL_LOG("Unable to set alarm: %s.", strerror(errno));
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
RoundOomScoreAdjUpWithLRU(int& aOomScoreAdj, uint32_t aLRU)
{
  // We want to add minimum value to round OomScoreAdj up according to
  // the steps by aLRU.
  aOomScoreAdj +=
    ceil(((float)OOM_SCORE_ADJ_MAX / OOM_ADJUST_MAX) * aLRU);
}

#define OOM_LOG(level, args...) __android_log_print(level, "OomLogger", ##args)
class OomVictimLogger final
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

protected:
  ~OomVictimLogger() {}

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
  // (ICS doesn't have KLOG_SIZE_BUFFER but 
  // JB and onwards does.)
  #define KLOG_SIZE_BUFFER KLOG_WRITE
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

/**
 * Wraps a particular ProcessPriority, giving us easy access to the prefs that
 * are relevant to it.
 *
 * Creating a PriorityClass also ensures that the control group is created.
 */
class PriorityClass
{
public:
  /**
   * Create a PriorityClass for the given ProcessPriority.  This implicitly
   * reads the relevant prefs and opens the cgroup.procs file of the relevant
   * control group caching its file descriptor for later use.
   */
  PriorityClass(ProcessPriority aPriority);

  /**
   * Closes the file descriptor for the cgroup.procs file of the associated
   * control group.
   */
  ~PriorityClass();

  PriorityClass(const PriorityClass& aOther);
  PriorityClass& operator=(const PriorityClass& aOther);

  ProcessPriority Priority()
  {
    return mPriority;
  }

  int32_t OomScoreAdj()
  {
    return clamped<int32_t>(mOomScoreAdj, OOM_SCORE_ADJ_MIN, OOM_SCORE_ADJ_MAX);
  }

  int32_t KillUnderKB()
  {
    return mKillUnderKB;
  }

  nsCString CGroup()
  {
    return mGroup;
  }

  /**
   * Adds a process to this priority class, this moves the process' PID into
   * the associated control group.
   *
   * @param aPid The PID of the process to be added.
   */
  void AddProcess(int aPid);

private:
  ProcessPriority mPriority;
  int32_t mOomScoreAdj;
  int32_t mKillUnderKB;
  int mCpuCGroupProcsFd;
  int mMemCGroupProcsFd;
  nsCString mGroup;

  /**
   * Return a string that identifies where we can find the value of aPref
   * that's specific to mPriority.  For example, we might return
   * "hal.processPriorityManager.gonk.FOREGROUND_HIGH.oomScoreAdjust".
   */
  nsCString PriorityPrefName(const char* aPref)
  {
    return nsPrintfCString("hal.processPriorityManager.gonk.%s.%s",
                           ProcessPriorityToString(mPriority), aPref);
  }

  /**
   * Get the full path of the cgroup.procs file associated with the group.
   */
  nsCString CpuCGroupProcsFilename()
  {
    nsCString cgroupName = mGroup;

    /* If mGroup is empty, our cgroup.procs file is the root procs file,
     * located at /dev/cpuctl/cgroup.procs.  Otherwise our procs file is
     * /dev/cpuctl/NAME/cgroup.procs. */

    if (!mGroup.IsEmpty()) {
      cgroupName.AppendLiteral("/");
    }

    return NS_LITERAL_CSTRING("/dev/cpuctl/") + cgroupName +
           NS_LITERAL_CSTRING("cgroup.procs");
  }

  nsCString MemCGroupProcsFilename()
  {
    nsCString cgroupName = mGroup;

    /* If mGroup is empty, our cgroup.procs file is the root procs file,
     * located at /sys/fs/cgroup/memory/cgroup.procs.  Otherwise our procs 
     * file is /sys/fs/cgroup/memory/NAME/cgroup.procs. */

    if (!mGroup.IsEmpty()) {
      cgroupName.AppendLiteral("/");
    }

    return NS_LITERAL_CSTRING("/sys/fs/cgroup/memory/") + cgroupName +
           NS_LITERAL_CSTRING("cgroup.procs");
  }

  int OpenCpuCGroupProcs()
  {
    return open(CpuCGroupProcsFilename().get(), O_WRONLY);
  }

  int OpenMemCGroupProcs()
  {
    return open(MemCGroupProcsFilename().get(), O_WRONLY);
  }
};

/**
 * Try to create the cgroup for the given PriorityClass, if it doesn't already
 * exist.  This essentially implements mkdir -p; that is, we create parent
 * cgroups as necessary.  The group parameters are also set according to
 * the corresponding preferences.
 *
 * @param aGroup The name of the group.
 * @return true if we successfully created the cgroup, or if it already
 * exists.  Otherwise, return false.
 */
static bool
EnsureCpuCGroupExists(const nsACString &aGroup)
{
  NS_NAMED_LITERAL_CSTRING(kDevCpuCtl, "/dev/cpuctl/");
  NS_NAMED_LITERAL_CSTRING(kSlash, "/");

  nsAutoCString groupName(aGroup);
  HAL_LOG("EnsureCpuCGroupExists for group '%s'", groupName.get());

  nsAutoCString prefPrefix("hal.processPriorityManager.gonk.cgroups.");

  /* If cgroup is not empty, append the cgroup name and a dot to obtain the
   * group specific preferences. */
  if (!aGroup.IsEmpty()) {
    prefPrefix += aGroup + NS_LITERAL_CSTRING(".");
  }

  nsAutoCString cpuSharesPref(prefPrefix + NS_LITERAL_CSTRING("cpu_shares"));
  int cpuShares = Preferences::GetInt(cpuSharesPref.get());

  nsAutoCString cpuNotifyOnMigratePref(prefPrefix
    + NS_LITERAL_CSTRING("cpu_notify_on_migrate"));
  int cpuNotifyOnMigrate = Preferences::GetInt(cpuNotifyOnMigratePref.get());

  // Create mCGroup and its parent directories, as necessary.
  nsCString cgroupIter = aGroup + kSlash;

  int32_t offset = 0;
  while ((offset = cgroupIter.FindChar('/', offset)) != -1) {
    nsAutoCString path = kDevCpuCtl + Substring(cgroupIter, 0, offset);
    int rv = mkdir(path.get(), 0744);

    if (rv == -1 && errno != EEXIST) {
      HAL_LOG("Could not create the %s control group.", path.get());
      return false;
    }

    offset++;
  }
  HAL_LOG("EnsureCpuCGroupExists created group '%s'", groupName.get());

  nsAutoCString pathPrefix(kDevCpuCtl + aGroup + kSlash);
  nsAutoCString cpuSharesPath(pathPrefix + NS_LITERAL_CSTRING("cpu.shares"));
  if (cpuShares && !WriteToFile(cpuSharesPath.get(),
                                nsPrintfCString("%d", cpuShares).get())) {
    HAL_LOG("Could not set the cpu share for group %s", cpuSharesPath.get());
    return false;
  }

  nsAutoCString notifyOnMigratePath(pathPrefix
    + NS_LITERAL_CSTRING("cpu.notify_on_migrate"));
  if (!WriteToFile(notifyOnMigratePath.get(),
                   nsPrintfCString("%d", cpuNotifyOnMigrate).get())) {
    HAL_LOG("Could not set the cpu migration notification flag for group %s",
            notifyOnMigratePath.get());
    return false;
  }

  return true;
}

static bool
EnsureMemCGroupExists(const nsACString &aGroup)
{
  NS_NAMED_LITERAL_CSTRING(kMemCtl, "/sys/fs/cgroup/memory/");
  NS_NAMED_LITERAL_CSTRING(kSlash, "/");

  nsAutoCString groupName(aGroup);
  HAL_LOG("EnsureMemCGroupExists for group '%s'", groupName.get());

  nsAutoCString prefPrefix("hal.processPriorityManager.gonk.cgroups.");

  /* If cgroup is not empty, append the cgroup name and a dot to obtain the
   * group specific preferences. */
  if (!aGroup.IsEmpty()) {
    prefPrefix += aGroup + NS_LITERAL_CSTRING(".");
  }

  nsAutoCString memSwappinessPref(prefPrefix + NS_LITERAL_CSTRING("memory_swappiness"));
  int memSwappiness = Preferences::GetInt(memSwappinessPref.get());

  // Create mCGroup and its parent directories, as necessary.
  nsCString cgroupIter = aGroup + kSlash;

  int32_t offset = 0;
  while ((offset = cgroupIter.FindChar('/', offset)) != -1) {
    nsAutoCString path = kMemCtl + Substring(cgroupIter, 0, offset);
    int rv = mkdir(path.get(), 0744);

    if (rv == -1 && errno != EEXIST) {
      HAL_LOG("Could not create the %s control group.", path.get());
      return false;
    }

    offset++;
  }
  HAL_LOG("EnsureMemCGroupExists created group '%s'", groupName.get());

  nsAutoCString pathPrefix(kMemCtl + aGroup + kSlash);
  nsAutoCString memSwappinessPath(pathPrefix + NS_LITERAL_CSTRING("memory.swappiness"));
  if (!WriteToFile(memSwappinessPath.get(),
                   nsPrintfCString("%d", memSwappiness).get())) {
    HAL_LOG("Could not set the memory.swappiness for group %s", memSwappinessPath.get());
    return false;
  }
  HAL_LOG("Set memory.swappiness for group %s to %d", memSwappinessPath.get(), memSwappiness);

  return true;
}

PriorityClass::PriorityClass(ProcessPriority aPriority)
  : mPriority(aPriority)
  , mOomScoreAdj(0)
  , mKillUnderKB(0)
  , mCpuCGroupProcsFd(-1)
  , mMemCGroupProcsFd(-1)
{
  DebugOnly<nsresult> rv;

  rv = Preferences::GetInt(PriorityPrefName("OomScoreAdjust").get(),
                           &mOomScoreAdj);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Missing oom_score_adj preference");

  rv = Preferences::GetInt(PriorityPrefName("KillUnderKB").get(),
                           &mKillUnderKB);

  rv = Preferences::GetCString(PriorityPrefName("cgroup").get(), &mGroup);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Missing control group preference");

  if (EnsureCpuCGroupExists(mGroup)) {
    mCpuCGroupProcsFd = OpenCpuCGroupProcs();
  }
  if (EnsureMemCGroupExists(mGroup)) {
    mMemCGroupProcsFd = OpenMemCGroupProcs();
  }
}

PriorityClass::~PriorityClass()
{
  close(mCpuCGroupProcsFd);
  close(mMemCGroupProcsFd);
}

PriorityClass::PriorityClass(const PriorityClass& aOther)
  : mPriority(aOther.mPriority)
  , mOomScoreAdj(aOther.mOomScoreAdj)
  , mKillUnderKB(aOther.mKillUnderKB)
  , mGroup(aOther.mGroup)
{
  mCpuCGroupProcsFd = OpenCpuCGroupProcs();
  mMemCGroupProcsFd = OpenMemCGroupProcs();
}

PriorityClass& PriorityClass::operator=(const PriorityClass& aOther)
{
  mPriority = aOther.mPriority;
  mOomScoreAdj = aOther.mOomScoreAdj;
  mKillUnderKB = aOther.mKillUnderKB;
  mGroup = aOther.mGroup;
  mCpuCGroupProcsFd = OpenCpuCGroupProcs();
  mMemCGroupProcsFd = OpenMemCGroupProcs();
  return *this;
}

void PriorityClass::AddProcess(int aPid)
{
  if (mCpuCGroupProcsFd >= 0) {
    nsPrintfCString str("%d", aPid);

    if (write(mCpuCGroupProcsFd, str.get(), strlen(str.get())) < 0) {
      HAL_ERR("Couldn't add PID %d to the %s cpu control group", aPid, mGroup.get());
    }
  }
  if (mMemCGroupProcsFd >= 0) {
    nsPrintfCString str("%d", aPid);

    if (write(mMemCGroupProcsFd, str.get(), strlen(str.get())) < 0) {
      HAL_ERR("Couldn't add PID %d to the %s memory control group", aPid, mGroup.get());
    }
  }
}

/**
 * Get the PriorityClass associated with the given ProcessPriority.
 *
 * If you pass an invalid ProcessPriority value, we return null.
 *
 * The pointers returned here are owned by GetPriorityClass (don't free them
 * yourself).  They are guaranteed to stick around until shutdown.
 */
PriorityClass*
GetPriorityClass(ProcessPriority aPriority)
{
  static StaticAutoPtr<nsTArray<PriorityClass>> priorityClasses;

  // Initialize priorityClasses if this is the first time we're running this
  // method.
  if (!priorityClasses) {
    priorityClasses = new nsTArray<PriorityClass>();
    ClearOnShutdown(&priorityClasses);

    for (int32_t i = 0; i < NUM_PROCESS_PRIORITY; i++) {
      priorityClasses->AppendElement(PriorityClass(ProcessPriority(i)));
    }
  }

  if (aPriority < 0 ||
      static_cast<uint32_t>(aPriority) >= priorityClasses->Length()) {
    return nullptr;
  }

  return &(*priorityClasses)[aPriority];
}

static void
EnsureKernelLowMemKillerParamsSet()
{
  static bool kernelLowMemKillerParamsSet;
  if (kernelLowMemKillerParamsSet) {
    return;
  }
  kernelLowMemKillerParamsSet = true;

  HAL_LOG("Setting kernel's low-mem killer parameters.");

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

  DebugOnly<int32_t> lowerBoundOfNextOomScoreAdj = OOM_SCORE_ADJ_MIN - 1;
  DebugOnly<int32_t> lowerBoundOfNextKillUnderKB = 0;
  int32_t countOfLowmemorykillerParametersSets = 0;

  long page_size = sysconf(_SC_PAGESIZE);

  for (int i = NUM_PROCESS_PRIORITY - 1; i >= 0; i--) {
    // The system doesn't function correctly if we're missing these prefs, so
    // crash loudly.

    PriorityClass* pc = GetPriorityClass(static_cast<ProcessPriority>(i));

    int32_t oomScoreAdj = pc->OomScoreAdj();
    int32_t killUnderKB = pc->KillUnderKB();

    if (killUnderKB == 0) {
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
    minfreeParams.AppendPrintf("%ld,", killUnderKB * 1024 / page_size);

    lowerBoundOfNextOomScoreAdj = oomScoreAdj;
    lowerBoundOfNextKillUnderKB = killUnderKB;
    countOfLowmemorykillerParametersSets++;
  }

  // Strip off trailing commas.
  adjParams.Cut(adjParams.Length() - 1, 1);
  minfreeParams.Cut(minfreeParams.Length() - 1, 1);
  if (!adjParams.IsEmpty() && !minfreeParams.IsEmpty()) {
    WriteToFile("/sys/module/lowmemorykiller/parameters/adj", adjParams.get());
    WriteToFile("/sys/module/lowmemorykiller/parameters/minfree",
                minfreeParams.get());
  }

  // Set the low-memory-notification threshold.
  int32_t lowMemNotifyThresholdKB;
  if (NS_SUCCEEDED(Preferences::GetInt(
        "hal.processPriorityManager.gonk.notifyLowMemUnderKB",
        &lowMemNotifyThresholdKB))) {

    // notify_trigger is in pages.
    WriteToFile("/sys/module/lowmemorykiller/parameters/notify_trigger",
      nsPrintfCString("%ld", lowMemNotifyThresholdKB * 1024 / page_size).get());
  }

  // Ensure OOM events appear in logcat
  nsRefPtr<OomVictimLogger> oomLogger = new OomVictimLogger();
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->AddObserver(oomLogger, "ipc:content-shutdown", false);
  }
}

void
SetProcessPriority(int aPid, ProcessPriority aPriority, uint32_t aLRU)
{
  HAL_LOG("SetProcessPriority(pid=%d, priority=%d, LRU=%u)",
          aPid, aPriority, aLRU);

  // If this is the first time SetProcessPriority was called, set the kernel's
  // OOM parameters according to our prefs.
  //
  // We could/should do this on startup instead of waiting for the first
  // SetProcessPriorityCall.  But in practice, the master process needs to set
  // its priority early in the game, so we can reasonably rely on
  // SetProcessPriority being called early in startup.
  EnsureKernelLowMemKillerParamsSet();

  PriorityClass* pc = GetPriorityClass(aPriority);

  int oomScoreAdj = pc->OomScoreAdj();

  RoundOomScoreAdjUpWithLRU(oomScoreAdj, aLRU);

  // We try the newer interface first, and fall back to the older interface
  // on failure.
  if (!WriteToFile(nsPrintfCString("/proc/%d/oom_score_adj", aPid).get(),
                   nsPrintfCString("%d", oomScoreAdj).get()))
  {
    WriteToFile(nsPrintfCString("/proc/%d/oom_adj", aPid).get(),
                nsPrintfCString("%d", OomAdjOfOomScoreAdj(oomScoreAdj)).get());
  }

  HAL_LOG("Assigning pid %d to cgroup %s", aPid, pc->CGroup().get());
  pc->AddProcess(aPid);
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

  HAL_LOG("Setting thread %d to priority level %s; nice level %d",
          aTid, ThreadPriorityToString(aThreadPriority), aValue);
  int rv = setpriority(PRIO_PROCESS, aTid, aValue);

  if (rv) {
    HAL_LOG("Failed to set thread %d to priority level %s; error %s", aTid,
            ThreadPriorityToString(aThreadPriority), strerror(errno));
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
  HAL_LOG("Setting thread %d to priority level %s; Real Time priority %d, "
          "Schedule FIFO", aTid, ThreadPriorityToString(aThreadPriority),
          aValue);
  sched_param schedParam;
  schedParam.sched_priority = aValue;
  int rv = sched_setscheduler(aTid, policy, &schedParam);

  if (rv) {
    HAL_LOG("Failed to set thread %d to real time priority level %s; error %s",
            aTid, ThreadPriorityToString(aThreadPriority), strerror(errno));
  }
}

/*
 * Used to store the nice value adjustments and real time priorities for the
 * various thread priority levels.
 */
struct ThreadPriorityPrefs {
  bool initialized;
  struct {
    int nice;
    int realTime;
  } priorities[NUM_THREAD_PRIORITY];
};

/*
 * Reads the preferences for the various process priority levels and sets up
 * watchers so that if they're dynamically changed the change is reflected on
 * the appropriate variables.
 */
void
EnsureThreadPriorityPrefs(ThreadPriorityPrefs* prefs)
{
  if (prefs->initialized) {
    return;
  }

  for (int i = THREAD_PRIORITY_COMPOSITOR; i < NUM_THREAD_PRIORITY; i++) {
    ThreadPriority priority = static_cast<ThreadPriority>(i);

    // Read the nice values
    const char* threadPriorityStr = ThreadPriorityToString(priority);
    nsPrintfCString niceStr("hal.gonk.%s.nice", threadPriorityStr);
    Preferences::AddIntVarCache(&prefs->priorities[i].nice, niceStr.get());

    // Read the real-time priorities
    nsPrintfCString realTimeStr("hal.gonk.%s.rt_priority", threadPriorityStr);
    Preferences::AddIntVarCache(&prefs->priorities[i].realTime,
                                realTimeStr.get());
  }

  prefs->initialized = true;
}

static void
DoSetThreadPriority(pid_t aTid, hal::ThreadPriority aThreadPriority)
{
  // See bug 999115, we can only read preferences on the main thread otherwise
  // we create a race condition in HAL
  MOZ_ASSERT(NS_IsMainThread(), "Can only set thread priorities on main thread");
  MOZ_ASSERT(aThreadPriority >= 0);

  static ThreadPriorityPrefs prefs = { 0 };
  EnsureThreadPriorityPrefs(&prefs);

  switch (aThreadPriority) {
    case THREAD_PRIORITY_COMPOSITOR:
      break;
    default:
      HAL_ERR("Unrecognized thread priority %d; Doing nothing",
              aThreadPriority);
      return;
  }

  int realTimePriority = prefs.priorities[aThreadPriority].realTime;

  if (IsValidRealTimePriority(realTimePriority, SCHED_FIFO)) {
    SetRealTimeThreadPriority(aTid, aThreadPriority, realTimePriority);
    return;
  }

  SetThreadNiceValue(aTid, aThreadPriority,
                     prefs.priorities[aThreadPriority].nice);
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
    hal_impl::DoSetThreadPriority(mThreadId, mThreadPriority);
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
  pid_t threadId = gettid();
  hal_impl::SetThreadPriority(threadId, aThreadPriority);
}

void
SetThreadPriority(PlatformThreadId aThreadId,
                         ThreadPriority aThreadPriority)
{
  switch (aThreadPriority) {
    case THREAD_PRIORITY_COMPOSITOR: {
      nsCOMPtr<nsIRunnable> runnable =
        new SetThreadPriorityRunnable(aThreadId, aThreadPriority);
      NS_DispatchToMainThread(runnable);
      break;
    }
    default:
      HAL_LOG("Unrecognized thread priority %d; Doing nothing",
              aThreadPriority);
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
  } else if (aReason == FactoryResetReason::Root) {
    recoveryService->FactoryReset("root");
  } else {
    recoveryService->FactoryReset("normal");
  }
}

} // hal_impl
} // mozilla
