/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the tracer doesn't connect to the backend when tracing is disabled.
 */

const TAB_URL = EXAMPLE_URL + "doc_tracing-01.html";

let gTab, gDebuggee, gPanel, gDebugger;

function test() {
  SpecialPowers.pushPrefEnv({'set': [["devtools.debugger.tracer", false]]}, () => {
    initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
      gTab = aTab;
      gDebuggee = aDebuggee;
      gPanel = aPanel;
      gDebugger = gPanel.panelWin;

      waitForSourceShown(gPanel, "code_tracing-01.js")
        .then(() => {
          ok(!gDebugger.DebuggerController.traceClient, "Should not have a trace client");
        })
        .then(() => {
          const deferred = promise.defer();
          SpecialPowers.popPrefEnv(deferred.resolve);
          return deferred.promise;
        })
        .then(() => closeDebuggerAndFinish(gPanel))
        .then(null, aError => {
          ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
        });
    });
  });
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
});
