/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/workers.html#the-workerglobalscope-common-interface
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and Opera
 * Software ASA.
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 */

[Exposed=Worker]
interface WorkerGlobalScope : EventTarget {
  readonly attribute WorkerGlobalScope self;

  [Replaceable]
  readonly attribute Console console;

  readonly attribute WorkerLocation location;

  void close();
  attribute OnErrorEventHandler onerror;

  attribute EventHandler onoffline;
  attribute EventHandler ononline;
  // also has additional members in a partial interface
};

partial interface WorkerGlobalScope {
  [Throws]
  void importScripts(DOMString... urls);

  readonly attribute WorkerNavigator navigator;
};

WorkerGlobalScope implements WindowTimers;
WorkerGlobalScope implements WindowBase64;
WorkerGlobalScope implements GlobalFetch;

// Not implemented yet: bug 1072107.
// WorkerGlobalScope implements FontFaceSource;

// Mozilla extensions
partial interface WorkerGlobalScope {
  attribute EventHandler onclose;

  void dump(optional DOMString str);

  // XXXbz no spec for this yet, because the webperf WG is a bit dysfunctional
  readonly attribute Performance performance;
};
