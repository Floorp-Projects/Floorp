/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that commands run by the user are executed in content space.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testExecutionScope);
  }, true);
}

function testExecutionScope(hud) {
  let jsterm = hud.jsterm;

  jsterm.clearOutput();
  jsterm.execute("window.location.href;");
  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "window.location.href;",
      category: CATEGORY_INPUT,
    },
    {
      text: TEST_URI,
      category: CATEGORY_OUTPUT,
    }],
  }).then(([input, output]) => {
    let inputNode = [...input.matched][0];
    let outputNode = [...output.matched][0];
    is(inputNode.getAttribute("category"), "input", "input node category is correct");
    is(outputNode.getAttribute("category"), "output", "output node category is correct");
    finishTest();
  });
}

