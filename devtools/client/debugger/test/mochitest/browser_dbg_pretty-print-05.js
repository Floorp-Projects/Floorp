/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that prettifying HTML sources doesn't do anything.
 */

const TAB_URL = EXAMPLE_URL + "doc_included-script.html";

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
      yield waitForSourceShown(gPanel, TAB_URL);

      // From this point onward, the source editor's text should never change.
      gEditor.once("change", () => {
        ok(false, "The source editor text shouldn't have changed.");
      });

      is(getSelectedSourceURL(gSources), TAB_URL,
        "The correct source is currently selected.");
      ok(gEditor.getText().includes("myFunction"),
        "The source shouldn't be pretty printed yet.");

      const source = queries.getSelectedSource(getState());
      try {
        yield actions.togglePrettyPrint(source);
        ok(false, "An error occurred while pretty-printing");
      }
      catch (err) {
        is(err.message, "Can't prettify non-javascript files.",
           "The promise was correctly rejected with a meaningful message.");
      }

      const { text } = yield queries.getSourceText(getState(), source.actor);
      is(getSelectedSourceURL(gSources), TAB_URL,
        "The correct source is still selected.");
      ok(gEditor.getText().includes("myFunction"),
        "The displayed source hasn't changed.");
      ok(text.includes("myFunction"),
        "The cached source text wasn't altered in any way.");

      yield closeDebuggerAndFinish(gPanel);
    });
  });
}

function prepareDebugger(aPanel) {
  aPanel._view.Sources.preferredSource = getSourceActor(
    aPanel.panelWin.DebuggerView.Sources,
    TAB_URL
  );
}

