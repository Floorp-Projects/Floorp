/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the debugger panes collapse properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    Task.spawn(function* () {
      let doc = aPanel.panelWin.document;
      let panel = doc.getElementById("instruments-pane");
      let button = doc.getElementById("instruments-pane-toggle");
      ok(panel.classList.contains("pane-collapsed"),
          "The instruments panel is initially in collapsed state");

      yield togglePane(button, "Press on the toggle button to expand", panel, "VK_RETURN");
      ok(!panel.classList.contains("pane-collapsed"),
          "The instruments panel is in the expanded state");

      yield togglePane(button, "Press on the toggle button to collapse", panel, "VK_SPACE");
      ok(panel.classList.contains("pane-collapsed"),
        "The instruments panel is in the collapsed state");

      closeDebuggerAndFinish(aPanel);
    });
  });
}

function* togglePane(button, message, pane, keycode) {
  let onTransitionEnd = once(pane, "transitionend");
  info(message);
  button.focus();
  EventUtils.synthesizeKey(keycode, {});
  yield onTransitionEnd;
}
