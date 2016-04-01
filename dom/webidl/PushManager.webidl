/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* The origin of this IDL file is
* https://w3c.github.io/push-api/
*/

// The main thread JS implementation. Please see comments in
// dom/push/PushManager.h for the split between PushManagerImpl and PushManager.
[JSImplementation="@mozilla.org/push/PushManager;1",
 ChromeOnly, Constructor(DOMString scope)]
interface PushManagerImpl {
  Promise<PushSubscription>    subscribe();
  Promise<PushSubscription?>   getSubscription();
  Promise<PushPermissionState> permissionState();
};

[Exposed=(Window,Worker), Func="nsContentUtils::PushEnabled",
 ChromeConstructor(DOMString scope)]
interface PushManager {
  [Throws, UseCounter]
  Promise<PushSubscription>    subscribe();
  [Throws]
  Promise<PushSubscription?>   getSubscription();
  [Throws]
  Promise<PushPermissionState> permissionState();
};

enum PushPermissionState
{
    "granted",
    "denied",
    "prompt"
};
