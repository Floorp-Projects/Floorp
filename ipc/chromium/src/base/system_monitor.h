// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYSTEM_MONITOR_H_
#define BASE_SYSTEM_MONITOR_H_

#include "base/observer_list_threadsafe.h"
#include "base/singleton.h"

// Windows HiRes timers drain the battery faster so we need to know the battery
// status.  This isn't true for other platforms.
#if defined(OS_WIN)
#define ENABLE_BATTERY_MONITORING 1
#else
#undef ENABLE_BATTERY_MONITORING
#endif  // !OS_WIN

namespace base {

// Class for monitoring various system-related subsystems
// such as power management, network status, etc.
// TODO(mbelshe):  Add support beyond just power management.
class SystemMonitor {
 public:
  // Access to the Singleton
  static SystemMonitor* Get() {
    // Uses the LeakySingletonTrait because cleanup is optional.
    return
        Singleton<SystemMonitor, LeakySingletonTraits<SystemMonitor> >::get();
  }

  // Start the System Monitor within a process.  This method
  // is provided so that the battery check can be deferred.
  // The MessageLoop must be started before calling this
  // method.
  // This is a no-op on platforms for which ENABLE_BATTERY_MONITORING is
  // disabled.
  static void Start();

  //
  // Power-related APIs
  //

  // Is the computer currently on battery power.
  // Can be called on any thread.
  bool BatteryPower() {
    // Using a lock here is not necessary for just a bool.
    return battery_in_use_;
  }

  // Normalized list of power events.
  enum PowerEvent {
    POWER_STATE_EVENT,  // The Power status of the system has changed.
    SUSPEND_EVENT,      // The system is being suspended.
    RESUME_EVENT        // The system is being resumed.
  };

  // Callbacks will be called on the thread which creates the SystemMonitor.
  // During the callback, Add/RemoveObserver will block until the callbacks
  // are finished. Observers should implement quick callback functions; if
  // lengthy operations are needed, the observer should take care to invoke
  // the operation on an appropriate thread.
  class PowerObserver {
  public:
    // Notification of a change in power status of the computer, such
    // as from switching between battery and A/C power.
    virtual void OnPowerStateChange(SystemMonitor*) = 0;

    // Notification that the system is suspending.
    virtual void OnSuspend(SystemMonitor*) = 0;

    // Notification that the system is resuming.
    virtual void OnResume(SystemMonitor*) = 0;
  };

  // Add a new observer.
  // Can be called from any thread.
  // Must not be called from within a notification callback.
  void AddObserver(PowerObserver* obs);

  // Remove an existing observer.
  // Can be called from any thread.
  // Must not be called from within a notification callback.
  void RemoveObserver(PowerObserver* obs);

#if defined(OS_WIN)
  // Windows-specific handling of a WM_POWERBROADCAST message.
  // Embedders of this API should hook their top-level window
  // message loop and forward WM_POWERBROADCAST through this call.
  void ProcessWmPowerBroadcastMessage(int event_id);
#endif

  // Cross-platform handling of a power event.
  void ProcessPowerMessage(PowerEvent event_id);

  // Constructor.
  // Don't use this; access SystemMonitor via the Singleton.
  SystemMonitor();

 private:
  // Platform-specific method to check whether the system is currently
  // running on battery power.  Returns true if running on batteries,
  // false otherwise.
  bool IsBatteryPower();

  // Checks the battery status and notifies observers if the battery
  // status has changed.
  void BatteryCheck();

  // Functions to trigger notifications.
  void NotifyPowerStateChange();
  void NotifySuspend();
  void NotifyResume();

  scoped_refptr<ObserverListThreadSafe<PowerObserver> > observer_list_;
  bool battery_in_use_;
  bool suspended_;

#if defined(ENABLE_BATTERY_MONITORING)
  base::OneShotTimer<SystemMonitor> delayed_battery_check_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SystemMonitor);
};

}

#endif  // BASE_SYSTEM_MONITOR_H_
