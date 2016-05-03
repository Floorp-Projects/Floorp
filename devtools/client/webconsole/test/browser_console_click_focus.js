/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the input field is focused when the console is opened.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "Dolske Digs Bacon",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  let msg = [...result.matched][0];
  let outputItem = msg.querySelector(".message-body");
  ok(outputItem, "found a logged message");

  let inputNode = hud.jsterm.inputNode;
  ok(inputNode.getAttribute("focused"), "input node is focused, first");

  yield waitForBlurredInput(inputNode);

  EventUtils.sendMouseEvent({type: "click"}, hud.outputNode);
  ok(inputNode.getAttribute("focused"), "input node is focused, second time");

  yield waitForBlurredInput(inputNode);

  info("Setting a text selection and making sure a click does not re-focus");
  let selection = hud.iframeWindow.getSelection();
  selection.selectAllChildren(outputItem);

  EventUtils.sendMouseEvent({type: "click"}, hud.outputNode);
  ok(!inputNode.getAttribute("focused"),
    "input node is not focused after drag");
});

function waitForBlurredInput(inputNode) {
  return new Promise(resolve => {
    let lostFocus = () => {
      inputNode.removeEventListener("blur", lostFocus);
      ok(!inputNode.getAttribute("focused"), "input node is not focused");
      resolve();
    };
    inputNode.addEventListener("blur", lostFocus);
    document.getElementById("urlbar").click();
  });
}
