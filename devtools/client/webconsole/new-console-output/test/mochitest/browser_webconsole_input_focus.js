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

  info("Focus during message logging");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.wrappedJSObject.console.log("console message 2");
  });
  let msg = yield waitFor(() => findMessage(hud, "console message 2"));
  ok(inputNode.getAttribute("focused"), "input node is focused, first time");

  info("Focus after clicking in the output area");
  yield waitForBlurredInput(hud);
  EventUtils.sendMouseEvent({type: "click"}, msg);
  ok(inputNode.getAttribute("focused"), "input node is focused, second time");

  info("Setting a text selection and making sure a click does not re-focus");
  yield waitForBlurredInput(hud);
  let selection = hud.iframeWindow.getSelection();
  selection.selectAllChildren(msg.querySelector(".message-body"));
  EventUtils.sendMouseEvent({type: "click"}, msg);
  ok(!inputNode.getAttribute("focused"),
    "input node not focused after text is selected");
});

function waitForBlurredInput(hud) {
  let inputNode = hud.jsterm.inputNode;
  return new Promise(resolve => {
    let lostFocus = () => {
      ok(!inputNode.getAttribute("focused"), "input node is not focused");
      resolve();
    };
    inputNode.addEventListener("blur", lostFocus, { once: true });

    // Clicking on a DOM Node outside of the webconsole document. The 'blur' event fires
    // if we click on something in this document (like the filter box), but the 'focus'
    // event won't re-fire on the textbox XBL binding when it's clicked on again.
    // Bug 1304328 is tracking removal of XUL for jsterm, we should be able to click on
    // the filter textbox instead of the url bar after that.
    document.getElementById("urlbar").click();
  });
}
