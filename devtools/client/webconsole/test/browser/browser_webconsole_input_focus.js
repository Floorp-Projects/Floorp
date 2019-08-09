/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the input field is focused when the console is opened.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,Test input focused
  <script>
    console.log("console message 1");
  </script>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Focus after console is opened");
  ok(isInputFocused(hud), "input node is focused after console is opened");

  hud.ui.clearOutput();
  ok(isInputFocused(hud), "input node is focused after output is cleared");

  info("Focus during message logging");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    content.wrappedJSObject.console.log("console message 2");
  });
  const msg = await waitFor(() => findMessage(hud, "console message 2"));
  ok(isInputFocused(hud), "input node is focused, first time");

  info("Focus after clicking in the output area");
  await waitForBlurredInput(hud);
  EventUtils.sendMouseEvent({ type: "click" }, msg);
  ok(isInputFocused(hud), "input node is focused, second time");

  info("Setting a text selection and making sure a click does not re-focus");
  await waitForBlurredInput(hud);
  const selection = hud.iframeWindow.getSelection();
  selection.selectAllChildren(msg.querySelector(".message-body"));
  EventUtils.sendMouseEvent({ type: "click" }, msg);
  ok(!isInputFocused(hud), "input node not focused after text is selected");
});

function waitForBlurredInput(hud) {
  const node = hud.jsterm.node;
  return new Promise(resolve => {
    const lostFocus = () => {
      ok(!isInputFocused(hud), "input node is not focused");
      resolve();
    };
    node.addEventListener("focusout", lostFocus, { once: true });

    // The 'focusout' event fires if we focus e.g. the filter box.
    getFilterInput(hud).focus();
  });
}
