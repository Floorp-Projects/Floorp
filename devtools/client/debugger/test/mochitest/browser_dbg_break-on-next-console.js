/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if 'break on next' functionality works from executions
 * in content triggered by the console in the toolbox.
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
      .then(testConsole)
      .then(() => closeDebuggerAndFinish(gPanel));
  });

  let testConsole = Task.async(function*() {
    info("Starting testConsole");

    let oncePaused = gTarget.once("thread-paused");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    let jsterm = yield getSplitConsole();
    let executed = jsterm.execute("1+1");
    yield oncePaused;

    let updatedFrame = yield waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES);
    let variables = gDebugger.DebuggerView.Variables;

    is(variables._store.length, 2, "Correct number of scopes available");
    is(variables.getScopeAtIndex(0).name, "With scope [Object]",
        "Paused with correct scope (0)");
    is(variables.getScopeAtIndex(1).name, "Global scope [Window]",
        "Paused with correct scope (1)");

    let onceResumed = gTarget.once("thread-resumed");
    EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
    yield onceResumed;

    yield executed;
  });

  function getSplitConsole() {
    return new Promise(resolve => {
      let toolbox = gDevTools.getToolbox(gPanel.target);
      toolbox.once("webconsole-ready", () => {
        ok(toolbox.splitConsole, "Split console is shown.");
        let jsterm = toolbox.getPanel("webconsole").hud.jsterm;
        resolve(jsterm);
      });
      EventUtils.synthesizeKey("VK_ESCAPE", {}, gDebugger);
    });
  }
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
});
