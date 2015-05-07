/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Hal_h
#define mozilla_Hal_h

#include "mozilla/hal_sandbox/PHal.h"
#include "mozilla/HalTypes.h"
#include "base/basictypes.h"
#include "base/platform_thread.h"
#include "mozilla/Observer.h"
#include "mozilla/Types.h"
#include "nsTArray.h"
#include "mozilla/dom/MozPowerManagerBinding.h"
#include "mozilla/dom/battery/Types.h"
#include "mozilla/dom/network/Types.h"
#include "mozilla/dom/power/Types.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/HalScreenConfiguration.h"

/*
 * Hal.h contains the public Hal API.
 *
 * By default, this file defines its functions in the hal namespace, but if
 * MOZ_HAL_NAMESPACE is defined, we'll define our functions in that namespace.
 *
 * This is used by HalImpl.h and HalSandbox.h, which define copies of all the
 * functions here in the hal_impl and hal_sandbox namespaces.
 */

class nsIDOMWindow;

#ifndef MOZ_HAL_NAMESPACE
# define MOZ_HAL_NAMESPACE hal
# define MOZ_DEFINED_HAL_NAMESPACE 1
#endif

namespace mozilla {

namespace hal {

typedef Observer<void_t> AlarmObserver;

class WindowIdentifier;

typedef Observer<int64_t> SystemClockChangeObserver;
typedef Observer<SystemTimezoneChangeInformation> SystemTimezoneChangeObserver;

} // namespace hal

namespace MOZ_HAL_NAMESPACE {

/**
 * Turn the default vibrator device on/off per the pattern specified
 * by |pattern|.  Each element in the pattern is the number of
 * milliseconds to turn the vibrator on or off.  The first element in
 * |pattern| is an "on" element, the next is "off", and so on.
 *
 * If |pattern| is empty, any in-progress vibration is canceled.
 *
 * Only an active window within an active tab may call Vibrate; calls
 * from inactive windows and windows on inactive tabs do nothing.
 *
 * If you're calling hal::Vibrate from the outside world, pass an
 * nsIDOMWindow* in place of the WindowIdentifier parameter.
 * The method with WindowIdentifier will be called automatically.
 */
void Vibrate(const nsTArray<uint32_t>& pattern,
             nsIDOMWindow* aWindow);
void Vibrate(const nsTArray<uint32_t>& pattern,
             const hal::WindowIdentifier &id);

/**
 * Cancel a vibration started by the content window identified by
 * WindowIdentifier.
 *
 * If the window was the last window to start a vibration, the
 * cancellation request will go through even if the window is not
 * active.
 *
 * As with hal::Vibrate(), if you're calling hal::CancelVibrate from the outside
 * world, pass an nsIDOMWindow*. The method with WindowIdentifier will be called
 * automatically.
 */
void CancelVibrate(nsIDOMWindow* aWindow);
void CancelVibrate(const hal::WindowIdentifier &id);

/**
 * Inform the battery backend there is a new battery observer.
 * @param aBatteryObserver The observer that should be added.
 */
void RegisterBatteryObserver(BatteryObserver* aBatteryObserver);

/**
 * Inform the battery backend a battery observer unregistered.
 * @param aBatteryObserver The observer that should be removed.
 */
void UnregisterBatteryObserver(BatteryObserver* aBatteryObserver);

/**
 * Returns the current battery information.
 */
void GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo);

/**
 * Notify of a change in the battery state.
 * @param aBatteryInfo The new battery information.
 */
void NotifyBatteryChange(const hal::BatteryInformation& aBatteryInfo);

/**
 * Determine whether the device's screen is currently enabled.
 */
bool GetScreenEnabled();

/**
 * Enable or disable the device's screen.
 *
 * Note that it may take a few seconds for the screen to turn on or off.
 */
void SetScreenEnabled(bool aEnabled);

/**
 * Determine whether the device's keypad/button backlight is currently enabled.
 */
bool GetKeyLightEnabled();

/**
 * Enable or disable the device's keypad/button backlight.
 */
void SetKeyLightEnabled(bool aEnabled);

/**
 * Get the brightness of the device's screen's backlight, on a scale from 0
 * (very dim) to 1 (full blast).
 *
 * If the display is currently disabled, this returns the brightness the
 * backlight will have when the display is re-enabled.
 */
double GetScreenBrightness();

/**
 * Set the brightness of the device's screen's backlight, on a scale from 0
 * (very dimm) to 1 (full blast).  Values larger than 1 are treated like 1, and
 * values smaller than 0 are treated like 0.
 *
 * Note that we may reduce the resolution of the given brightness value before
 * sending it to the screen.  Therefore if you call SetScreenBrightness(x)
 * followed by GetScreenBrightness(), the value returned by
 * GetScreenBrightness() may not be exactly x.
 */
void SetScreenBrightness(double aBrightness);

/**
 * Determine whether the device is allowed to sleep.
 */
bool GetCpuSleepAllowed();

/**
 * Set whether the device is allowed to suspend automatically after
 * the screen is disabled.
 */
void SetCpuSleepAllowed(bool aAllowed);

/**
 * Register an observer for the sensor of given type.
 *
 * The observer will receive data whenever the data generated by the
 * sensor is avaiable.
 */
void RegisterSensorObserver(hal::SensorType aSensor,
                            hal::ISensorObserver *aObserver);

/**
 * Unregister an observer for the sensor of given type.
 */
void UnregisterSensorObserver(hal::SensorType aSensor,
                              hal::ISensorObserver *aObserver);

/**
 * Post a value generated by a sensor.
 *
 * This API is internal to hal; clients shouldn't call it directly.
 */
void NotifySensorChange(const hal::SensorData &aSensorData);

/**
 * Enable sensor notifications from the backend
 *
 * This method is only visible from implementation of sensor manager.
 * Rest of the system should not try this.
 */
void EnableSensorNotifications(hal::SensorType aSensor);

/**
 * Disable sensor notifications from the backend
 *
 * This method is only visible from implementation of sensor manager.
 * Rest of the system should not try this.
 */
void DisableSensorNotifications(hal::SensorType aSensor);


/**
 * Inform the network backend there is a new network observer.
 * @param aNetworkObserver The observer that should be added.
 */
void RegisterNetworkObserver(NetworkObserver* aNetworkObserver);

/**
 * Inform the network backend a network observer unregistered.
 * @param aNetworkObserver The observer that should be removed.
 */
void UnregisterNetworkObserver(NetworkObserver* aNetworkObserver);

/**
 * Returns the current network information.
 */
void GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo);

