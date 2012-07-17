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

interface PerformanceTiming {
  [Infallible]
  readonly attribute unsigned long long navigationStart;
  [Infallible]
  readonly attribute unsigned long long unloadEventStart;
  [Infallible]
  readonly attribute unsigned long long unloadEventEnd;
  [Infallible]
  readonly attribute unsigned long long redirectStart;
  [Infallible]
  readonly attribute unsigned long long redirectEnd;
  [Infallible]
  readonly attribute unsigned long long fetchStart;
  [Infallible]
  readonly attribute unsigned long long domainLookupStart;
  [Infallible]
  readonly attribute unsigned long long domainLookupEnd;
  [Infallible]
  readonly attribute unsigned long long connectStart;
  [Infallible]
  readonly attribute unsigned long long connectEnd;
  // secureConnectionStart will be implemneted in bug 772589
  // [Infallible]
  // readonly attribute unsigned long long secureConnectionStart;
  [Infallible]
  readonly attribute unsigned long long requestStart;
  [Infallible]
  readonly attribute unsigned long long responseStart;
  [Infallible]
  readonly attribute unsigned long long responseEnd;
  [Infallible]
  readonly attribute unsigned long long domLoading;
  [Infallible]
  readonly attribute unsigned long long domInteractive;
  [Infallible]
  readonly attribute unsigned long long domContentLoadedEventStart;
  [Infallible]
  readonly attribute unsigned long long domContentLoadedEventEnd;
  [Infallible]
  readonly attribute unsigned long long domComplete;
  [Infallible]
  readonly attribute unsigned long long loadEventStart;
  [Infallible]
  readonly attribute unsigned long long loadEventEnd;
};
