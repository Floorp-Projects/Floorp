/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that we have the correct line selected after pretty printing.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;

    Task.spawn(function* () {
      yield waitForSourceShown(gPanel, "code_ugly.js");

      const paused = waitForPause(gDebugger.gThreadClient);
      callInTab(gTab, "foo");
      yield paused;

      const finished = promise.all([
        waitForSourceShown(gPanel, "code_ugly.js"),
        waitForCaretUpdated(gPanel, 7)
      ]);
      gDebugger.document.getElementById("pretty-print").click();
      yield finished;

      resumeDebuggerThenCloseAndFinish(gPanel);
    });
  });
}
