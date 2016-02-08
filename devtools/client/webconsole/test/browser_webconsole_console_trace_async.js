/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/" +
                 "webconsole/test/test-console-trace-async.html";

add_task(function* runTest() {
  // Async stacks aren't on by default in all builds
  yield new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["javascript.options.asyncstack", true]
    ]}, resolve);
  });

  let {tab} = yield loadTab("data:text/html;charset=utf8,<p>hello");
  let hud = yield openConsole(tab);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI);

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "console.trace output",
      consoleTrace: {
        file: "test-console-trace-async.html",
        fn: "inner",
      },
    }],
  });

  let node = [...result.matched][0];
  ok(node, "found trace log node");
  ok(node.textContent.includes("console.trace()"),
     "trace log node includes console.trace()");
  ok(node.textContent.includes("promise callback"),
     "trace log node includes promise callback");
  ok(node.textContent.includes("setTimeout handler"),
     "trace log node includes setTimeout handler");

  // The expected stack trace object.
  let stacktrace = [
    {
      columnNumber: 3,
      filename: TEST_URI,
      functionName: "inner",
      language: 2,
      lineNumber: 9
    },
    {
      asyncCause: "promise callback",
      columnNumber: 3,
      filename: TEST_URI,
      functionName: "time1",
      language: 2,
      lineNumber: 13,
    },
    {
      asyncCause: "setTimeout handler",
      columnNumber: 1,
      filename: TEST_URI,
      functionName: "",
      language: 2,
      lineNumber: 18,
    }
  ];

  let obj = node._messageObject;
  ok(obj, "console.trace message object");
  ok(obj._stacktrace, "found stacktrace object");
  is(obj._stacktrace.toSource(), stacktrace.toSource(),
    "stacktrace is correct");
});
