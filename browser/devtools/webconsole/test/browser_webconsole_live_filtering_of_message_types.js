/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the message type filter checkboxes work.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

let hud;

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(aHud) {
  hud = aHud;
  hud.jsterm.clearOutput();

  let console = content.console;

  for (let i = 0; i < 50; i++) {
    console.log("foobarz #" + i);
  }

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foobarz #49",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(testLiveFilteringOfMessageTypes);
}

function testLiveFilteringOfMessageTypes() {
  is(hud.outputNode.itemCount, 50, "number of messages");

  hud.setFilterState("log", false);
  is(countMessageNodes(), 0, "the log nodes are hidden when the " +
    "corresponding filter is switched off");

  hud.setFilterState("log", true);
  is(countMessageNodes(), 50, "the log nodes reappear when the " +
    "corresponding filter is switched on");

  finishTest();
}

function countMessageNodes() {
  let messageNodes = hud.outputNode.querySelectorAll(".hud-log");
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
