/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view remains responsive when faced with
 * huge ammounts of data.
 */

const TAB_URL = EXAMPLE_URL + "doc_large-array-buffer.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gVariables;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVariables = gDebugger.DebuggerView.Variables;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 18)
      .then(() => performTest())
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    EventUtils.sendMouseEvent({ type: "click" },
      gDebuggee.document.querySelector("button"),
      gDebuggee);
  });
}

function performTest() {

}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gVariables = null;
});
