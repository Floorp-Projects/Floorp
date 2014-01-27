/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface WorkerConsole {
  void log(any... data);
  void info(any... data);
  void warn(any... data);
  void error(any... data);
  void _exception(any... data);
  void debug(any... data);
  void trace();
  void dir(optional any data);
  void group(any... data);
  void groupCollapsed(any... data);
  void groupEnd(any... data);
  void time(optional any time);
  void timeEnd(optional any time);
  void profile(any... data);
  void profileEnd(any... data);
  void assert(boolean condition, any... data);
  void ___noSuchMethod__();
};

// This dictionary is used internally to send the stack trace from the worker to
// the main thread Console API implementation.
dictionary WorkerConsoleStack {
  DOMString filename = "";
  unsigned long lineNumber = 0;
  DOMString functionName = "";
  unsigned long language = 0;
};

