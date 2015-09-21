/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that window.console functions that are not implemented yet do not
// output anything in the web console and they do not throw any exceptions.
// See bug 614350.

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-console-extras.html";

function test() {
  loadTab(TEST_URI).then(() => {
    openConsole().then(consoleOpened);
  });
}

function consoleOpened(hud) {
  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "start",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    },
    {
      text: "end",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(() => {
    let nodes = hud.outputNode.querySelectorAll(".message");
    is(nodes.length, 2, "only two messages are displayed");
    finishTest();
  });

  let button = content.document.querySelector("button");
  ok(button, "we have the button");
  EventUtils.sendMouseEvent({ type: "click" }, button, content);
}
