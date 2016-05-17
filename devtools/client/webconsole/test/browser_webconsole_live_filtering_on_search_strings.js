/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the text filter box works.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();

  ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    for (let i = 0; i < 50; i++) {
      content.console.log("http://www.example.com/ " + i);
    }
  });

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

