/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HalInternal_h
#define mozilla_HalInternal_h 1

/*
 * This file is included by HalImpl.h and HalSandbox.h with a mechanism similar
 * to Hal.h. That means those headers set MOZ_HAL_NAMESPACE to specify in which
 * namespace the internal functions should appear.
 *
 * The difference between Hal.h and HalInternal.h is that methods declared in
 * HalInternal.h don't appear in the hal namespace. That also means this file
 * should not be included except by HalImpl.h and HalSandbox.h.
 */

#ifndef MOZ_HAL_NAMESPACE
# error "You shouldn't directly include HalInternal.h!"
#endif

namespace mozilla {
namespace MOZ_HAL_NAMESPACE {

/**
 * Enables battery notifications from the backend.
 */
void EnableBatteryNotifications();

/**
 * Disables battery notifications from the backend.
 */
void DisableBatteryNotifications();

/**
 * Enables network notifications from the backend.
 */
void EnableNetworkNotifications();

/**
 * Disables network notifications from the backend.
 */
void DisableNetworkNotifications();

/**
 * Enables screen orientation notifications from the backend.
 */
void EnableScreenConfigurationNotifications();

/**
 * Disables screen orientation notifications from the backend.
 */
void DisableScreenConfigurationNotifications();

/**
 * Enable switch notifications from the backend
 */
void EnableSwitchNotifications(hal::SwitchDevice aDevice);

/**
 * Disable switch notifications from the backend
 */
void DisableSwitchNotifications(hal::SwitchDevice aDevice);

/**
 * Enable alarm notifications from the backend.
 */
bool EnableAlarm();

/**
 * Disable alarm notifications from the backend.
 */
void DisableAlarm();

/**
 * Enable system clock change notifications from the backend.
 */
void EnableSystemClockChangeNotifications();

/**
 * Disable system clock change notifications from the backend.
 */
void DisableSystemClockChangeNotifications();

/**
 * Enable system timezone change notifications from the backend.
 */
void EnableSystemTimezoneChangeNotifications();

/**
 * Disable system timezone change notifications from the backend.
 */
void DisableSystemTimezoneChangeNotifications();

/**
 * Has the child-side HAL IPC object been destroyed?  If so, you shouldn't send
 * messages to hal_sandbox.
 */
bool HalChildDestroyed();
} // namespace MOZ_HAL_NAMESPACE
} // namespace mozilla

#endif  // mozilla_HalInternal_h
