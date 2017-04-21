/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/dom/tests/browser/test-console-api.html";

add_task(function*() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URI);
  registerCleanupFunction(() => gBrowser.removeTab(tab));
  let browser = gBrowser.selectedBrowser;

  yield* consoleAPISanityTest(browser);
  yield* observeConsoleTest(browser);
  yield* startTraceTest(browser);
  yield* startLocationTest(browser);
  yield* startNativeCallbackTest(browser);
  yield* startGroupTest(browser);
  yield* startTimeTest(browser);
  yield* startTimeEndTest(browser);
  yield* startTimeStampTest(browser);
  yield* startEmptyTimeStampTest(browser);
  yield* startEmptyTimerTest(browser);
});

function spawnWithObserver(browser, observerFunc, func) {
  // Build the observer generating function.
  let source = [
    "const TEST_URI = 'http://example.com/browser/dom/tests/browser/test-console-api.html';",
    // Create a promise to be resolved when the text is complete. It is stored
    // on content, such that it can be waited on by calling waitForResolve. This
    // is done rather than returning it from this function such that the
    // initialization can be yeilded on before yeilding on the conclusion of the
    // test.
    "content._promise = new Promise(_resolve => {",
    // These are variables which are used by the test runner to communicate
    // state to the observer.
    "  let gLevel, gArgs, gStyle;",
    "  let expect = function(level) {",
    "    gLevel = level;",
    "    gArgs = Array.slice(arguments, 1);",
    "  }",
    // To ease the transition to the new format, content.window is avaliable as gWindow
    // in the content.
    "  let gWindow = content.window;",
    // This method is called rather than _resolve such that the observer is removed
    // before exiting the test
    "  let resolve = () => {",
    "    Services.obs.removeObserver(ConsoleObserver, 'console-api-log-event');",
    "    _resolve();",
    "  };",
    // This is the observer itself, it calls the passed-in function whenever
    // it encounters an event
    "  let ConsoleObserver = {",
    "    QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),",
    "    observe: function(aSubject, aTopic, aData) {",
    "      try {",
    "        (" + observerFunc.toString() + ")(aSubject.wrappedJSObject);",
    "      } catch (ex) {",
    "        ok(false, 'Exception thrown in observe: ' + ex);",
    "      }",
    "    }",
    "  };",
    "  Services.obs.addObserver(ConsoleObserver, 'console-api-log-event', false);",
    // Call the initialization function (if present)
    func ? ("(" + func.toString() + ")();") : "",
    "});",
  ].join('\n');

  return ContentTask.spawn(browser, null, new Function(source));
}

function waitForResolve(browser) {
  return ContentTask.spawn(browser, null, function() {
    return content._promise;
  });
}

function* consoleAPISanityTest(browser) {
  yield ContentTask.spawn(browser, null, function() {
    let win = XPCNativeWrapper.unwrap(content.window);

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
    ok(win.console.timeStamp, "console.timeStamp is here");
    ok(win.console.assert, "console.assert is here");
    ok(win.console.count, "console.count is here");
  });
}

function testConsoleData(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  is(aMessageObject.level, gLevel, "expected level received");
  ok(aMessageObject.arguments, "we have arguments");

  switch (gLevel) {
  case "trace": {
    is(aMessageObject.arguments.length, 0, "arguments.length matches");
    is(aMessageObject.stacktrace.toSource(), gArgs.toSource(),
       "stack trace is correct");
    break;
  }
  case "count": {
    is(aMessageObject.counter.label, gArgs[0].label, "label matches");
    is(aMessageObject.counter.count, gArgs[0].count, "count matches");
    break;
  }
  default: {
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

    if (gStyle) {
      is(aMessageObject.styles.length, gStyle.length, "styles.length matches");
      is(aMessageObject.styles + "", gStyle + "", "styles match");
    } else {
      ok(!aMessageObject.styles || aMessageObject.styles.length === 0,
         "styles match");
    }
  }
  }
}

