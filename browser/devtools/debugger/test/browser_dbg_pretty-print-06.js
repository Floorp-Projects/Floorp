/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that prettifying JS sources with type errors works as expected.
 */

const TAB_URL = EXAMPLE_URL + "doc_included-script.html";
const JS_URL = EXAMPLE_URL + "code_location-changes.js";

let gTab, gPanel, gDebugger, gClient;
let gEditor, gSources, gControllerSources, gPrettyPrinted;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gClient = gDebugger.gClient;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gControllerSources = gDebugger.DebuggerController.SourceScripts;

    // We can't feed javascript files with syntax errors to the debugger,
    // because they will never run, thus sometimes getting gc'd before the
    // debugger is opened, or even before the target finishes navigating.
    // Make the client lie about being able to parse perfectly fine code.
    gClient.request = (function (aOriginalRequestMethod) {
      return function (aPacket, aCallback) {
        if (aPacket.type == "prettyPrint") {
          gPrettyPrinted = true;
          return executeSoon(() => aCallback({ error: "prettyPrintError" }));
        }
        return aOriginalRequestMethod(aPacket, aCallback);
      };
    }(gClient.request));

    Task.spawn(function*() {
      yield waitForSourceShown(gPanel, JS_URL);

      // From this point onward, the source editor's text should never change.
      gEditor.once("change", () => {
        ok(false, "The source editor text shouldn't have changed.");
      });

      is(getSelectedSourceURL(gSources), JS_URL,
        "The correct source is currently selected.");
      ok(gEditor.getText().includes("myFunction"),
        "The source shouldn't be pretty printed yet.");

      clickPrettyPrintButton();

      let { source } = gSources.selectedItem.attachment;
      try {
        yield gControllerSources.togglePrettyPrint(source);
        ok(false, "The promise for a prettified source should be rejected!");
      } catch ([source, error]) {
        ok(error.includes("prettyPrintError"),
          "The promise was correctly rejected with a meaningful message.");
      }

      let text;
      [source, text] = yield gControllerSources.getText(source);
      is(getSelectedSourceURL(gSources), JS_URL,
        "The correct source is still selected.");
      ok(gEditor.getText().includes("myFunction"),
        "The displayed source hasn't changed.");
      ok(text.includes("myFunction"),
        "The cached source text wasn't altered in any way.");

      is(gPrettyPrinted, true,
        "The hijacked pretty print method was executed.");

      yield closeDebuggerAndFinish(gPanel);
    });
  });
}

function clickPrettyPrintButton() {
  gDebugger.document.getElementById("pretty-print").click();
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gClient = null;
  gEditor = null;
  gSources = null;
  gControllerSources = null;
  gPrettyPrinted = null;
});
