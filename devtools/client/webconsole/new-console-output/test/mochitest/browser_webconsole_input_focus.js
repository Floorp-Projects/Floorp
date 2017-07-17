/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the input field is focused when the console is opened.

"use strict";

const TEST_URI =
  `data:text/html;charset=utf-8,Test input focused
  <script>
    console.log("console message 1");
  </script>`;

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);
  hud.jsterm.clearOutput();

  let inputNode = hud.jsterm.inputNode;
  ok(inputNode.getAttribute("focused"), "input node is focused after output is cleared");

  ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.wrappedJSObject.console.log("console message 2");
  });
  let msg = yield waitFor(() => findMessage(hud, "console message 2"));
  let outputItem = msg.querySelector(".message-body");

  inputNode = hud.jsterm.inputNode;
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
    "input node focused after text is selected");
});

function waitForBlurredInput(inputNode) {
  return new Promise(resolve => {
    let lostFocus = () => {
      ok(!inputNode.getAttribute("focused"), "input node is not focused");
      resolve();
    };
    inputNode.addEventListener("blur", lostFocus, { once: true });
    document.getElementById("urlbar").click();
  });
}
