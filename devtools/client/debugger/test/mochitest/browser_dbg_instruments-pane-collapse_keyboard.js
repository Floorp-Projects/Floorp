/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the debugger panes collapse properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

async function test() {
  let [aTab,, aPanel] = await initDebugger(TAB_URL);

  let doc = aPanel.panelWin.document;
  let panel = doc.getElementById("instruments-pane");
  let button = doc.getElementById("instruments-pane-toggle");
  ok(panel.classList.contains("pane-collapsed"),
     "The instruments panel is initially in collapsed state");

  await togglePane(button, "Press on the toggle button to expand", panel, "VK_RETURN");
  ok(!panel.classList.contains("pane-collapsed"),
     "The instruments panel is in the expanded state");

  await togglePane(button, "Press on the toggle button to collapse", panel, "VK_SPACE");
  ok(panel.classList.contains("pane-collapsed"),
     "The instruments panel is in the collapsed state");

  closeDebuggerAndFinish(aPanel);
}

async function togglePane(button, message, pane, keycode) {
  let onTransitionEnd = once(pane, "transitionend");
  info(message);
  button.focus();
  EventUtils.synthesizeKey(keycode, {});
  await onTransitionEnd;
}
