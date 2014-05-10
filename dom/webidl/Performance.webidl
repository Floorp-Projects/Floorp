/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/hr-time/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

typedef double DOMHighResTimeStamp;
typedef sequence <PerformanceEntry> PerformanceEntryList;

interface Performance {
  DOMHighResTimeStamp now();

  [Constant]
  readonly attribute PerformanceTiming timing;
  [Constant]
  readonly attribute PerformanceNavigation navigation;

  jsonifier;
};

// http://www.w3.org/TR/performance-timeline/#sec-window.performance-attribute
partial interface Performance {
  [Pref="dom.enable_resource_timing"]
  PerformanceEntryList getEntries();
  [Pref="dom.enable_resource_timing"]
  PerformanceEntryList getEntriesByType(DOMString entryType);
  [Pref="dom.enable_resource_timing"]
  PerformanceEntryList getEntriesByName(DOMString name, optional DOMString
    entryType);
};

// http://www.w3.org/TR/resource-timing/#extensions-performance-interface
partial interface Performance {
  [Pref="dom.enable_resource_timing"]
  void clearResourceTimings();
  [Pref="dom.enable_resource_timing"]
  void setResourceTimingBufferSize(unsigned long maxSize);
  [Pref="dom.enable_resource_timing"]
  attribute EventHandler onresourcetimingbufferfull;
};
