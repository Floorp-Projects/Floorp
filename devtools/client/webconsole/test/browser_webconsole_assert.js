/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that console.assert() works as expected (i.e. outputs only on falsy
// asserts). See bug 760193.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console-assert.html";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  yield consoleOpened(hud);
});

function consoleOpened(hud) {
  hud.jsterm.execute("test()");

  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "undefined",
      category: CATEGORY_OUTPUT,
      severity: SEVERITY_LOG,
    },
    {
      text: "start",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    },
    {
      text: "false assert",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_ERROR,
    },
    {
      text: "falsy assert",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_ERROR,
    },
    {
      text: "end",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(() => {
    let nodes = hud.outputNode.querySelectorAll(".message");
    is(nodes.length, 6,
       "only six messages are displayed, no output from the true assert");
  });
}
