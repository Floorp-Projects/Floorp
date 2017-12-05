/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the text filter box works to filter based on filenames
// where the logs were generated.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug_923281_console_log_filter.html";

var hud;

add_task(function* () {
  yield loadTab(TEST_URI);

  hud = yield openConsole();
  yield consoleOpened();

  testLiveFilteringOnSearchStrings();

  hud = null;
});

function consoleOpened() {
  let console = gBrowser.contentWindowAsCPOW.console;
  console.log("sentinel log");
  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "sentinel log",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG
    }],
  });
}

function testLiveFilteringOnSearchStrings() {
  is(hud.outputNode.children.length, 4, "number of messages");

  setStringFilter("random");
  is(countMessageNodes(), 1, "the log nodes not containing string " +
      "\"random\" are hidden");

  setStringFilter("test2.js");
  is(countMessageNodes(), 2, "show only log nodes containing string " +
      "\"test2.js\" or log nodes created from files with filename " +
      "containing \"test2.js\" as substring.");

  setStringFilter("test1");
  is(countMessageNodes(), 2, "show only log nodes containing string " +
      "\"test1\" or log nodes created from files with filename " +
      "containing \"test1\" as substring.");

  setStringFilter("");
  is(countMessageNodes(), 4, "show all log nodes on setting filter string " +
      "as \"\".");
}

function countMessageNodes() {
  let outputNode = hud.outputNode;

  let messageNodes = outputNode.querySelectorAll(".message");
  gBrowser.contentWindowAsCPOW.console.log(messageNodes.length);
  let displayedMessageNodes = 0;
  let view = hud.iframeWindow;
  for (let i = 0; i < messageNodes.length; i++) {
    let computedStyle = view.getComputedStyle(messageNodes[i]);
    if (computedStyle.display !== "none") {
      displayedMessageNodes++;
    }
  }

  return displayedMessageNodes;
}

function setStringFilter(value) {
  hud.ui.filterBox.value = value;
  hud.ui.adjustVisibilityOnSearchStringChange();
}

