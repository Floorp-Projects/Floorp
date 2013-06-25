/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the text filter box works.

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
    console.log("http://www.example.com/ " + i);
  }

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "http://www.example.com/ 49",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  }).then(testLiveFilteringOnSearchStrings);
}

function testLiveFilteringOnSearchStrings() {
  is(hud.outputNode.itemCount, 50, "number of messages");

  setStringFilter("http");
  isnot(countMessageNodes(), 0, "the log nodes are not hidden when the " +
    "search string is set to \"http\"");

  setStringFilter("hxxp");
  is(countMessageNodes(), 0, "the log nodes are hidden when the search " +
    "string is set to \"hxxp\"");

  setStringFilter("ht tp");
  isnot(countMessageNodes(), 0, "the log nodes are not hidden when the " +
    "search string is set to \"ht tp\"");

  setStringFilter(" zzzz   zzzz ");
  is(countMessageNodes(), 0, "the log nodes are hidden when the search " +
    "string is set to \" zzzz   zzzz \"");

  setStringFilter("");
  isnot(countMessageNodes(), 0, "the log nodes are not hidden when the " +
    "search string is removed");

  setStringFilter("\u9f2c");
  is(countMessageNodes(), 0, "the log nodes are hidden when searching " +
    "for weasels");

  setStringFilter("\u0007");
  is(countMessageNodes(), 0, "the log nodes are hidden when searching for " +
    "the bell character");

  setStringFilter('"foo"');
  is(countMessageNodes(), 0, "the log nodes are hidden when searching for " +
    'the string "foo"');

  setStringFilter("'foo'");
  is(countMessageNodes(), 0, "the log nodes are hidden when searching for " +
    "the string 'foo'");

  setStringFilter("foo\"bar'baz\"boo'");
  is(countMessageNodes(), 0, "the log nodes are hidden when searching for " +
    "the string \"foo\"bar'baz\"boo'\"");

  finishTest();
}

function countMessageNodes() {
  let outputNode = hud.outputNode;

  let messageNodes = outputNode.querySelectorAll(".hud-log");
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

