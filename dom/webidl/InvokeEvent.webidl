/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://github.com/whatwg/html/pull/9841
 */

[Pref="dom.element.invokers.enabled",
 Exposed=Window]
interface InvokeEvent : Event {
    constructor(DOMString type, optional InvokeEventInit eventInitDict = {});
    readonly attribute Element? invoker;
    readonly attribute DOMString action;
};

dictionary InvokeEventInit : EventInit {
    Element? invoker = null;
    DOMString action = "auto";
};
