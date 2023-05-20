/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the input field is focused when the console is opened.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>Test input focused
  <script>
    console.log("console message 1");
  </script>`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Focus after console is opened");
  ok(isInputFocused(hud), "input node is focused after console is opened");

  info("Set the input value and select the first line");
  const expression = `x = 10;x;
    x = 20;x;`;
  setInputValue(hud, expression);
  hud.ui.jsterm.editor.setSelection(
    { line: 0, ch: 0 },
    { line: 0, ch: expression.split("\n")[0].length }
  );

  await clearOutput(hud);
  ok(isInputFocused(hud), "input node is focused after output is cleared");

  info("Focus during message logging");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.console.log("console message 2");
  });
  const msg = await waitFor(() =>
    findConsoleAPIMessage(hud, "console message 2")
  );
  ok(isInputFocused(hud), "input node is focused, first time");

  // Checking that there's still a selection in the input
  is(
    hud.ui.jsterm.editor.getSelection(),
    "x = 10;x;",
    "editor has the expected selection"
  );

  info("Focus after clicking in the output area");
  await waitForBlurredInput(hud);
  AccessibilityUtils.setEnv({
    actionCountRule: false,
    focusableRule: false,
    interactiveRule: false,
    labelRule: false,
  });
  EventUtils.sendMouseEvent({ type: "click" }, msg);
  AccessibilityUtils.resetEnv();
  ok(isInputFocused(hud), "input node is focused, second time");

  is(
    hud.ui.jsterm.editor.getSelection(),
    "",
    "input selection was removed when the input was blurred"
  );

  info("Setting a text selection and making sure a click does not re-focus");
  await waitForBlurredInput(hud);
  const selection = hud.iframeWindow.getSelection();
  selection.selectAllChildren(msg.querySelector(".message-body"));
  AccessibilityUtils.setEnv({
    actionCountRule: false,
    focusableRule: false,
    interactiveRule: false,
    labelRule: false,
  });
  EventUtils.sendMouseEvent({ type: "click" }, msg);
  AccessibilityUtils.resetEnv();
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
