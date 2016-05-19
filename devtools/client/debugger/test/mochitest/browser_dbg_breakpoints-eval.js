/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test setting breakpoints on an eval script
 */

const TAB_URL = EXAMPLE_URL + "doc_script-eval.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gSources = gDebugger.DebuggerView.Sources;
    const actions = bindActionCreators(gPanel);

    Task.spawn(function* () {
      yield waitForSourceShown(gPanel, "-eval.js");

      let newSource = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.NEW_SOURCE);
      callInTab(gTab, "evalSourceWithSourceURL");
      yield newSource;
      // Wait for it to be added to the UI
      yield waitForTick();

      const newSourceActor = getSourceActor(gSources, EXAMPLE_URL + "bar.js");
      yield actions.addBreakpoint({
        actor: newSourceActor,
        line: 2
      });
      yield ensureThreadClientState(gPanel, "resumed");

      const paused = waitForThreadEvents(gPanel, "paused");
      callInTab(gTab, "bar");
      let frame = (yield paused).frame;
      is(frame.where.source.actor, newSourceActor, "Should have broken on the eval'ed source");
      is(frame.where.line, 2, "Should break on line 2");

      yield resumeDebuggerThenCloseAndFinish(gPanel);
    });
  });
}
