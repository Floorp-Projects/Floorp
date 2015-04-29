/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* The origin of this IDL file is
* https://w3c.github.io/push-api/
*/

[JSImplementation="@mozilla.org/push/PushSubscription;1",
 Constructor(DOMString pushEndpoint, DOMString scope, DOMString pageURL), ChromeOnly]
interface PushSubscription
{
    readonly attribute USVString endpoint;
    Promise<boolean> unsubscribe();
    jsonifier;
};
