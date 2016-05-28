/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                   "test/test-bug_939783_console_trace_duplicates.html";

  Task.spawn(runner).then(finishTest);

  function* runner() {
    const {tab} = yield loadTab("data:text/html;charset=utf8,<p>hello");
    const hud = yield openConsole(tab);

    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI);

    // NB: Now that stack frames include a column number multiple invocations
    //     on the same line are considered unique. ie:
    //       |foo(); foo();|
    //     will generate two distinct trace entries.
    yield waitForMessages({
      webconsole: hud,
      messages: [{
        name: "console.trace output for foo1()",
        text: "foo1()",
        consoleTrace: {
          file: "test-bug_939783_console_trace_duplicates.html",
          fn: "foo3()",
        },
      }, {
        name: "console.trace output for foo1()",
        text: "foo1()",
        consoleTrace: {
          file: "test-bug_939783_console_trace_duplicates.html",
          fn: "foo3()",
        },
      }, {
        name: "console.trace output for foo1b()",
        text: "foo1b()",
        consoleTrace: {
          file: "test-bug_939783_console_trace_duplicates.html",
          fn: "foo3()",
        },
      }],
    });
  }
}
