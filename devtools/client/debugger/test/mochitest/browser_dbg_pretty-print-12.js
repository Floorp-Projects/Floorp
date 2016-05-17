/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that we don't leave the pretty print button checked when we fail to
 * pretty print a source (because it isn't a JS file, for example).
 */

const TAB_URL = EXAMPLE_URL + "doc_blackboxing.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
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
      yield waitForSourceShown(gPanel, "");
      const source = getSourceForm(gSources, TAB_URL);
      let shown = ensureSourceIs(gPanel, TAB_URL, true);
      actions.selectSource(source);
      yield shown;

      try {
        yield actions.togglePrettyPrint(source);
        ok(false, "An error occurred while pretty-printing");
      }
      catch (err) {
        is(err.message, "Can't prettify non-javascript files.",
           "The promise was correctly rejected with a meaningful message.");
      }

      is(gDebugger.document.getElementById("pretty-print").checked, false,
         "The button shouldn't be checked after trying to pretty print a non-js file.");

      closeDebuggerAndFinish(gPanel);
    });
  });
}
