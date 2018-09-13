/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/performance-timeline/#the-performanceobserver-interface
 */

dictionary PerformanceObserverInit {
  required sequence<DOMString> entryTypes;
  boolean buffered = false;
};

callback PerformanceObserverCallback = void (PerformanceObserverEntryList entries,
                                             PerformanceObserver observer);

[Func="mozilla::dom::DOMPrefs::dom_enable_performance_observer",
 Constructor(PerformanceObserverCallback callback),
 Exposed=(Window,Worker)]
interface PerformanceObserver {
    void observe(PerformanceObserverInit options);
    void disconnect();
    PerformanceEntryList takeRecords();
};
