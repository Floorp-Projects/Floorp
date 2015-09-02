/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if 'break on next' functionality works from executions
 * in content that are triggered by the page.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-eval.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gSources, gBreakpoints, gTarget, gResumeButton, gResumeKey, gThreadClient;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    gTarget = gDebugger.gTarget;
    gThreadClient = gDebugger.gThreadClient;
    gResumeButton = gDebugger.document.getElementById("resume");
    gResumeKey = gDebugger.document.getElementById("resumeKey");

    waitForSourceShown(gPanel, "-eval.js")
      .then(testInterval)
      .then(testEvent)
      .then(() => closeDebuggerAndFinish(gPanel));
  });

  // Testing an interval instead of a timeout / rAF because
  // it's less likely to fail due to timing issues.  If the
  // first callback happens to fire before the break request
  // happens then we'll just get it next time.
  let testInterval = Task.async(function*() {
    info("Starting testInterval");

    yield evalInTab(gTab, `
      var interval = setInterval(function() {
        return 1+1;
      }, 100);
    `);

    let oncePaused = gTarget.once("thread-paused");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    yield oncePaused;

    let updatedFrame = yield waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
    let variables = gDebugger.DebuggerView.Variables;

    is(variables._store.length, 3, "Correct number of scopes available");
    is(variables.getScopeAtIndex(0).name, "Function scope [interval<]",
        "Paused with correct scope (0)");
    is(variables.getScopeAtIndex(1).name, "Block scope",
        "Paused with correct scope (1)");
    is(variables.getScopeAtIndex(2).name, "Global scope [Window]",
        "Paused with correct scope (2)");

    yield evalInTab(gTab, "clearInterval(interval)");
    let onceResumed = gTarget.once("thread-resumed");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    yield onceResumed;
  });

  let testEvent = Task.async(function*() {
    info("Starting testEvent");

    let oncePaused = gTarget.once("thread-paused");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    once(gDebugger.gClient, "willInterrupt").then(() => {
      generateMouseClickInTab(gTab, "content.document.querySelector('button')");
    });
    yield oncePaused;

    let updatedFrame = yield waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
    let variables = gDebugger.DebuggerView.Variables;

    is(variables._store.length, 4, "Correct number of scopes available");
    is(variables.getScopeAtIndex(0).name, "Function scope [onclick]",
        "Paused with correct scope (0)");
    is(variables.getScopeAtIndex(1).name, "With scope [HTMLButtonElement]",
        "Paused with correct scope (1)");
    is(variables.getScopeAtIndex(2).name, "With scope [HTMLDocument]",
        "Paused with correct scope (2)");
    is(variables.getScopeAtIndex(3).name, "Global scope [Window]",
        "Paused with correct scope (3)");

    let onceResumed = gTarget.once("thread-resumed");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    yield onceResumed;
  });
}
