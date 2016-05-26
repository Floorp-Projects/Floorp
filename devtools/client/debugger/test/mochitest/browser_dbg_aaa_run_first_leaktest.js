/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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

  let options = {
    source: "-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    ok(aTab, "Should have a tab available.");
    ok(aPanel, "Should have a debugger pane available.");

    waitForSourceAndCaretAndScopes(aPanel, "-02.js", 1).then(() => {
      resumeDebuggerThenCloseAndFinish(aPanel);
    });

    callInTab(aTab, "firstCall");
  });
}
