/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/" +
                 "webconsole/test/test-bug-585956-console-trace.html";

add_task(function* () {
  let {tab} = yield loadTab("data:text/html;charset=utf8,<p>hello");
  let hud = yield openConsole(tab);

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI);

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "console.trace output",
      consoleTrace: {
        file: "test-bug-585956-console-trace.html",
        fn: "window.foobar585956c",
      },
    }],
  });

  let node = [...result.matched][0];
  ok(node, "found trace log node");

  let obj = node._messageObject;
  ok(obj, "console.trace message object");

  // The expected stack trace object.
  let stacktrace = [
    {
      columnNumber: 3,
      filename: TEST_URI,
      functionName: "window.foobar585956c",
      language: 2,
      lineNumber: 9
    },
    {
      columnNumber: 10,
      filename: TEST_URI,
      functionName: "foobar585956b",
      language: 2,
      lineNumber: 14
    },
    {
      columnNumber: 10,
      filename: TEST_URI,
      functionName: "foobar585956a",
      language: 2,
      lineNumber: 18
    },
    {
      columnNumber: 1,
      filename: TEST_URI,
      functionName: "",
      language: 2,
      lineNumber: 21
    }
  ];

  ok(obj._stacktrace, "found stacktrace object");
  is(obj._stacktrace.toSource(), stacktrace.toSource(),
     "stacktrace is correct");
  isnot(node.textContent.indexOf("bug-585956"), -1, "found file name");
});
