/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that console.count() counts as expected. See bug 922208.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-count.html";

function test() {
  Task.spawn(runner).then(finishTest);

  function* runner() {
    const {tab} = yield loadTab(TEST_URI);
    const hud = yield openConsole(tab);

    let button = content.document.querySelector("#local");
    ok(button, "we have the local-tests button");
    EventUtils.sendMouseEvent({ type: "click" }, button, content);
    let messages = [];
    [
      "start",
      "<no label>: 2",
      "console.count() testcounter: 1",
      "console.count() testcounter: 2",
      "console.count() testcounter: 3",
      "console.count() testcounter: 4",
      "end"
    ].forEach(function (msg) {
      messages.push({
        text: msg,
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG
      });
    });
    messages.push({
      name: "Three local counts with no label and count=1",
      text: "<no label>: 1",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      count: 3
    });
    yield waitForMessages({
      webconsole: hud,
      messages: messages
    });

    hud.jsterm.clearOutput();

    button = content.document.querySelector("#external");
    ok(button, "we have the external-tests button");
    EventUtils.sendMouseEvent({ type: "click" }, button, content);
    messages = [];
    [
      "start",
      "console.count() testcounter: 5",
      "console.count() testcounter: 6",
      "end"
    ].forEach(function (msg) {
      messages.push({
        text: msg,
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG
      });
    });
    messages.push({
      name: "Two external counts with no label and count=1",
      text: "<no label>: 1",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      count: 2
    });
    yield waitForMessages({
      webconsole: hud,
      messages: messages
    });
  }
}
