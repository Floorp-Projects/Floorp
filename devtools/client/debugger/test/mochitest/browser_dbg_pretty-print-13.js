/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that clicking the pretty print button prettifies the source, even
 * when the source URL does not end in ".js", but the content type is
 * JavaScript.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print-3.html";

function test() {
  // Wait for debugger panel to be fully set and break on debugger statement
  let options = {
    source: EXAMPLE_URL + "code_ugly-8",
    line: 2
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require("./content/queries");
    const constants = gDebugger.require("./content/constants");
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    Task.spawn(function* () {
      ok(!gEditor.getText().includes("\n  "),
         "The source shouldn't be pretty printed yet.");

      const finished = waitForSourceShown(gPanel, "code_ugly-8");
      gDebugger.document.getElementById("pretty-print").click();
      const deck = gDebugger.document.getElementById("editor-deck");
      is(deck.selectedIndex, 2, "The progress bar should be shown");
      yield finished;

      ok(gEditor.getText().includes("\n  "),
         "The source should be pretty printed.");
      is(deck.selectedIndex, 0, "The editor should be shown");

      const source = queries.getSelectedSource(getState());
      const { text } = queries.getSourceText(getState(), source.actor);
      ok(text.includes("\n  "),
         "Subsequent calls to getText return the pretty printed source.");

      resumeDebuggerThenCloseAndFinish(gPanel);
    })
  });
}
