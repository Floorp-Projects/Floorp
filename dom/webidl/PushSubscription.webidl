/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* The origin of this IDL file is
* https://w3c.github.io/push-api/
*/

interface Principal;

enum PushEncryptionKeyName
{
  "p256dh",
  "auth"
};

[Exposed=(Window,Worker), Func="nsContentUtils::PushEnabled",
 ChromeConstructor(DOMString pushEndpoint, DOMString scope,
                   ArrayBuffer? key, ArrayBuffer? authSecret)]
interface PushSubscription
{
    readonly attribute USVString endpoint;
    ArrayBuffer? getKey(PushEncryptionKeyName name);
    [Throws, UseCounter]
    Promise<boolean> unsubscribe();
    jsonifier;

    // Used to set the principal from the JS implemented PushManager.
    [Exposed=Window,ChromeOnly]
    void setPrincipal(Principal principal);
};
