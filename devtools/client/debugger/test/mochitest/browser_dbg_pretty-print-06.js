/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that prettifying JS sources with type errors works as expected.
 */

const TAB_URL = EXAMPLE_URL + "doc_included-script.html";
const JS_URL = EXAMPLE_URL + "code_location-changes.js";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gClient = gDebugger.gClient;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require("./content/queries");
    const constants = gDebugger.require("./content/constants");
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;
    let gPrettyPrinted = false;

    // We can't feed javascript files with syntax errors to the debugger,
    // because they will never run, thus sometimes getting gc'd before the
    // debugger is opened, or even before the target finishes navigating.
    // Make the client lie about being able to parse perfectly fine code.
    gClient.request = (function (aOriginalRequestMethod) {
      return function (aPacket, aCallback) {
        if (aPacket.type == "prettyPrint") {
          gPrettyPrinted = true;
          return promise.reject({ error: "prettyPrintError" });
        }
        return aOriginalRequestMethod(aPacket, aCallback);
      };
    }(gClient.request));

    Task.spawn(function* () {
      yield waitForSourceShown(gPanel, JS_URL);

      // From this point onward, the source editor's text should never change.
      gEditor.once("change", () => {
        ok(false, "The source editor text shouldn't have changed.");
      });

      is(getSelectedSourceURL(gSources), JS_URL,
        "The correct source is currently selected.");
      ok(gEditor.getText().includes("myFunction"),
        "The source shouldn't be pretty printed yet.");

      const source = queries.getSelectedSource(getState());
      try {
        yield actions.togglePrettyPrint(source);
        ok(false, "The promise for a prettified source should be rejected!");
      } catch (error) {
        ok(error.error, "Error came from a RDP request");
        ok(error.error.includes("prettyPrintError"),
          "The promise was correctly rejected with a meaningful message.");
      }

      const { text } = yield queries.getSourceText(getState(), source.actor);
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