function* observeConsoleTest(browser) {
  yield spawnWithObserver(browser, testConsoleData, function(opts) {
    let win = XPCNativeWrapper.unwrap(content.window);
    expect("log", "arg");
    win.console.log("arg");

    expect("info", "arg", "extra arg");
    win.console.info("arg", "extra arg");

    expect("warn", "Lesson 1: PI is approximately equal to 3");
    win.console.warn("Lesson %d: %s is approximately equal to %1.0f",
                     1,
                     "PI",
                     3.14159);

    expect("warn", "Lesson 1: PI is approximately equal to 3.14");
    win.console.warn("Lesson %d: %s is approximately equal to %1.2f",
                     1,
                     "PI",
                     3.14159);

    expect("warn", "Lesson 1: PI is approximately equal to 3.141590");
    win.console.warn("Lesson %d: %s is approximately equal to %f",
                     1,
                     "PI",
                     3.14159);

    expect("warn", "Lesson 1: PI is approximately equal to 3.1415900");
    win.console.warn("Lesson %d: %s is approximately equal to %0.7f",
                     1,
                     "PI",
                     3.14159);

    expect("log", "%d, %s, %l");
    win.console.log("%d, %s, %l");

    expect("log", "%a %b %g");
    win.console.log("%a %b %g");

    expect("log", "%a %b %g", "a", "b");
    win.console.log("%a %b %g", "a", "b");

    expect("log", "2, a, %l", 3);
    win.console.log("%d, %s, %l", 2, "a", 3);

    // Bug #692550 handle null and undefined.
    expect("log", "null, undefined");
    win.console.log("%s, %s", null, undefined);

    // Bug #696288 handle object as first argument.
    let obj = { a: 1 };
    expect("log", obj, "a");
    win.console.log(obj, "a");

    expect("dir", win.toString());
    win.console.dir(win);

    expect("error", "arg");
    win.console.error("arg");

    expect("exception", "arg");
    win.console.exception("arg");

    expect("log", "foobar");
    gStyle = ["color:red;foobar;;"];
    win.console.log("%cfoobar", gStyle[0]);

    let obj4 = { d: 4 };
    expect("warn", "foobar", obj4, "test", "bazbazstr", "last");
    gStyle = [null, null, null, "color:blue;", "color:red"];
    win.console.warn("foobar%Otest%cbazbaz%s%clast", obj4, gStyle[3], "str", gStyle[4]);

    let obj3 = { c: 3 };
    expect("info", "foobar", "bazbaz", obj3, "%comg", "color:yellow");
    gStyle = [null, "color:pink;"];
    win.console.info("foobar%cbazbaz", gStyle[1], obj3, "%comg", "color:yellow");

    gStyle = null;
    let obj2 = { b: 2 };
    expect("log", "omg ", obj, " foo ", 4, obj2);
    win.console.log("omg %o foo %o", obj, 4, obj2);

    expect("assert", "message");
    win.console.assert(false, "message");

    expect("count", { label: "label a", count: 1 });
    win.console.count("label a");

    expect("count", { label: "label b", count: 1 });
    win.console.count("label b");

    expect("count", { label: "label a", count: 2 });
    win.console.count("label a");

    expect("count", { label: "label b", count: 2 });
    win.console.count("label b");
    dump("Resolving\n");
    resolve();
  });
  dump("There\n");
}

function testTraceConsoleData(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  is(aMessageObject.level, gLevel, "expected level received");
  ok(aMessageObject.arguments, "we have arguments");

  is(gLevel, "trace", "gLevel should be trace");
  is(aMessageObject.arguments.length, 0, "arguments.length matches");
  dump(aMessageObject.stacktrace.toSource() + "\n" + gArgs.toSource() + "\n");
  is(aMessageObject.stacktrace.toSource(), gArgs.toSource(),
     "stack trace is correct");
  resolve();
}

function* startTraceTest(browser) {
  dump("HERE\n");
  yield spawnWithObserver(browser, testTraceConsoleData, function(opts) {
    dump("Observer attached\n");
    gLevel = "trace";
    gArgs = [
      {columnNumber: 9, filename: TEST_URI, functionName: "window.foobar585956c", language: 2, lineNumber: 6},
      {columnNumber: 16, filename: TEST_URI, functionName: "foobar585956b", language: 2, lineNumber: 11},
      {columnNumber: 16, filename: TEST_URI, functionName: "foobar585956a", language: 2, lineNumber: 15},
      {columnNumber: 1, filename: TEST_URI, functionName: "onclick", language: 2, lineNumber: 1}
    ];

  });

  BrowserTestUtils.synthesizeMouseAtCenter("#test-trace", {}, browser);
  yield waitForResolve(browser);
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

  resolve();
}

function* startLocationTest(browser) {
  yield spawnWithObserver(browser, testLocationData, function(opts) {
    gLevel = "log";
    gArgs = [
      {filename: TEST_URI, functionName: "foobar646025", arguments: ["omg", "o", "d"], lineNumber: 19}
    ];
  });

  BrowserTestUtils.synthesizeMouseAtCenter("#test-location", {}, browser);
  yield waitForResolve(browser);
}

function testNativeCallback(aMessageObject) {
  is(aMessageObject.level, "log", "expected level received");
  is(aMessageObject.filename, "", "filename matches");
  is(aMessageObject.lineNumber, 0, "lineNumber matches");
  is(aMessageObject.functionName, "", "functionName matches");

  resolve();
}

function* startNativeCallbackTest(browser) {
  yield spawnWithObserver(browser, testNativeCallback);

  BrowserTestUtils.synthesizeMouseAtCenter("#test-nativeCallback", {}, browser);
  yield waitForResolve(browser);
}

function testConsoleGroup(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  ok(aMessageObject.level == "group" ||
     aMessageObject.level == "groupCollapsed" ||
     aMessageObject.level == "groupEnd",
     "expected level received");

  is(aMessageObject.functionName, "testGroups", "functionName matches");
  ok(aMessageObject.lineNumber >= 46 && aMessageObject.lineNumber <= 50,
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
    is(aMessageObject.groupName, "b group", "groupEnd groupName matches");
  }

  if (aMessageObject.level == "groupEnd") {
    resolve();
  }
}