/**
 * Notify of a change in the network state.
 * @param aNetworkInfo The new network information.
 */
void NotifyNetworkChange(const hal::NetworkInformation& aNetworkInfo);

/**
 * Adjusting system clock.
 * @param aDeltaMilliseconds The difference compared with current system clock.
 */
void AdjustSystemClock(int64_t aDeltaMilliseconds);

/**
 * Set timezone
 * @param aTimezoneSpec The definition can be found in
 * http://en.wikipedia.org/wiki/List_of_tz_database_time_zones
 */
void SetTimezone(const nsCString& aTimezoneSpec);

/**
 * Get timezone
 * http://en.wikipedia.org/wiki/List_of_tz_database_time_zones
 */
nsCString GetTimezone();

/**
 * Get timezone offset
 * returns the timezone offset relative to UTC in minutes (DST effect included)
 */
int32_t GetTimezoneOffset();

/**
 * Register observer for system clock changed notification.
 * @param aObserver The observer that should be added.
 */
void RegisterSystemClockChangeObserver(
  hal::SystemClockChangeObserver* aObserver);

/**
 * Unregister the observer for system clock changed.
 * @param aObserver The observer that should be removed.
 */
void UnregisterSystemClockChangeObserver(
  hal::SystemClockChangeObserver* aObserver);

