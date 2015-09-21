/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the console receive exceptions include a stackframe.
// See bug 1184172.

// On e10s, the exception is triggered in child process
// and is ignored by test harness
if (!Services.appinfo.browserTabsRemoteAutostart) {
  expectUncaughtException();
}

function test() {
  let hud;

  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                   "test/test-exception-stackframe.html";
  const TEST_FILE = TEST_URI.substr(TEST_URI.lastIndexOf("/"));

  Task.spawn(runner).then(finishTest);

  function* runner() {
    const {tab} = yield loadTab(TEST_URI);
    hud = yield openConsole(tab);

    const stack = [{
      file: TEST_FILE,
      fn: "thirdCall",
      line: 21,
    }, {
      file: TEST_FILE,
      fn: "secondCall",
      line: 17,
    }, {
      file: TEST_FILE,
      fn: "firstCall",
      line: 12,
    }];

    let results = yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "nonExistingMethodCall is not defined",
        category: CATEGORY_JS,
        severity: SEVERITY_ERROR,
        collapsible: true,
        stacktrace: stack,
      }],
    });

    let elem = [...results[0].matched][0];
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
