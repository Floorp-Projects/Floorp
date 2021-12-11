/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/IntersectionObserver/
 */

[ProbablyShortLivingWrapper, Pref="dom.IntersectionObserver.enabled",
 Exposed=Window]
interface IntersectionObserverEntry {
  [Constant]
  readonly attribute DOMHighResTimeStamp time;
  [Constant]
  readonly attribute DOMRectReadOnly? rootBounds;
  [Constant]
  readonly attribute DOMRectReadOnly boundingClientRect;
  [Constant]
  readonly attribute DOMRectReadOnly intersectionRect;
  [Constant]
  readonly attribute boolean isIntersecting;
  [Constant]
  readonly attribute double intersectionRatio;
  [Constant]
  readonly attribute Element target;
};

[Pref="dom.IntersectionObserver.enabled",
 Exposed=Window]
interface IntersectionObserver {
  [Throws]
  constructor(IntersectionCallback intersectionCallback,
              optional IntersectionObserverInit options = {});

  [Constant]
  readonly attribute Node? root;
  [Constant]
  readonly attribute UTF8String rootMargin;
  [Constant,Cached]
  readonly attribute sequence<double> thresholds;
  void observe(Element target);
  void unobserve(Element target);
  void disconnect();
  sequence<IntersectionObserverEntry> takeRecords();
};

callback IntersectionCallback =
  void (sequence<IntersectionObserverEntry> entries, IntersectionObserver observer);

dictionary IntersectionObserverEntryInit {
  required DOMHighResTimeStamp time;
  required DOMRectInit rootBounds;
  required DOMRectInit boundingClientRect;
  required DOMRectInit intersectionRect;
  required Element target;
};

dictionary IntersectionObserverInit {
  (Element or Document)? root = null;
  UTF8String rootMargin = "0px";
  (double or sequence<double>) threshold = 0;
};
