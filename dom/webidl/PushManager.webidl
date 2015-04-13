/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* The origin of this IDL file is
* https://w3c.github.io/push-api/
*/

[JSImplementation="@mozilla.org/push/PushManager;1",
 Pref="dom.push.enabled"]
interface PushManager {
    Promise<PushSubscription>     subscribe();
    Promise<PushSubscription?>    getSubscription();
    Promise<PushPermissionStatus> hasPermission();

    // We need a setter in the bindings so that the C++ can use it,
    // but we don't want it exposed to client JS.  WebPushMethodHider
    // always returns false.
    [Func="ServiceWorkerRegistration::WebPushMethodHider"] void setScope(DOMString scope);
};

enum PushPermissionStatus
{
    "granted",
    "denied",
    "default"
};
