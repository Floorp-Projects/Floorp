/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Exposed=(Window,Worker,WorkerDebugger),
 ClassString="Console"]
namespace console {
  void log(any... data);
  void info(any... data);
  void warn(any... data);
  void error(any... data);
  void _exception(any... data);
  void debug(any... data);
  void table(any... data);
  void trace();
  void dir(any... data);
  void dirxml(any... data);
  void group(any... data);
  void groupCollapsed(any... data);
  void groupEnd(any... data);
  void time(optional any time);
  void timeEnd(optional any time);
  void timeStamp(optional any data);
  void clear(any... data);

  void profile(any... data);
  void profileEnd(any... data);

  void assert(boolean condition, any... data);
  void count(any... data);

  // No-op methods for compatibility with other browsers.
  [BinaryName="noopMethod"]
  void markTimeline();
  [BinaryName="noopMethod"]
  void timeline();
  [BinaryName="noopMethod"]
  void timelineEnd();

  [ChromeOnly]
  const boolean IS_NATIVE_CONSOLE = true;
};

// This is used to propagate console events to the observers.
dictionary ConsoleEvent {
  (unsigned long long or DOMString) ID;
  (unsigned long long or DOMString) innerID;
  any originAttributes = null;
  DOMString level = "";
  DOMString filename = "";
  unsigned long lineNumber = 0;
  unsigned long columnNumber = 0;
  DOMString functionName = "";
  double timeStamp = 0;
  sequence<any> arguments;
  sequence<DOMString?> styles;
  boolean private = false;
  // stacktrace is handled via a getter in some cases so we can construct it
  // lazily.  Note that we're not making this whole thing an interface because
  // consumers expect to see own properties on it, which would mean making the
  // props unforgeable, which means lots of JSFunction allocations.  Maybe we
  // should fix those consumers, of course....
  // sequence<ConsoleStackEntry> stacktrace;
  DOMString groupName = "";
  any timer = null;
  any counter = null;
};

// Event for profile operations
dictionary ConsoleProfileEvent {
  DOMString action = "";
  sequence<any> arguments;
};

// This dictionary is used to manage stack trace data.
dictionary ConsoleStackEntry {
  DOMString filename = "";
  unsigned long lineNumber = 0;
  unsigned long columnNumber = 0;
  DOMString functionName = "";
  unsigned long language = 0;
  DOMString? asyncCause;
};

dictionary ConsoleTimerStart {
  DOMString name = "";
  double started = 0;
};

dictionary ConsoleTimerEnd {
  DOMString name = "";
  double duration = 0;
};

dictionary ConsoleTimerError {
  DOMString error = "maxTimersExceeded";
};

dictionary ConsoleCounter {
  DOMString label = "";
  unsigned long count = 0;
};

dictionary ConsoleCounterError {
  DOMString error = "maxCountersExceeded";
};
