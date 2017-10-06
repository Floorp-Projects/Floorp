/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/resource-timing/#performanceresourcetiming
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface PerformanceResourceTiming : PerformanceEntry
{
  readonly attribute DOMString initiatorType;
  readonly attribute DOMString nextHopProtocol;

  readonly attribute DOMHighResTimeStamp workerStart;
  readonly attribute DOMHighResTimeStamp redirectStart;
  readonly attribute DOMHighResTimeStamp redirectEnd;
  readonly attribute DOMHighResTimeStamp fetchStart;
  readonly attribute DOMHighResTimeStamp domainLookupStart;
  readonly attribute DOMHighResTimeStamp domainLookupEnd;
  readonly attribute DOMHighResTimeStamp connectStart;
  readonly attribute DOMHighResTimeStamp connectEnd;
  readonly attribute DOMHighResTimeStamp secureConnectionStart;
  readonly attribute DOMHighResTimeStamp requestStart;
  readonly attribute DOMHighResTimeStamp responseStart;
  readonly attribute DOMHighResTimeStamp responseEnd;

  readonly attribute unsigned long long transferSize;
  readonly attribute unsigned long long encodedBodySize;
  readonly attribute unsigned long long decodedBodySize;

  jsonifier;
};
