/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[Global=(WorkerDebugger), Exposed=WorkerDebugger]
interface WorkerDebuggerGlobalScope : EventTarget {
  [Throws]
  readonly attribute object global;

  [Throws]
  object createSandbox(DOMString name, object prototype);

  [Throws]
  void loadSubScript(DOMString url, optional object sandbox);

  void enterEventLoop();

  void leaveEventLoop();

  void postMessage(DOMString message);

  attribute EventHandler onmessage;

  attribute EventHandler onmessageerror;

  [Throws]
  void setImmediate(Function handler);

  void reportError(DOMString message);

  [Throws]
  sequence<any> retrieveConsoleEvents();

  [Throws]
  void setConsoleEventHandler(AnyCallback? handler);

  // base64 utility methods
  [Throws]
  DOMString btoa(DOMString btoa);
  [Throws]
  DOMString atob(DOMString atob);
};

// So you can debug while you debug
partial interface WorkerDebuggerGlobalScope {
  void dump(optional DOMString string);
};
