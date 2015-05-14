/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure anonymous eval scripts can still break with a `debugger`
 * statement
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

    return Task.spawn(function*() {
      yield waitForSourceShown(gPanel, "-eval.js");
      is(gSources.values.length, 1, "Should have 1 source");

      let hasFrames = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.AFTER_FRAMES_REFILLED);
      callInTab(gTab, "evalSourceWithDebugger");
      yield hasFrames;

      is(gSources.values.length, 2, "Should have 2 sources");

      let item = gSources.getItemForAttachment(e => e.label.indexOf("SCRIPT") === 0);
      ok(item, "Source label is incorrect.");
      is(item.attachment.group, gDebugger.L10N.getStr('anonymousSourcesLabel'),
         'Source group is incorrect');

      yield resumeDebuggerThenCloseAndFinish(gPanel);
    });
  });
}
