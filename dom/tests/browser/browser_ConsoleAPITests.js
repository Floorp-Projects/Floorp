/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/dom/tests/browser/test-console-api.html";

var gWindow, gLevel, gArgs, gTestDriver;

function test() {
  waitForExplicitFinish();

  var tab = gBrowser.addTab(TEST_URI);
  gBrowser.selectedTab = tab;
  var browser = gBrowser.selectedBrowser;

  registerCleanupFunction(function () {
    gWindow = gLevel = gArgs = gTestDriver = null;
    gBrowser.removeTab(tab);
  });

  ConsoleObserver.init();

  browser.addEventListener("DOMContentLoaded", function onLoad(event) {
    browser.removeEventListener("DOMContentLoaded", onLoad, false);
    executeSoon(function test_executeSoon() {
      gWindow = browser.contentWindow;
      consoleAPISanityTest();
      gTestDriver = observeConsoleTest();
      gTestDriver.next();
    });

  }, false);
}

function testConsoleData(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  is(aMessageObject.level, gLevel, "expected level received");
  ok(aMessageObject.arguments, "we have arguments");

  if (gLevel == "trace") {
    is(aMessageObject.arguments.length, 0, "arguments.length matches");
    is(aMessageObject.stacktrace.toSource(), gArgs.toSource(),
       "stack trace is correct");
  }
  else {
    is(aMessageObject.arguments.length, gArgs.length, "arguments.length matches");
    gArgs.forEach(function (a, i) {
      // Waive Xray so that we don't get messed up by Xray ToString.
      //
      // It'd be nice to just use XPCNativeWrapper.unwrap here, but there are
      // a number of dumb reasons we can't. See bug 868675.
      var arg = aMessageObject.arguments[i];
      if (Components.utils.isXrayWrapper(arg))
        arg = arg.wrappedJSObject;
      is(arg, a, "correct arg " + i);
    });
  }

  gTestDriver.next();
}

function testLocationData(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  is(aMessageObject.level, gLevel, "expected level received");
  ok(aMessageObject.arguments, "we have arguments");

  is(aMessageObject.filename, gArgs[0].filename, "filename matches");
  is(aMessageObject.lineNumber, gArgs[0].lineNumber, "lineNumber matches");
  is(aMessageObject.functionName, gArgs[0].functionName, "functionName matches");
  is(aMessageObject.arguments.length, gArgs[0].arguments.length, "arguments.length matches");
  gArgs[0].arguments.forEach(function (a, i) {
    is(aMessageObject.arguments[i], a, "correct arg " + i);
  });

  startNativeCallbackTest();
}

function startNativeCallbackTest() {
  // Reset the observer function to cope with the fabricated test data.
  ConsoleObserver.observe = function CO_observe(aSubject, aTopic, aData) {
    try {
      testNativeCallback(aSubject.wrappedJSObject);
    } catch (ex) {
      // XXX Bug 906593 - Exceptions in this function currently aren't
      // reported, because of some XPConnect weirdness, so report them manually
      ok(false, "Exception thrown in CO_observe: " + ex);
    }
  };

  let button = gWindow.document.getElementById("test-nativeCallback");
  ok(button, "found #test-nativeCallback button");
  EventUtils.synthesizeMouseAtCenter(button, {}, gWindow);
}

function testNativeCallback(aMessageObject) {
  is(aMessageObject.level, "log", "expected level received");
  is(aMessageObject.filename, "", "filename matches");
  is(aMessageObject.lineNumber, 0, "lineNumber matches");
  is(aMessageObject.functionName, "", "functionName matches");

  startGroupTest();
}

function startGroupTest() {
  // Reset the observer function to cope with the fabricated test data.
  ConsoleObserver.observe = function CO_observe(aSubject, aTopic, aData) {
    try {
      testConsoleGroup(aSubject.wrappedJSObject);
    } catch (ex) {
      // XXX Bug 906593 - Exceptions in this function currently aren't
      // reported, because of some XPConnect weirdness, so report them manually
      ok(false, "Exception thrown in CO_observe: " + ex);
    }
  };
  let button = gWindow.document.getElementById("test-groups");
  ok(button, "found #test-groups button");
  EventUtils.synthesizeMouseAtCenter(button, {}, gWindow);
}

function testConsoleGroup(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  ok(aMessageObject.level == "group" ||
     aMessageObject.level == "groupCollapsed" ||
     aMessageObject.level == "groupEnd",
     "expected level received");

  is(aMessageObject.functionName, "testGroups", "functionName matches");
  ok(aMessageObject.lineNumber >= 45 && aMessageObject.lineNumber <= 48,
     "lineNumber matches");
  if (aMessageObject.level == "groupCollapsed") {
    is(aMessageObject.groupName, "a group", "groupCollapsed groupName matches");
    is(aMessageObject.arguments[0], "a", "groupCollapsed arguments[0] matches");
    is(aMessageObject.arguments[1], "group", "groupCollapsed arguments[0] matches");
  }
  else if (aMessageObject.level == "group") {
    is(aMessageObject.groupName, "b group", "group groupName matches");
    is(aMessageObject.arguments[0], "b", "group arguments[0] matches");
    is(aMessageObject.arguments[1], "group", "group arguments[1] matches");
  }
  else if (aMessageObject.level == "groupEnd") {
    let groupName = Array.prototype.join.call(aMessageObject.arguments, " ");
    is(groupName,"b group", "groupEnd arguments matches");
    is(aMessageObject.groupName, "b group", "groupEnd groupName matches");
  }

  if (aMessageObject.level == "groupEnd") {
    startTimeTest();
  }
}

