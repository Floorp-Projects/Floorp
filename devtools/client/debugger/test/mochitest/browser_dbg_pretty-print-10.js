/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that we disable the pretty print button for black boxed sources,
 * and that clicking it doesn't do anything.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print.html";

function test() {
  // Wait for debugger panel to be fully set and break on debugger statement
  let options = {
    source: EXAMPLE_URL + "code_ugly.js",
    line: 2
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require("./content/queries");
    const getState = gDebugger.DebuggerController.getState;

    Task.spawn(function* () {
      ok(!gEditor.getText().includes("\n    "),
         "The source shouldn't be pretty printed yet.");

      yield toggleBlackBoxing(gPanel);

      // Wait a tick before clicking to make sure the frontend's blackboxchange
      // handlers have finished.
      yield waitForTick();
      gDebugger.document.getElementById("pretty-print").click();
      // Make sure the text updates
      yield waitForTick();

      const source = queries.getSelectedSource(getState());
      const { text } = queries.getSourceText(getState(), source.actor);
      ok(!text.includes("\n    "));

      resumeDebuggerThenCloseAndFinish(gPanel);
    });
  });
}
