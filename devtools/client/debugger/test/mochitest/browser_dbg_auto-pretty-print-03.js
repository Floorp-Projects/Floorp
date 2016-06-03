/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * If auto pretty-printing it enabled, make sure that if
 * pretty-printing fails that it still properly shows the original
 * source.
 */

const TAB_URL = EXAMPLE_URL + "doc_auto-pretty-print-02.html";

var FIRST_SOURCE = EXAMPLE_URL + "code_ugly-6.js";
var SECOND_SOURCE = EXAMPLE_URL + "code_ugly-7.js";

function test() {
  let options = {
    source: FIRST_SOURCE,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab, aDebuggee, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;

    const gController = gDebugger.DebuggerController;
    const gEditor = gDebugger.DebuggerView.editor;
    const constants = gDebugger.require("./content/constants");
    const queries = gDebugger.require("./content/queries");
    const actions = bindActionCreators(gPanel);

    Task.spawn(function* () {
      const secondSource = queries.getSourceByURL(gController.getState(), SECOND_SOURCE);
      actions.selectSource(secondSource);

      // It should be showing the loading text
      is(gEditor.getText(), gDebugger.DebuggerView._loadingText,
        "The editor loading text is shown");

      gController.dispatch({
        type: constants.TOGGLE_PRETTY_PRINT,
        status: "error",
        source: secondSource,
      });

      is(gEditor.getText(), gDebugger.DebuggerView._loadingText,
        "The editor loading text is shown");

      yield waitForSourceShown(gPanel, SECOND_SOURCE);

      ok(gEditor.getText().includes("function foo"),
         "The second source is shown");

      yield closeDebuggerAndFinish(gPanel);
    });
  });
}
