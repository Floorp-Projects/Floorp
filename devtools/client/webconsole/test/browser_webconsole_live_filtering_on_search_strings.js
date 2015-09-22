/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the text filter box works.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

var test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();

  let console = content.console;

  for (let i = 0; i < 50; i++) {
    console.log("http://www.example.com/ " + i);
  }

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "http://www.example.com/ 49",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  is(hud.outputNode.children.length, 50, "number of messages");

  setStringFilter(hud, "http");
  isnot(countMessageNodes(hud), 0, "the log nodes are not hidden when the " +
    "search string is set to \"http\"");

  setStringFilter(hud, "hxxp");
  is(countMessageNodes(hud), 0, "the log nodes are hidden when the search " +
    "string is set to \"hxxp\"");

  setStringFilter(hud, "ht tp");
  isnot(countMessageNodes(hud), 0, "the log nodes are not hidden when the " +
    "search string is set to \"ht tp\"");

  setStringFilter(hud, " zzzz   zzzz ");
  is(countMessageNodes(hud), 0, "the log nodes are hidden when the search " +
    "string is set to \" zzzz   zzzz \"");

  setStringFilter(hud, "");
  isnot(countMessageNodes(hud), 0, "the log nodes are not hidden when the " +
    "search string is removed");

  setStringFilter(hud, "\u9f2c");
  is(countMessageNodes(hud), 0, "the log nodes are hidden when searching " +
    "for weasels");

  setStringFilter(hud, "\u0007");
  is(countMessageNodes(hud), 0, "the log nodes are hidden when searching for " +
    "the bell character");

  setStringFilter(hud, '"foo"');
  is(countMessageNodes(hud), 0, "the log nodes are hidden when searching for " +
    'the string "foo"');

  setStringFilter(hud, "'foo'");
  is(countMessageNodes(hud), 0, "the log nodes are hidden when searching for " +
    "the string 'foo'");

  setStringFilter(hud, "foo\"bar'baz\"boo'");
  is(countMessageNodes(hud), 0, "the log nodes are hidden when searching for " +
    "the string \"foo\"bar'baz\"boo'\"");
});

function countMessageNodes(hud) {
  let outputNode = hud.outputNode;

  let messageNodes = outputNode.querySelectorAll(".message");
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

function setStringFilter(hud, value) {
  hud.ui.filterBox.value = value;
  hud.ui.adjustVisibilityOnSearchStringChange();
}

