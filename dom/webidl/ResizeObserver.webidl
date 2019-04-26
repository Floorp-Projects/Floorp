/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/resize-observer/
 */

[Constructor(ResizeObserverCallback callback),
 Exposed=Window,
 Pref="layout.css.resizeobserver.enabled"]
interface ResizeObserver {
    // Bug 1545239: Add "optional ResizeObserverOptions" in observe.
    [Throws]
    void observe(Element target);
    [Throws]
    void unobserve(Element target);
    void disconnect();
};

callback ResizeObserverCallback = void (sequence<ResizeObserverEntry> entries, ResizeObserver observer);

[Constructor(Element target),
 ChromeOnly,
 Pref="layout.css.resizeobserver.enabled"]
interface ResizeObserverEntry {
    readonly attribute Element target;
    readonly attribute DOMRectReadOnly contentRect;
    // Bug 1545239: Add borderBoxSize and contentBoxSize attributes.
};
