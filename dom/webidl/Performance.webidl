/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://w3c.github.io/hr-time/
 *
 * Copyright © 2015 W3C® (MIT, ERCIM, Keio, Beihang).
 * W3C liability, trademark and document use rules apply.
 */

typedef double DOMHighResTimeStamp;
typedef sequence <PerformanceEntry> PerformanceEntryList;

[Exposed=(Window,Worker)]
interface Performance {
  [DependsOn=DeviceState, Affects=Nothing]
  DOMHighResTimeStamp now();

  [Constant]
  readonly attribute DOMHighResTimeStamp timeOrigin;
};

[Exposed=Window]
partial interface Performance {
  [Constant]
  readonly attribute PerformanceTiming timing;
  [Constant]
  readonly attribute PerformanceNavigation navigation;

  jsonifier;
};

// http://www.w3.org/TR/performance-timeline/#sec-window.performance-attribute
[Exposed=(Window,Worker)]
partial interface Performance {
  [Func="Performance::IsEnabled"]
  PerformanceEntryList getEntries();
  [Func="Performance::IsEnabled"]
  PerformanceEntryList getEntriesByType(DOMString entryType);
  [Func="Performance::IsEnabled"]
  PerformanceEntryList getEntriesByName(DOMString name, optional DOMString
    entryType);
};

// http://www.w3.org/TR/resource-timing/#extensions-performance-interface
[Exposed=Window]
partial interface Performance {
  [Func="Performance::IsEnabled"]
  void clearResourceTimings();
  [Func="Performance::IsEnabled"]
  void setResourceTimingBufferSize(unsigned long maxSize);
  [Func="Performance::IsEnabled"]
  attribute EventHandler onresourcetimingbufferfull;
};

// GC microbenchmarks, pref-guarded, not for general use (bug 1125412)
[Exposed=Window]
partial interface Performance {
  [Pref="dom.enable_memory_stats"]
  readonly attribute object mozMemory;
};

// http://www.w3.org/TR/user-timing/
[Exposed=(Window,Worker)]
partial interface Performance {
  [Func="Performance::IsEnabled", Throws]
  void mark(DOMString markName);
  [Func="Performance::IsEnabled"]
  void clearMarks(optional DOMString markName);
  [Func="Performance::IsEnabled", Throws]
  void measure(DOMString measureName, optional DOMString startMark, optional DOMString endMark);
  [Func="Performance::IsEnabled"]
  void clearMeasures(optional DOMString measureName);
};

