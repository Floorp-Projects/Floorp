/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that commands run by the user are executed in content space.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

add_task(function* () {
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
