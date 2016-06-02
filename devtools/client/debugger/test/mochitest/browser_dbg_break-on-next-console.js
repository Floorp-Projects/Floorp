/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if 'break on next' functionality works from executions
 * in content triggered by the console in the toolbox.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-eval.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gSources, gBreakpoints, gTarget, gResumeButton, gResumeKey, gThreadClient;

  let options = {
    source: EXAMPLE_URL + "code_script-eval.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gTarget = gDebugger.gTarget;
    gThreadClient = gDebugger.gThreadClient;
    gResumeButton = gDebugger.document.getElementById("resume");
    gResumeKey = gDebugger.document.getElementById("resumeKey");

    testConsole()
      .then(() => closeDebuggerAndFinish(gPanel));
  });

  let testConsole = Task.async(function* () {
    info("Starting testConsole");

    let oncePaused = gTarget.once("thread-paused");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    let jsterm = yield getSplitConsole(gDevTools.getToolbox(gPanel.target));
    let executed = jsterm.execute("1+1");
    yield oncePaused;

    let updatedFrame = yield waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
    let variables = gDebugger.DebuggerView.Variables;

    is(variables._store.length, 3, "Correct number of scopes available");
    is(variables.getScopeAtIndex(0).name, "With scope [Object]",
        "Paused with correct scope (0)");
    is(variables.getScopeAtIndex(1).name, "Block scope",
        "Paused with correct scope (1)");
    is(variables.getScopeAtIndex(2).name, "Global scope [Window]",
        "Paused with correct scope (2)");

    let onceResumed = gTarget.once("thread-resumed");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    yield onceResumed;

    yield executed;
  });
}
