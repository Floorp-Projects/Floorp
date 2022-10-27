/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/performance-timeline/#the-performanceobserver-interface
 */

dictionary PerformanceObserverInit {
  sequence<DOMString> entryTypes;
  DOMString type;
  boolean buffered;
  [Pref="dom.enable_event_timing"]
  DOMHighResTimeStamp durationThreshold;
};

callback PerformanceObserverCallback = undefined (PerformanceObserverEntryList entries,
                                                  PerformanceObserver observer);

[Pref="dom.enable_performance_observer",
 Exposed=(Window,Worker)]
interface PerformanceObserver {
    [Throws]
    constructor(PerformanceObserverCallback callback);

    [Throws] undefined observe(optional PerformanceObserverInit options = {});
    undefined disconnect();
    PerformanceEntryList takeRecords();
    static readonly attribute object supportedEntryTypes;
};