/**
 * Notify of a change in the system clock.
 * @param aClockDeltaMS
 */
void NotifySystemClockChange(const int64_t& aClockDeltaMS);

/**
 * Register observer for system timezone changed notification.
 * @param aObserver The observer that should be added.
 */
void RegisterSystemTimezoneChangeObserver(
  hal::SystemTimezoneChangeObserver* aObserver);

/**
 * Unregister the observer for system timezone changed.
 * @param aObserver The observer that should be removed.
 */
void UnregisterSystemTimezoneChangeObserver(
  hal::SystemTimezoneChangeObserver* aObserver);

/**
 * Notify of a change in the system timezone.
 * @param aSystemTimezoneChangeInfo
 */
void NotifySystemTimezoneChange(
  const hal::SystemTimezoneChangeInformation& aSystemTimezoneChangeInfo);

/**
 * Reboot the device.
 *
 * This API is currently only allowed to be used from the main process.
 */
void Reboot();

/**
 * Power off the device.
 *
 * This API is currently only allowed to be used from the main process.
 */
void PowerOff();

/**
 * Enable wake lock notifications from the backend.
 *
 * This method is only used by WakeLockObserversManager.
 */
void EnableWakeLockNotifications();

/**
 * Disable wake lock notifications from the backend.
 *
 * This method is only used by WakeLockObserversManager.
 */
void DisableWakeLockNotifications();

/**
 * Inform the wake lock backend there is a new wake lock observer.
 * @param aWakeLockObserver The observer that should be added.
 */
void RegisterWakeLockObserver(WakeLockObserver* aObserver);

/**
 * Inform the wake lock backend a wake lock observer unregistered.
 * @param aWakeLockObserver The observer that should be removed.
 */
void UnregisterWakeLockObserver(WakeLockObserver* aObserver);

/**
 * Adjust a wake lock's counts on behalf of a given process.
 *
 * In most cases, you shouldn't need to pass the aProcessID argument; the
 * default of CONTENT_PROCESS_ID_UNKNOWN is probably what you want.
 *
 * @param aTopic        lock topic
 * @param aLockAdjust   to increase or decrease active locks
 * @param aHiddenAdjust to increase or decrease hidden locks
 * @param aProcessID    indicates which process we're modifying the wake lock
 *                      on behalf of.  It is interpreted as
 *
 *                      CONTENT_PROCESS_ID_UNKNOWN: The current process
 *                      CONTENT_PROCESS_ID_MAIN: The root process
 *                      X: The process with ContentChild::GetID() == X
 */
void ModifyWakeLock(const nsAString &aTopic,
                    hal::WakeLockControl aLockAdjust,
                    hal::WakeLockControl aHiddenAdjust,
                    uint64_t aProcessID = hal::CONTENT_PROCESS_ID_UNKNOWN);

/**
 * Query the wake lock numbers of aTopic.
 * @param aTopic        lock topic
 * @param aWakeLockInfo wake lock numbers
 */
void GetWakeLockInfo(const nsAString &aTopic, hal::WakeLockInformation *aWakeLockInfo);

/**
 * Notify of a change in the wake lock state.
 * @param aWakeLockInfo The new wake lock information.
 */
void NotifyWakeLockChange(const hal::WakeLockInformation& aWakeLockInfo);

/**
 * Inform the backend there is a new screen configuration observer.
 * @param aScreenConfigurationObserver The observer that should be added.
 */
void RegisterScreenConfigurationObserver(hal::ScreenConfigurationObserver* aScreenConfigurationObserver);

/**
 * Inform the backend a screen configuration observer unregistered.
 * @param aScreenConfigurationObserver The observer that should be removed.
 */
void UnregisterScreenConfigurationObserver(hal::ScreenConfigurationObserver* aScreenConfigurationObserver);

/**
 * Returns the current screen configuration.
 */