function* startGroupTest(browser) {
  yield spawnWithObserver(browser, testConsoleGroup);

  BrowserTestUtils.synthesizeMouseAtCenter("#test-groups", {}, browser);
  yield waitForResolve(browser);
}

function testConsoleTime(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  is(aMessageObject.level, gLevel, "expected level received");

  is(aMessageObject.filename, gArgs[0].filename, "filename matches");
  is(aMessageObject.lineNumber, gArgs[0].lineNumber, "lineNumber matches");
  is(aMessageObject.functionName, gArgs[0].functionName, "functionName matches");
  is(aMessageObject.timer.name, gArgs[0].timer.name, "timer.name matches");

  gArgs[0].arguments.forEach(function (a, i) {
    is(aMessageObject.arguments[i], a, "correct arg " + i);
  });

  resolve();
}

function* startTimeTest(browser) {
  yield spawnWithObserver(browser, testConsoleTime, function(opts) {
    gLevel = "time";
    gArgs = [
      {filename: TEST_URI, lineNumber: 23, functionName: "startTimer",
       arguments: ["foo"],
       timer: { name: "foo" },
      }
    ];
  });

  BrowserTestUtils.synthesizeMouseAtCenter("#test-time", {}, browser);
  yield waitForResolve(browser);
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

  resolve();
}

function* startTimeEndTest(browser) {
  yield spawnWithObserver(browser, testConsoleTimeEnd, function(opts) {
    gLevel = "timeEnd";
    gArgs = [
      {filename: TEST_URI, lineNumber: 27, functionName: "stopTimer",
       arguments: ["foo"],
       timer: { name: "foo" },
      },
    ];
  });

  BrowserTestUtils.synthesizeMouseAtCenter("#test-timeEnd", {}, browser);
  yield waitForResolve(browser);
}

function testConsoleTimeStamp(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  is(aMessageObject.level, gLevel, "expected level received");

  is(aMessageObject.filename, gArgs[0].filename, "filename matches");
  is(aMessageObject.lineNumber, gArgs[0].lineNumber, "lineNumber matches");
  is(aMessageObject.functionName, gArgs[0].functionName, "functionName matches");
  ok(aMessageObject.timeStamp > 0, "timeStamp is a positive value");

  gArgs[0].arguments.forEach(function (a, i) {
    is(aMessageObject.arguments[i], a, "correct arg " + i);
  });

  resolve();
}

function* startTimeStampTest(browser) {
  yield spawnWithObserver(browser, testConsoleTimeStamp, function() {
    gLevel = "timeStamp";
    gArgs = [
      {filename: TEST_URI, lineNumber: 58, functionName: "timeStamp",
       arguments: ["!!!"]
      }
    ];
  });

  BrowserTestUtils.synthesizeMouseAtCenter("#test-timeStamp", {}, browser);
  yield waitForResolve(browser);
}

function testEmptyConsoleTimeStamp(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  is(aMessageObject.level, gLevel, "expected level received");

  is(aMessageObject.filename, gArgs[0].filename, "filename matches");
  is(aMessageObject.lineNumber, gArgs[0].lineNumber, "lineNumber matches");
  is(aMessageObject.functionName, gArgs[0].functionName, "functionName matches");
  ok(aMessageObject.timeStamp > 0, "timeStamp is a positive value");
  is(aMessageObject.arguments.length, 0, "we don't have arguments");

  resolve();
}

function* startEmptyTimeStampTest(browser) {
  yield spawnWithObserver(browser, testEmptyConsoleTimeStamp, function() {
    gLevel = "timeStamp";
    gArgs = [
      {filename: TEST_URI, lineNumber: 58, functionName: "timeStamp",
       arguments: []
      }
    ];
  });

  BrowserTestUtils.synthesizeMouseAtCenter("#test-emptyTimeStamp", {}, browser);
  yield waitForResolve(browser);
}

function testEmptyTimer(aMessageObject) {
  let messageWindow = Services.wm.getOuterWindowWithId(aMessageObject.ID);
  is(messageWindow, gWindow, "found correct window by window ID");

  ok(aMessageObject.level == "time" || aMessageObject.level == "timeEnd",
     "expected level received");
  is(aMessageObject.arguments.length, 1, "we have the default argument");
  is(aMessageObject.arguments[0], "default", "we have the default argument");
  ok(aMessageObject.timer, "we have a timer");

  is(aMessageObject.functionName, "namelessTimer", "functionName matches");
  ok(aMessageObject.lineNumber == 31 || aMessageObject.lineNumber == 32,
     "lineNumber matches");

  resolve();
}

function* startEmptyTimerTest(browser) {
  yield spawnWithObserver(browser, testEmptyTimer);

  BrowserTestUtils.synthesizeMouseAtCenter("#test-namelessTimer", {}, browser);
  yield waitForResolve(browser);
}
