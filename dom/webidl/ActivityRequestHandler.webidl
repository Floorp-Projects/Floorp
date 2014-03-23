/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[Pref="dom.sysmsg.enabled",
 JSImplementation="@mozilla.org/dom/activities/request-handler;1",
 ChromeConstructor(DOMString id, optional ActivityOptions options),
 ChromeOnly]
interface ActivityRequestHandler
{
    void postResult(any result);
    void postError(DOMString error);
    [Pure, Cached, Frozen]
    readonly attribute ActivityOptions source;
};
