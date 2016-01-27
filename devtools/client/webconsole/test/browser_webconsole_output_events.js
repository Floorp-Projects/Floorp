/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.

"use strict";

thisTestLeaksUncaughtRejectionsAndShouldBeFixed("null");

// Test the webconsole output for DOM events.

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console-output-events.html";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  hud.jsterm.clearOutput();
  hud.jsterm.execute("testDOMEvents()");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "testDOMEvents() output",
      text: "undefined",
      category: CATEGORY_OUTPUT,
    }],
  });

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "console.log() output for mousemove",
      text: /eventLogger mousemove { target: .+, buttons: 0, clientX: \d+, clientY: \d+, layerX: \d+, layerY: \d+ }/,
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "console.log() output for keypress",
      text: /eventLogger keypress Shift { target: .+, key: .+, charCode: \d+, keyCode: \d+ }/,
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });
});
