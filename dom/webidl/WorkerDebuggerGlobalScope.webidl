/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[Global=(WorkerDebugger), Exposed=WorkerDebugger]
interface WorkerDebuggerGlobalScope : EventTarget {
  readonly attribute object global;

  void enterEventLoop();

  void leaveEventLoop();

  void postMessage(DOMString message);

  attribute EventHandler onmessage;

  void reportError(DOMString message);
};

// So you can debug while you debug
partial interface WorkerDebuggerGlobalScope {
  void dump(optional DOMString string);
};
