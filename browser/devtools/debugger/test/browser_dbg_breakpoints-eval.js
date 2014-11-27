/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test setting breakpoints on an eval script
 */

const TAB_URL = EXAMPLE_URL + "doc_script-eval.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gSources, gBreakpoints;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;

    waitForSourceShown(gPanel, "-eval.js")
      .then(run)
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function run() {
    return Task.spawn(function*() {
      let newSource = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.NEW_SOURCE);
      callInTab(gTab, "evalSource");
      yield newSource;

      yield gPanel.addBreakpoint({ actor: gSources.values[1], line: 2 });
      yield ensureThreadClientState(gPanel, "resumed");

      const paused = waitForThreadEvents(gPanel, "paused");
      callInTab(gTab, "bar");
      let frame = (yield paused).frame;
      is(frame.where.source.actor, gSources.values[1], "Should have broken on the eval'ed source");
      is(frame.where.line, 2, "Should break on line 2");

      yield resumeDebuggerThenCloseAndFinish(gPanel);
    });
  }
}
