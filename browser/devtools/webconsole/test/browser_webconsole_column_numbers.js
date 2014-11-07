/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

 // Check if console provides the right column number alongside line number

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-column.html";

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

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: 'Error Message',
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_ERROR
    }]
  }).then(testLocationColumn);
}

function testLocationColumn() {
  let messages = hud.outputNode.children;
  let expected = ['6:6', '6:38', '7:8', '8:10', '9:8', '10:6'];

  let valid = true;

  for(let i = 0, len = messages.length; i < len; i++) {
    let msg = messages[i].textContent;

    is(!msg.contains(expected[i]), true, 'Found expected line:column of ' + expected[i])
  }

  is(valid, true, 'column numbers match expected results');

  finishTest();
}