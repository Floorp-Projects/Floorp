/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/navigation-timing/#the-performancenavigation-interface
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Pref="dom.enable_event_timing",
 Exposed=Window]
interface EventCounts {
    readonly maplike<DOMString, unsigned long long>;
};

[Pref="dom.enable_event_timing",
 Exposed=Window]
interface PerformanceEventTiming : PerformanceEntry {
    readonly attribute DOMHighResTimeStamp processingStart;
    readonly attribute DOMHighResTimeStamp processingEnd;
    readonly attribute boolean cancelable;
    readonly attribute Node? target;

    [Default] object toJSON();
};
