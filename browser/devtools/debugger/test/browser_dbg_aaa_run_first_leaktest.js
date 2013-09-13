/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests if the debugger leaks on initialization and sudden destruction.
 * You can also use this initialization format as a template for other tests.
 * If leaks happen here, there's something very, very fishy going on.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  // Wait longer for this very simple test that comes first, to make sure that
  // GC from previous tests does not interfere with the debugger suite.
  requestLongerTimeout(2);

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    ok(aTab, "Should have a tab available.");
    ok(aDebuggee, "Should have a debuggee available.");
    ok(aPanel, "Should have a debugger pane available.");

    waitForSourceAndCaretAndScopes(aPanel, "-02.js", 6).then(() => {
      resumeDebuggerThenCloseAndFinish(aPanel);
    });

    aDebuggee.firstCall();
  });
}
