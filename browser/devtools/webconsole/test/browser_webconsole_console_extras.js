/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the basic console.log()-style APIs and filtering work.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console-extras.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(hud) {
  waitForSuccess({
    name: "two nodes displayed",
    validatorFn: function()
    {
      return hud.outputNode.querySelectorAll(".hud-msg-node").length == 2;
    },
    successFn: function()
    {
      let nodes = hud.outputNode.querySelectorAll(".hud-msg-node");
      ok(/start/.test(nodes[0].textContent), "start found");
      ok(/end/.test(nodes[1].textContent), "end found - complete!");

      finishTest();
    },
    failureFn: finishTest,
  });

  let button = content.document.querySelector("button");
  ok(button, "we have the button");
  EventUtils.sendMouseEvent({ type: "click" }, button, content);
}
