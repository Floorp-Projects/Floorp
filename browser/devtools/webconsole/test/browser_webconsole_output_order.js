/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that any output created from calls to the console API comes after the
// echoed JavaScript.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testOutputOrder);
  }, true);
}

function testOutputOrder(hud) {
  let jsterm = hud.jsterm;
  let outputNode = jsterm.outputNode;

  jsterm.clearOutput();
  jsterm.execute("console.log('foo', 'bar');");

  waitForSuccess({
    name: "console.log message displayed",
    validatorFn: function()
    {
      return outputNode.querySelectorAll(".hud-msg-node").length == 3;
    },
    successFn: function()
    {
      let nodes = outputNode.querySelectorAll(".hud-msg-node");
      let executedStringFirst =
        /console\.log\('foo', 'bar'\);/.test(nodes[0].textContent);
      let outputSecond = /foo bar/.test(nodes[2].textContent);
      ok(executedStringFirst && outputSecond, "executed string comes first");

      finishTest();
    },
    failureFn: finishTest,
  });
}
