/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that warnings about ineffective iframe sandboxing are logged to the
// web console when necessary (and not otherwise).

"use strict";

requestLongerTimeout(2);

const TEST_URI_WARNING = "http://example.com/browser/devtools/client/webconsole/test/test-bug-752559-ineffective-iframe-sandbox-warning0.html";
const TEST_URI_NOWARNING = [
  "http://example.com/browser/devtools/client/webconsole/test/test-bug-752559-ineffective-iframe-sandbox-warning1.html",
  "http://example.com/browser/devtools/client/webconsole/test/test-bug-752559-ineffective-iframe-sandbox-warning2.html",
  "http://example.com/browser/devtools/client/webconsole/test/test-bug-752559-ineffective-iframe-sandbox-warning3.html",
  "http://example.com/browser/devtools/client/webconsole/test/test-bug-752559-ineffective-iframe-sandbox-warning4.html",
  "http://example.com/browser/devtools/client/webconsole/test/test-bug-752559-ineffective-iframe-sandbox-warning5.html"
];

const INEFFECTIVE_IFRAME_SANDBOXING_MSG = "An iframe which has both " +
  "allow-scripts and allow-same-origin for its sandbox attribute can remove " +
  "its sandboxing.";
const SENTINEL_MSG = "testing ineffective sandboxing message";

add_task(function* () {
  yield testYesWarning();

  for (let id = 0; id < TEST_URI_NOWARNING.length; id++) {
    yield testNoWarning(id);
  }
});

function* testYesWarning() {
  yield loadTab(TEST_URI_WARNING);
  let hud = yield openConsole();

  ContentTask.spawn(gBrowser.selectedBrowser, SENTINEL_MSG, function* (msg) {
    content.console.log(msg);
  });

  yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "Ineffective iframe sandboxing warning displayed successfully",
        text: INEFFECTIVE_IFRAME_SANDBOXING_MSG,
        category: CATEGORY_SECURITY,
        severity: SEVERITY_WARNING
      },
      {
        text: SENTINEL_MSG,
        severity: SEVERITY_LOG
      }
    ]
  });

  let msgs = hud.outputNode.querySelectorAll(".message[category=security]");
  is(msgs.length, 1, "one security message");
}

function* testNoWarning(id) {
  yield loadTab(TEST_URI_NOWARNING[id]);
  let hud = yield openConsole();

  ContentTask.spawn(gBrowser.selectedBrowser, SENTINEL_MSG, function* (msg) {
    content.console.log(msg);
  });

  yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        text: SENTINEL_MSG,
        severity: SEVERITY_LOG
      }
    ]
  });

  let msgs = hud.outputNode.querySelectorAll(".message[category=security]");
  is(msgs.length, 0, "no security messages (case " + id + ")");
}
