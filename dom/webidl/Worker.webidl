/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://html.spec.whatwg.org/multipage/workers.html
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and Opera
 * Software ASA.
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 */

[Exposed=(Window,DedicatedWorker,SharedWorker)]
interface Worker : EventTarget {
  [Throws]
  constructor(USVString scriptURL, optional WorkerOptions options = {});

  void terminate();

  [Throws]
  void postMessage(any message, sequence<object> transfer);
  [Throws]
  void postMessage(any message, optional StructuredSerializeOptions aOptions = {});

  attribute EventHandler onmessage;
  attribute EventHandler onmessageerror;
};

Worker includes AbstractWorker;

dictionary WorkerOptions {
  // WorkerType type = "classic"; TODO: Bug 1247687
  // RequestCredentials credentials = "omit"; // credentials is only used if type is "module" TODO: Bug 1247687
  DOMString name = "";
};

[Func="mozilla::dom::ChromeWorker::WorkerAvailable",
 Exposed=(Window,DedicatedWorker,SharedWorker)]
interface ChromeWorker : Worker {
  [Throws]
  constructor(USVString scriptURL);
};
