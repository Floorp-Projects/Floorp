/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that clicking the pretty print button prettifies the source.
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
    const constants = gDebugger.require('./content/constants');
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    Task.spawn(function*() {
      yield waitForSourceShown(gPanel, "code_ugly.js");

      ok(!gEditor.getText().includes("\n  "),
         "The source shouldn't be pretty printed yet.");

      const finished = waitForSourceShown(gPanel, "code_ugly.js");
      gDebugger.document.getElementById("pretty-print").click();
      const deck = gDebugger.document.getElementById("editor-deck");
      is(deck.selectedIndex, 2, "The progress bar should be shown");
      yield finished;

      ok(gEditor.getText().includes("\n  "),
         "The source should be pretty printed.")
      is(deck.selectedIndex, 0, "The editor should be shown");

      const source = queries.getSelectedSource(getState());
      const { loading, text } = queries.getSourceText(getState(), source.actor);
      ok(!loading, "Source text is not loading");
      ok(text.includes("\n  "),
         "Subsequent calls to getText return the pretty printed source.");

      closeDebuggerAndFinish(gPanel);
    });
  });
}
