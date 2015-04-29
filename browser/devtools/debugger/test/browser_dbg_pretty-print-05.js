/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that prettifying HTML sources doesn't do anything.
 */

const TAB_URL = EXAMPLE_URL + "doc_included-script.html";

let gTab, gPanel, gDebugger;
let gEditor, gSources, gControllerSources;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gControllerSources = gDebugger.DebuggerController.SourceScripts;

    Task.spawn(function() {
      yield waitForSourceShown(gPanel, TAB_URL);

      // From this point onward, the source editor's text should never change.
      gEditor.once("change", () => {
        ok(false, "The source editor text shouldn't have changed.");
      });

      is(getSelectedSourceURL(gSources), TAB_URL,
        "The correct source is currently selected.");
      ok(gEditor.getText().includes("myFunction"),
        "The source shouldn't be pretty printed yet.");

      clickPrettyPrintButton();

      let { source } = gSources.selectedItem.attachment;
      try {
        yield gControllerSources.togglePrettyPrint(source);
        ok(false, "The promise for a prettified source should be rejected!");
      } catch ([source, error]) {
        is(error, "Can't prettify non-javascript files.",
          "The promise was correctly rejected with a meaningful message.");
      }

      let text;
      [source, text] = yield gControllerSources.getText(source);
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

function clickPrettyPrintButton() {
  gDebugger.document.getElementById("pretty-print").click();
}

function prepareDebugger(aPanel) {
  aPanel._view.Sources.preferredSource = getSourceActor(
    aPanel.panelWin.DebuggerView.Sources,
    TAB_URL
  );
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gControllerSources = null;
});
