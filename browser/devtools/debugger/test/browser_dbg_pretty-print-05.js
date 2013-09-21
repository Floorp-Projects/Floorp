/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that prettifying HTML sources doesn't do anything.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print-02.html";
const JS_URL = EXAMPLE_URL + "code_test-syntax-error.js";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources, gControllerSources;

function test() {
  // A source with a syntax error will be loaded.
  ignoreAllUncaughtExceptions();

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gControllerSources = gDebugger.DebuggerController.SourceScripts;

    Task.spawn(function() {
      yield waitForSourceShown(gPanel, TAB_URL);

      // From this point onward, the source editor's text should never change.
      once(gEditor, SourceEditor.EVENTS.TEXT_CHANGED).then(() => {
        ok(false, "The source editor text shouldn't have changed.");
      });

      is(gSources.selectedValue, TAB_URL,
        "The correct source is currently selected.");
      ok(gEditor.getText().contains("banana"),
        "The source shouldn't be pretty printed yet.");

      clickPrettyPrintButton();

      let { source } = gSources.selectedItem.attachment;
      try {
        yield gControllerSources.prettyPrint(source);
        ok(false, "The promise for a prettified source should be rejected!");
      } catch ([source, error]) {
        is(error, "Can't prettify non-javascript files.",
          "The promise was correctly rejected with a meaningful message.");
      }

      let [source, text] = yield gControllerSources.getText(source);
      is(gSources.selectedValue, TAB_URL,
        "The correct source is still selected.");
      ok(gEditor.getText().contains("banana"),
        "The displayed source hasn't changed.");
      ok(text.contains("banana"),
        "The cached source text wasn't altered in any way.");

      yield closeDebuggerAndFinish(gPanel);
    });
  });
}

function clickPrettyPrintButton() {
  EventUtils.sendMouseEvent({ type: "click" },
    gDebugger.document.getElementById("pretty-print"),
    gDebugger);
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gControllerSources = null;
});