void GetCurrentScreenConfiguration(hal::ScreenConfiguration* aScreenConfiguration);

/**
 * Notify of a change in the screen configuration.
 * @param aScreenConfiguration The new screen orientation.
 */
void NotifyScreenConfigurationChange(const hal::ScreenConfiguration& aScreenConfiguration);

/**
 * Lock the screen orientation to the specific orientation.
 * @return Whether the lock has been accepted.
 */
bool LockScreenOrientation(const dom::ScreenOrientation& aOrientation);

/**
 * Unlock the screen orientation.
 */
void UnlockScreenOrientation();

/**
 * Register an observer for the switch of given SwitchDevice.
 *
 * The observer will receive data whenever the data generated by the
 * given switch.
 */
void RegisterSwitchObserver(hal::SwitchDevice aDevice, hal::SwitchObserver *aSwitchObserver);

/**
 * Unregister an observer for the switch of given SwitchDevice.
 */
void UnregisterSwitchObserver(hal::SwitchDevice aDevice, hal::SwitchObserver *aSwitchObserver);

/**
 * Notify the state of the switch.
 *
 * This API is internal to hal; clients shouldn't call it directly.
 */
void NotifySwitchChange(const hal::SwitchEvent& aEvent);

/**
 * Get current switch information.
 */
hal::SwitchState GetCurrentSwitchState(hal::SwitchDevice aDevice);

/**
 * Notify switch status change from input device.
 */
void NotifySwitchStateFromInputDevice(hal::SwitchDevice aDevice,
                                      hal::SwitchState aState);

/**
 * Register an observer that is notified when a programmed alarm
 * expires.
 *
 * Currently, there can only be 0 or 1 alarm observers.
 */
bool RegisterTheOneAlarmObserver(hal::AlarmObserver* aObserver);

/**
 * Unregister the alarm observer.  Doing so will implicitly cancel any
 * programmed alarm.
 */
void UnregisterTheOneAlarmObserver();

/**
 * Notify that the programmed alarm has expired.
 *
 * This API is internal to hal; clients shouldn't call it directly.
 */
void NotifyAlarmFired();

/**
 * Program the real-time clock to expire at the time |aSeconds|,
 * |aNanoseconds|.  These specify a point in real time relative to the
 * UNIX epoch.  The alarm will fire at this time point even if the
 * real-time clock is changed; that is, this alarm respects changes to
 * the real-time clock.  Return true iff the alarm was programmed.
 *
 * The alarm can be reprogrammed at any time.
 *
 * This API is currently only allowed to be used from non-sandboxed
 * contexts.
 */
bool SetAlarm(int32_t aSeconds, int32_t aNanoseconds);

/**
 * Set the priority of the given process.
 *
 * Exactly what this does will vary between platforms.  On *nix we might give
 * background processes higher nice values.  On other platforms, we might
 * ignore this call entirely.
 */
void SetProcessPriority(int aPid,
                        hal::ProcessPriority aPriority,
                        uint32_t aLRU = 0);


/**
 * Set the current thread's priority to appropriate platform-specific value for
 * given functionality. Instead of providing arbitrary priority numbers you
 * must specify a type of function like THREAD_PRIORITY_COMPOSITOR.
 */
void SetCurrentThreadPriority(hal::ThreadPriority aThreadPriority);

/**
 * Set a thread priority to appropriate platform-specific value for
 * given functionality. Instead of providing arbitrary priority numbers you
 * must specify a type of function like THREAD_PRIORITY_COMPOSITOR.
 */
void SetThreadPriority(PlatformThreadId aThreadId,
                       hal::ThreadPriority aThreadPriority);

/**
 * Register an observer for the FM radio.
 */
void RegisterFMRadioObserver(hal::FMRadioObserver* aRadioObserver);

/**
 * Unregister the observer for the FM radio.
 */
void UnregisterFMRadioObserver(hal::FMRadioObserver* aRadioObserver);

