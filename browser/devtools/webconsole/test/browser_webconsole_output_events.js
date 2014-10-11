/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("null");

// Test the webconsole output for DOM events.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-output-events.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    Task.spawn(runner);
  }, true);

  function* runner()
  {
    let hud = yield openConsole();

    hud.jsterm.clearOutput();
    hud.jsterm.execute("testDOMEvents()");

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        name: "testDOMEvents() output",
        text: "undefined",
        category: CATEGORY_OUTPUT,
      }],
    });

    EventUtils.synthesizeMouse(content.document.body, 2, 2, {type: "mousemove"}, content);

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        name: "console.log() output for mousemove",
        text: /"eventLogger" mousemove { target: .+, buttons: 1, clientX: \d+, clientY: \d+, layerX: \d+, layerY: \d+ }/,
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    });

    content.focus();
    EventUtils.synthesizeKey("a", {shiftKey: true}, content);

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        name: "console.log() output for keypress",
        text: /"eventLogger" keypress Shift { target: .+, key: .+, charCode: \d+, keyCode: \d+ }/,
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    });

    finishTest();
  }
}
