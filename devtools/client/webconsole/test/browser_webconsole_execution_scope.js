/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that commands run by the user are executed in content space.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

var test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();
  hud.jsterm.execute("window.location.href;");

  let [input, output] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "window.location.href;",
      category: CATEGORY_INPUT,
    },
    {
      text: TEST_URI,
      category: CATEGORY_OUTPUT,
    }],
  });

  let inputNode = [...input.matched][0];
  let outputNode = [...output.matched][0];
  is(inputNode.getAttribute("category"), "input",
     "input node category is correct");
  is(outputNode.getAttribute("category"), "output",
     "output node category is correct");
});
