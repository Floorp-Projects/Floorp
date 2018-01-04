/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * https://console.spec.whatwg.org/#console-namespace
 */

[Exposed=(Window,Worker,WorkerDebugger,Worklet,System),
 ClassString="Console",
 ProtoObjectHack]
namespace console {

  // NOTE: if you touch this namespace, remember to update the ConsoleInstance
  // interface as well!

  // Logging
  void assert(optional boolean condition = false, any... data);
  void clear();
  void count(optional DOMString label = "default");
  void debug(any... data);
  void error(any... data);
  void info(any... data);
  void log(any... data);
  void table(any... data); // FIXME: The spec is still unclear about this.
  void trace(any... data);
  void warn(any... data);
  void dir(any... data); // FIXME: This doesn't follow the spec yet.
  void dirxml(any... data);

  // Grouping
  void group(any... data);
  void groupCollapsed(any... data);
  void groupEnd();

  // Timing
  void time(optional DOMString label = "default");
  void timeEnd(optional DOMString label = "default");

  // Mozilla only or Webcompat methods

  void _exception(any... data);
  void timeStamp(optional any data);

  void profile(any... data);
  void profileEnd(any... data);

  [ChromeOnly]
  const boolean IS_NATIVE_CONSOLE = true;

  [ChromeOnly, NewObject]
  ConsoleInstance createInstance(optional ConsoleInstanceOptions options);
};

// This is used to propagate console events to the observers.
dictionary ConsoleEvent {
  (unsigned long long or DOMString) ID;
  (unsigned long long or DOMString) innerID;
  DOMString consoleID = "";
  DOMString addonId = "";
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
  DOMString? asyncCause;
};

dictionary ConsoleTimerStart {
  DOMString name = "";
};

dictionary ConsoleTimerEnd {
  DOMString name = "";
  double duration = 0;
};

dictionary ConsoleTimerError {
  DOMString error = "";
  DOMString name = "";
};

dictionary ConsoleCounter {
  DOMString label = "";
  unsigned long count = 0;
};

dictionary ConsoleCounterError {
  DOMString error = "maxCountersExceeded";
};

[ChromeOnly,
 Exposed=(Window,Worker,WorkerDebugger,Worklet,System)]
// This is basically a copy of the console namespace.
interface ConsoleInstance {
  // Logging
  void assert(optional boolean condition = false, any... data);
  void clear();
  void count(optional DOMString label = "default");
  void debug(any... data);
  void error(any... data);
  void info(any... data);
  void log(any... data);
  void table(any... data); // FIXME: The spec is still unclear about this.
  void trace(any... data);
  void warn(any... data);
  void dir(any... data); // FIXME: This doesn't follow the spec yet.
  void dirxml(any... data);

  // Grouping
  void group(any... data);
  void groupCollapsed(any... data);
  void groupEnd();

  // Timing
  void time(optional DOMString label = "default");
  void timeEnd(optional DOMString label = "default");

  // Mozilla only or Webcompat methods

  void _exception(any... data);
  void timeStamp(optional any data);

  void profile(any... data);
  void profileEnd(any... data);
};

dictionary ConsoleInstanceOptions {
/* TODO:
  boolean dump = false;
  DOMString prefix = "";
  DOMString maxLogLevel = "";
  DOMString innerID = "";
*/
  DOMString consoleID = "";
};
