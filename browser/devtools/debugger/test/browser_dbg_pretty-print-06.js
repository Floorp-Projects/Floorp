/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that prettifying JS sources with type errors works as expected.
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

      let reloaded = promise.all([
        waitForDebuggerEvents(aPanel, gDebugger.EVENTS.NEW_SOURCE),
        waitForDebuggerEvents(aPanel, gDebugger.EVENTS.SOURCES_ADDED),
        waitForDebuggerEvents(aPanel, gDebugger.EVENTS.SOURCE_SHOWN)
      ]);

      is(gSources.itemCount, 1,
        "There should be 1 item displayed in the sources view before reloading.");
      is(gSources.values[0], TAB_URL,
        "The only source shown should be the html page.");

      gDebugger.gClient.activeTab.reload();
      yield reloaded;

      is(gSources.itemCount, 2,
        "There should be 2 items displayed in the sources view after reloading.");
      is(gSources.values[0], JS_URL,
        "The first source shown should be the js file with the syntax error.");
      is(gSources.values[1], TAB_URL,
        "The second source shown should be the html page.");

      let changed = waitForSourceShown(gPanel, JS_URL);
      gSources.selectedLabel = JS_URL;
      yield changed;

      // From this point onward, the source editor's text should never change.
      once(gEditor, SourceEditor.EVENTS.TEXT_CHANGED).then(() => {
        ok(false, "The source editor text shouldn't have changed.");
      });

      is(gSources.selectedValue, JS_URL,
        "The correct source is currently selected.");
      ok(gEditor.getText().contains("pineapple"),
        "The source shouldn't be pretty printed yet.");

      clickPrettyPrintButton();

      let { source } = gSources.selectedItem.attachment;
      try {
        yield gControllerSources.prettyPrint(source);
        ok(false, "The promise for a prettified source should be rejected!");
      } catch ([source, error]) {
        ok(error.contains("SyntaxError: missing ; before statement"),
          "The promise was correctly rejected with a SyntaxError message.");
      }

      let [source, text] = yield gControllerSources.getText(source);
      is(gSources.selectedValue, JS_URL,
        "The correct source is still selected.");
      ok(gEditor.getText().contains("pineapple"),
        "The displayed source hasn't changed.");
      ok(text.contains("pineapple"),
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
