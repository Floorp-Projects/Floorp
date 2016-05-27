/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if 'break on next' functionality works from executions
 * in content that are triggered by the page.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-eval.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gSources, gBreakpoints, gTarget, gResumeButton, gResumeKey, gThreadClient;

  const options = {
    source: "-eval.js",
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

    testInterval()
      .then(testEvent)
      .then(() => closeDebuggerAndFinish(gPanel));
  });

  // Testing an interval instead of a timeout / rAF because
  // it's less likely to fail due to timing issues.  If the
  // first callback happens to fire before the break request
  // happens then we'll just get it next time.
  let testInterval = Task.async(function* () {
    info("Starting testInterval");

    yield evalInTab(gTab, `
      var interval = setInterval(function() {
        return 1+1;
      }, 100);
    `);

    let oncePaused = gTarget.once("thread-paused");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    yield oncePaused;

    yield waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
    let variables = gDebugger.DebuggerView.Variables;

    is(variables._store.length, 4, "Correct number of scopes available");
    is(variables.getScopeAtIndex(0).name, "Function scope [interval<]",
        "Paused with correct scope (0)");
    is(variables.getScopeAtIndex(1).name, "Block scope",
        "Paused with correct scope (1)");
    is(variables.getScopeAtIndex(2).name, "Block scope",
        "Paused with correct scope (2)");
    is(variables.getScopeAtIndex(3).name, "Global scope [Window]",
        "Paused with correct scope (3)");

    yield evalInTab(gTab, "clearInterval(interval)");
    let onceResumed = gTarget.once("thread-resumed");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    yield onceResumed;
  });

  let testEvent = Task.async(function* () {
    info("Starting testEvent");

    let oncePaused = gTarget.once("thread-paused");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    once(gDebugger.gClient, "willInterrupt").then(() => {
      generateMouseClickInTab(gTab, "content.document.querySelector('button')");
    });
    yield oncePaused;

    yield waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
    let variables = gDebugger.DebuggerView.Variables;

    is(variables._store.length, 6, "Correct number of scopes available");
    is(variables.getScopeAtIndex(0).name, "Function scope [onclick]",
        "Paused with correct scope (0)");
    // Non-syntactic lexical scope introduced by non-syntactic scope chain.
    is(variables.getScopeAtIndex(1).name, "Block scope",
        "Paused with correct scope (1)");
    is(variables.getScopeAtIndex(2).name, "With scope [HTMLButtonElement]",
        "Paused with correct scope (2)");
    is(variables.getScopeAtIndex(3).name, "With scope [HTMLDocument]",
        "Paused with correct scope (3)");
    // Global lexical scope.
    is(variables.getScopeAtIndex(4).name, "Block scope",
        "Paused with correct scope (4)");
    is(variables.getScopeAtIndex(5).name, "Global scope [Window]",
        "Paused with correct scope (5)");

    let onceResumed = gTarget.once("thread-resumed");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    yield onceResumed;
  });
}
