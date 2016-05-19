/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1201008 - Make sure you can't set a breakpoint in a blank
 * editor
 */

function test() {
  initDebugger("data:text/html,hi").then(([aTab,, aPanel]) => {
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;

    Task.spawn(function* () {
      const editor = gDebugger.DebuggerView.editor;
      editor.emit("gutterClick", 0);
      is(editor.getBreakpoints().length, 0,
         "A breakpoint should not exist");

      closeDebuggerAndFinish(gPanel);
    });
  });
}
