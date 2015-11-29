/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that we disable the pretty print button for black boxed sources,
 * and that clicking it doesn't do anything.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require('./content/queries');
    const getState = gDebugger.DebuggerController.getState;

    Task.spawn(function*() {
      yield waitForSourceShown(gPanel, "code_ugly.js");

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
      closeDebuggerAndFinish(gPanel);
    });
  });
}
