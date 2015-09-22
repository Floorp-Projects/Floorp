/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the console API messages for console.error()/exception()/assert()
// include a stackframe. See bug 920116.

function test() {
  let hud;

  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                   "test/test-console-api-stackframe.html";
  const TEST_FILE = TEST_URI.substr(TEST_URI.lastIndexOf("/"));

  Task.spawn(runner).then(finishTest);

  function* runner() {
    const {tab} = yield loadTab(TEST_URI);
    hud = yield openConsole(tab);

    const stack = [{
      file: TEST_FILE,
      fn: "thirdCall",
      // 21,22,23
      line: /\b2[123]\b/,
    }, {
      file: TEST_FILE,
      fn: "secondCall",
      line: 16,
    }, {
      file: TEST_FILE,
      fn: "firstCall",
      line: 12,
    }];

    let results = yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "foo-log",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
        collapsible: false,
      }, {
        text: "foo-error",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_ERROR,
        collapsible: true,
        stacktrace: stack,
      }, {
        text: "foo-exception",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_ERROR,
        collapsible: true,
        stacktrace: stack,
      }, {
        text: "foo-assert",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_ERROR,
        collapsible: true,
        stacktrace: stack,
      }],
    });

    let elem = [...results[1].matched][0];
    ok(elem, "message element");

    let msg = elem._messageObject;
    ok(msg, "message object");

    ok(msg.collapsed, "message is collapsed");

    msg.toggleDetails();

    ok(!msg.collapsed, "message is not collapsed");

    msg.toggleDetails();

    ok(msg.collapsed, "message is collapsed");

    yield closeConsole(tab);
  }
}