/**
 * Register an observer for the FM radio.
 */
void RegisterFMRadioRDSObserver(hal::FMRadioRDSObserver* aRDSObserver);

/**
 * Unregister the observer for the FM radio.
 */
void UnregisterFMRadioRDSObserver(hal::FMRadioRDSObserver* aRDSObserver);

/**
 * Notify observers that a call to EnableFMRadio, DisableFMRadio, or FMRadioSeek
 * has completed, and indicate what the call returned.
 */
void NotifyFMRadioStatus(const hal::FMRadioOperationInformation& aRadioState);

/**
 * Notify observers of new RDS data
 * This can be called on any thread.
 */
void NotifyFMRadioRDSGroup(const hal::FMRadioRDSGroup& aRDSGroup);

/**
 * Enable the FM radio and configure it according to the settings in aInfo.
 */
void EnableFMRadio(const hal::FMRadioSettings& aInfo);

/**
 * Disable the FM radio.
 */
void DisableFMRadio();

/**
 * Seek to an available FM radio station.
 *
 * This can be called off main thread, but all calls must be completed
 * before calling DisableFMRadio.
 */
void FMRadioSeek(const hal::FMRadioSeekDirection& aDirection);

/**
 * Get the current FM radio settings.
 */
void GetFMRadioSettings(hal::FMRadioSettings* aInfo);

/**
 * Set the FM radio's frequency.
 *
 * This can be called off main thread, but all calls must be completed
 * before calling DisableFMRadio.
 */
void SetFMRadioFrequency(const uint32_t frequency);

/**
 * Get the FM radio's frequency.
 */
uint32_t GetFMRadioFrequency();

/**
 * Get FM radio power state
 */
bool IsFMRadioOn();

/**
 * Get FM radio signal strength
 */
uint32_t GetFMRadioSignalStrength();

/**
 * Cancel FM radio seeking
 */
void CancelFMRadioSeek();

/**
 * Get FM radio band settings by country.
 */
hal::FMRadioSettings GetFMBandSettings(hal::FMRadioCountry aCountry);

/**
 * Enable RDS data reception
 */
bool EnableRDS(uint32_t aMask);

/**
 * Disable RDS data reception
 */
void DisableRDS();

/**
 * Start a watchdog to compulsively shutdown the system if it hangs.
 * @param aMode Specify how to shutdown the system.
 * @param aTimeoutSecs Specify the delayed seconds to shutdown the system.
 *
 * This API is currently only allowed to be used from the main process.
 */
void StartForceQuitWatchdog(hal::ShutdownMode aMode, int32_t aTimeoutSecs);

/**
 * Perform Factory Reset to wipe out all user data.
 */
void FactoryReset(mozilla::dom::FactoryResetReason& aReason);

/**
 * Start monitoring disk space for low space situations.
 *
 * This API is currently only allowed to be used from the main process.
 */
void StartDiskSpaceWatcher();

/**
 * Stop monitoring disk space for low space situations.
 *
 * This API is currently only allowed to be used from the main process.
 */
void StopDiskSpaceWatcher();

/**
 * Get total system memory of device being run on in bytes.
 *
 * Returns 0 if we are unable to determine this information from /proc/meminfo.
 */
uint32_t GetTotalSystemMemory();

/**
 * Get the level of total system memory on device in MiB.
 * (round the value up to the next power of two)
 *
 * Returns 0 if we are unable to determine this information from /proc/meminfo.
 */
uint32_t GetTotalSystemMemoryLevel();

/**
 * Determine whether the headphone switch event is from input device
 */
bool IsHeadphoneEventFromInputDev();

} // namespace MOZ_HAL_NAMESPACE
} // namespace mozilla

#ifdef MOZ_DEFINED_HAL_NAMESPACE
# undef MOZ_DEFINED_HAL_NAMESPACE
# undef MOZ_HAL_NAMESPACE
#endif

#endif  // mozilla_Hal_h