function startTraceTest() {
  gLevel = "trace";
  gArgs = [
    {filename: TEST_URI, lineNumber: 6, functionName: "window.foobar585956c", language: 2},
    {filename: TEST_URI, lineNumber: 11, functionName: "foobar585956b", language: 2},
    {filename: TEST_URI, lineNumber: 15, functionName: "foobar585956a", language: 2},
    {filename: TEST_URI, lineNumber: 1, functionName: "onclick", language: 2}
  ];

  let button = gWindow.document.getElementById("test-trace");
  ok(button, "found #test-trace button");
  EventUtils.synthesizeMouseAtCenter(button, {}, gWindow);
}

function startLocationTest() {
  // Reset the observer function to cope with the fabricated test data.
  ConsoleObserver.observe = function CO_observe(aSubject, aTopic, aData) {
    try {
      testLocationData(aSubject.wrappedJSObject);
    } catch (ex) {
      // XXX Bug 906593 - Exceptions in this function currently aren't
      // reported, because of some XPConnect weirdness, so report them manually
      ok(false, "Exception thrown in CO_observe: " + ex);
    }
  };
  gLevel = "log";
  gArgs = [
    {filename: TEST_URI, lineNumber: 19, functionName: "foobar646025", arguments: ["omg", "o", "d"]}
  ];

  let button = gWindow.document.getElementById("test-location");
  ok(button, "found #test-location button");
  EventUtils.synthesizeMouseAtCenter(button, {}, gWindow);
}

function expect(level) {
  gLevel = level;
  gArgs = Array.slice(arguments, 1);
}

function observeConsoleTest() {
  let win = XPCNativeWrapper.unwrap(gWindow);
  expect("log", "arg");
  win.console.log("arg");
  yield undefined;

  expect("info", "arg", "extra arg");
  win.console.info("arg", "extra arg");
  yield undefined;

  // We don't currently support width and precision qualifiers, but we don't
  // choke on them either.
  expect("warn", "Lesson 1: PI is approximately equal to 3.14159");
  win.console.warn("Lesson %d: %s is approximately equal to %1.2f",
                   1,
                   "PI",
                   3.14159);
  yield undefined;

  expect("log", "%d, %s, %l");
  win.console.log("%d, %s, %l");
  yield undefined;

  expect("log", "%a %b %c");
  win.console.log("%a %b %c");
  yield undefined;

  expect("log", "%a %b %c", "a", "b");
  win.console.log("%a %b %c", "a", "b");
  yield undefined;

  expect("log", "2, a, %l", 3);
  win.console.log("%d, %s, %l", 2, "a", 3);
  yield undefined;

  // Bug #692550 handle null and undefined.
  expect("log", "null, undefined");
  win.console.log("%s, %s", null, undefined);
  yield undefined;

  // Bug #696288 handle object as first argument.
  let obj = { a: 1 };
  expect("log", obj, "a");
  win.console.log(obj, "a");
  yield undefined;

  expect("dir", win.toString());
  win.console.dir(win);
  yield undefined;

  expect("error", "arg");
  win.console.error("arg");
  yield undefined;

  expect("exception", "arg");
  win.console.exception("arg");
  yield undefined;

  let obj2 = { b: 2 };
  expect("log", "omg ", obj, " foo ", 4, obj2);
  win.console.log("omg %o foo %o", obj, 4, obj2);
  yield undefined;

  startTraceTest();
  yield undefined;

  startLocationTest();
  yield undefined;
}

function consoleAPISanityTest() {
  let win = XPCNativeWrapper.unwrap(gWindow);
  ok(win.console, "we have a console attached");
  ok(win.console, "we have a console attached, 2nd attempt");

  ok(win.console.log, "console.log is here");
  ok(win.console.info, "console.info is here");
  ok(win.console.warn, "console.warn is here");
  ok(win.console.error, "console.error is here");
  ok(win.console.exception, "console.exception is here");
  ok(win.console.trace, "console.trace is here");
  ok(win.console.dir, "console.dir is here");
  ok(win.console.group, "console.group is here");
  ok(win.console.groupCollapsed, "console.groupCollapsed is here");
  ok(win.console.groupEnd, "console.groupEnd is here");
  ok(win.console.time, "console.time is here");
  ok(win.console.timeEnd, "console.timeEnd is here");
}

