/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://w3c-test.org/webperf/specs/ResourceTiming/#performanceresourcetiming
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface PerformanceResourceTiming : PerformanceEntry
{
  // A string with the name of that element that initiated the load.
  // If the initiator is a CSS resource, the initiatorType attribute must return
  // the string "css".
  // If the initiator is an XMLHttpRequest object, the initiatorType attribute
  // must return the string "xmlhttprequest".
  readonly attribute DOMString initiatorType;
  readonly attribute DOMString nextHopProtocol;

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
