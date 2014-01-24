/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug_939783_console_trace_duplicates.html";

  Task.spawn(runner).then(finishTest);

  function* runner() {
    const {tab} = yield loadTab("data:text/html;charset=utf8,<p>hello");
    const hud = yield openConsole(tab);

    content.location = TEST_URI;

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        name: "console.trace output for foo1()",
        text: "foo1()",
        repeats: 2,
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
