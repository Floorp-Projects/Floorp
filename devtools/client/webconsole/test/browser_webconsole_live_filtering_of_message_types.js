/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the message type filter checkboxes work.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();

  let console = content.console;

  for (let i = 0; i < 50; i++) {
    console.log("foobarz #" + i);
  }

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foobarz #49",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  is(hud.outputNode.children.length, 50, "number of messages");

  hud.setFilterState("log", false);
  is(countMessageNodes(hud), 0, "the log nodes are hidden when the " +
    "corresponding filter is switched off");

  hud.setFilterState("log", true);
  is(countMessageNodes(hud), 50, "the log nodes reappear when the " +
    "corresponding filter is switched on");
});

function countMessageNodes(hud) {
  let messageNodes = hud.outputNode.querySelectorAll(".message");
  let displayedMessageNodes = 0;
  let view = hud.iframeWindow;
  for (let i = 0; i < messageNodes.length; i++) {
    let computedStyle = view.getComputedStyle(messageNodes[i], null);
    if (computedStyle.display !== "none") {
      displayedMessageNodes++;
    }
  }

  return displayedMessageNodes;
}
