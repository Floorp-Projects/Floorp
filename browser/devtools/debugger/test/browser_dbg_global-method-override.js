/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that scripts that override properties of the global object, like
 * toString don't break the debugger. The test page used to cause the debugger
 * to throw when trying to attach to the thread actor.
 */

const TAB_URL = EXAMPLE_URL + "doc_global-method-override.html";

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    let gDebugger = aPanel.panelWin;
    ok(gDebugger, "Should have a debugger available.");
    is(gDebugger.gThreadClient.state, "attached", "Debugger should be attached.");

    closeDebuggerAndFinish(aPanel);
  });
}