function startTimeTest() {
  // Reset the observer function to cope with the fabricated test data.
  ConsoleObserver.observe = function CO_observe(aSubject, aTopic, aData) {
    try {
      testConsoleTime(aSubject.wrappedJSObject);
    } catch (ex) {
      // XXX Bug 906593 - Exceptions in this function currently aren't
      // reported, because of some XPConnect weirdness, so report them manually
      ok(false, "Exception thrown in CO_observe: " + ex);
    }
  };
  gLevel = "time";
  gArgs = [
    {filename: TEST_URI, lineNumber: 23, functionName: "startTimer",
     arguments: ["foo"],
     timer: { name: "foo" },
    }
  ];

  let button = gWindow.document.getElementById("test-time");
  ok(button, "found #test-time button");
  EventUtils.synthesizeMouseAtCenter(button, {}, gWindow);
}

function testConsoleTime(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  is(aMessageObject.level, gLevel, "expected level received");

  is(aMessageObject.filename, gArgs[0].filename, "filename matches");
  is(aMessageObject.lineNumber, gArgs[0].lineNumber, "lineNumber matches");
  is(aMessageObject.functionName, gArgs[0].functionName, "functionName matches");
  is(aMessageObject.timer.name, gArgs[0].timer.name, "timer.name matches");
  ok(aMessageObject.timer.started, "timer.started exists");

  gArgs[0].arguments.forEach(function (a, i) {
    is(aMessageObject.arguments[i], a, "correct arg " + i);
  });

  startTimeEndTest();
}

function startTimeEndTest() {
  // Reset the observer function to cope with the fabricated test data.
  ConsoleObserver.observe = function CO_observe(aSubject, aTopic, aData) {
    try {
      testConsoleTimeEnd(aSubject.wrappedJSObject);
    } catch (ex) {
      // XXX Bug 906593 - Exceptions in this function currently aren't
      // reported, because of some XPConnect weirdness, so report them manually
      ok(false, "Exception thrown in CO_observe: " + ex);
    }
  };
  gLevel = "timeEnd";
  gArgs = [
    {filename: TEST_URI, lineNumber: 27, functionName: "stopTimer",
     arguments: ["foo"],
     timer: { name: "foo" },
    },
  ];

  let button = gWindow.document.getElementById("test-timeEnd");
  ok(button, "found #test-timeEnd button");
  EventUtils.synthesizeMouseAtCenter(button, {}, gWindow);
}

function testConsoleTimeEnd(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  is(aMessageObject.level, gLevel, "expected level received");
  ok(aMessageObject.arguments, "we have arguments");

  is(aMessageObject.filename, gArgs[0].filename, "filename matches");
  is(aMessageObject.lineNumber, gArgs[0].lineNumber, "lineNumber matches");
  is(aMessageObject.functionName, gArgs[0].functionName, "functionName matches");
  is(aMessageObject.arguments.length, gArgs[0].arguments.length, "arguments.length matches");
  is(aMessageObject.timer.name, gArgs[0].timer.name, "timer name matches");
  is(typeof aMessageObject.timer.duration, "number", "timer duration is a number");
  info("timer duration: " + aMessageObject.timer.duration);
  ok(aMessageObject.timer.duration >= 0, "timer duration is positive");

  gArgs[0].arguments.forEach(function (a, i) {
    is(aMessageObject.arguments[i], a, "correct arg " + i);
  });

  startEmptyTimerTest();
}

function startEmptyTimerTest() {
  // Reset the observer function to cope with the fabricated test data.
  ConsoleObserver.observe = function CO_observe(aSubject, aTopic, aData) {
    try {
      testEmptyTimer(aSubject.wrappedJSObject);
    } catch (ex) {
      // XXX Bug 906593 - Exceptions in this function currently aren't
      // reported, because of some XPConnect weirdness, so report them manually
      ok(false, "Exception thrown in CO_observe: " + ex);
    }
  };

  let button = gWindow.document.getElementById("test-namelessTimer");
  ok(button, "found #test-namelessTimer button");
  EventUtils.synthesizeMouseAtCenter(button, {}, gWindow);
}

function testEmptyTimer(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  ok(aMessageObject.level == "time" || aMessageObject.level == "timeEnd",
     "expected level received");
  is(aMessageObject.arguments.length, 0, "we don't have arguments");
  ok(!aMessageObject.timer, "we don't have a timer");

  is(aMessageObject.functionName, "namelessTimer", "functionName matches");
  ok(aMessageObject.lineNumber == 31 || aMessageObject.lineNumber == 32,
     "lineNumber matches");
  // Test finished
  ConsoleObserver.destroy();
  finish();
}

var ConsoleObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  init: function CO_init() {
    Services.obs.addObserver(this, "console-api-log-event", false);
  },

  destroy: function CO_destroy() {
    Services.obs.removeObserver(this, "console-api-log-event");
  },

  observe: function CO_observe(aSubject, aTopic, aData) {
    try {
      testConsoleData(aSubject.wrappedJSObject);
    } catch (ex) {
      // XXX Bug 906593 - Exceptions in this function currently aren't
      // reported, because of some XPConnect weirdness, so report them manually
      ok(false, "Exception thrown in CO_observe: " + ex);
    }
  }
};

function getWindowId(aWindow)
{
  return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIDOMWindowUtils)
                .outerWindowID;
}
