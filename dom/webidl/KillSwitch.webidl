/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
  * This API is intended to lock the device in a way that it has no value
  * anymore for a thief. A side effect is also protecting user's data. This
  * means that we expect the device to be:
  *  - locked so that only the legitimate user can unlock it
  *  - unable to communitate via ADB, Devtools, MTP and/or UMS, ...
  *  - unable to go into recovery or fastboot mode to avoid flashing anything
  */

[JSImplementation="@mozilla.org/moz-kill-switch;1",
 NavigatorProperty="mozKillSwitch",
 AvailableIn="CertifiedApps",
 CheckAnyPermissions="killswitch",
 Pref="dom.mozKillSwitch.enabled"]
interface KillSwitch {
  Promise<any> enable();
  Promise<any> disable();
};
