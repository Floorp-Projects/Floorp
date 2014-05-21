/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that debuggee scripts are terminated on tab closure.
 */

const TAB_URL = EXAMPLE_URL + "doc_terminate-on-tab-close.html";

let gTab, gDebuggee, gDebugger, gPanel;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;

    testTerminate();
  });
}

function testTerminate() {
  gDebugger.gThreadClient.addOneTimeListener("paused", () => {
    resumeDebuggerThenCloseAndFinish(gPanel).then(function () {
      ok(true, "should not throw after this point");
    });
  });

  gDebuggee.debuggerThenThrow();
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
});
