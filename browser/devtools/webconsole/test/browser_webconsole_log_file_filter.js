/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the text filter box works to filter based on filenames
// where the logs were generated.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug_923281_console_log_filter.html";

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
  let console = content.console;
  console.log("sentinel log");
  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "sentinel log",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG
    }],
  }).then(testLiveFilteringOnSearchStrings);
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

  finishTest();
}

function countMessageNodes() {
  let outputNode = hud.outputNode;

  let messageNodes = outputNode.querySelectorAll(".message");
  content.console.log(messageNodes.length);
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

function setStringFilter(aValue)
{
  hud.ui.filterBox.value = aValue;
  hud.ui.adjustVisibilityOnSearchStringChange();
}

