/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

 // Check if console provides the right column number alongside line number

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/old/" +
                 "test/test-console-column.html";

var hud;

function test() {
  loadTab(TEST_URI).then(() => {
    openConsole().then(consoleOpened);
  });
}

function consoleOpened(aHud) {
  hud = aHud;

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "Error Message",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_ERROR
    }]
  }).then(testLocationColumn);
}

function testLocationColumn() {
  let messages = hud.outputNode.children;
  let expected = ["11:7", "11:39", "12:9", "13:11", "14:9", "15:7"];

  for (let i = 0, len = messages.length; i < len; i++) {
    let msg = messages[i].textContent;

    is(msg.includes(expected[i]), true, "Found expected line:column of " +
                    expected[i]);
  }

  finishTest();
}
