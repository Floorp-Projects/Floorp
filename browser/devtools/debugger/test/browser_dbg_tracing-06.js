/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the tracer doesn't connect to the backend when tracing is disabled.
 */

const TAB_URL = EXAMPLE_URL + "doc_tracing-01.html";
const TRACER_PREF = "devtools.debugger.tracer";

let gTab, gPanel, gDebugger;
let gOriginalPref = Services.prefs.getBoolPref(TRACER_PREF);
Services.prefs.setBoolPref(TRACER_PREF, false);

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;

    waitForSourceShown(gPanel, "code_tracing-01.js")
      .then(() => {
        ok(!gDebugger.DebuggerController.traceClient, "Should not have a trace client");
        closeDebuggerAndFinish(gPanel);
      })
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  Services.prefs.setBoolPref(TRACER_PREF, gOriginalPref);
});
