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
 *   Mounir Lamouri <mounir.lamouri@mozilla.com>
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

#ifndef mozilla_HalInternal_h
#define mozilla_HalInternal_h 1

/*
 * This file is included by HalImpl.h and HalSandbox.h with a mechanism similar
 * to Hal.h. That means those headers set MOZ_HAL_NAMESPACE to specify in which
 * namespace the internal functions should appear.
 *
 * The difference between Hal.h and HalInternal.h is that methods declared in
 * HalInternal.h don't appear in the hal namespace. That also means this file
 * should not be included except by HalInternal.h and HalSandbox.h.
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
void EnableScreenOrientationNotifications();

/**
 * Disables screen orientation notifications from the backend.
 */
void DisableScreenOrientationNotifications();

/**
 * Enable switch notifications from the backend
 */
void EnableSwitchNotifications(hal::SwitchDevice aDevice);

/**
 * Disable switch notifications from the backend
 */
void DisableSwitchNotifications(hal::SwitchDevice aDevice);

} // namespace MOZ_HAL_NAMESPACE
} // namespace mozilla

#endif  // mozilla_HalInternal_h
