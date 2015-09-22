/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

 // Check if console provides the right column number alongside line number

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
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
  let expected = ["10:7", "10:39", "11:9", "12:11", "13:9", "14:7"];

  for (let i = 0, len = messages.length; i < len; i++) {
    let msg = messages[i].textContent;

    is(msg.includes(expected[i]), true, "Found expected line:column of " +
                    expected[i]);
  }

  finishTest();
}
