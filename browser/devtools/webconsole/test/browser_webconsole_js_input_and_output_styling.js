/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the correct CSS styles are applied to the lines of console
// output.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testJSInputAndOutputStyling);
  }, true);
}

function testJSInputAndOutputStyling(hud) {
  let jsterm = hud.jsterm;

  jsterm.clearOutput();
  jsterm.execute("2 + 2");

  waitForSuccess({
    name: "jsterm output is displayed",
    validatorFn: function()
    {
      return jsterm.outputNode.querySelector(".webconsole-msg-output");
    },
    successFn: function()
    {
      let jsInputNode = jsterm.outputNode.querySelector(".hud-msg-node");
      isnot(jsInputNode.textContent.indexOf("2 + 2"), -1,
            "JS input node contains '2 + 2'");
      ok(jsInputNode.classList.contains("webconsole-msg-input"),
         "JS input node is of the CSS class 'webconsole-msg-input'");

      let output = jsterm.outputNode.querySelector(".webconsole-msg-output");
      isnot(output.textContent.indexOf("4"), -1,
            "JS output node contains '4'");

      finishTest();
    },
    failureFn: finishTest,
  });
}

