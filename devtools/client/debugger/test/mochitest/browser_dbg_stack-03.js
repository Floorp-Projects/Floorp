/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that stackframes are scrollable.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";
let framesScrollingInterval;

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab, aDebuggee, aPanel]) => {
    const tab = aTab;
    const debuggee = aDebuggee;
    const panel = aPanel;
    const gDebugger = panel.panelWin;
    const frames = gDebugger.DebuggerView.StackFrames;
    const classicFrames = gDebugger.DebuggerView.StackFramesClassicList;

    Task.spawn(function* () {
      framesScrollingInterval = window.setInterval(() => {
        frames.widget._list.scrollByIndex(-1);
      }, 100);

      yield waitForDebuggerEvents(panel, gDebugger.EVENTS.AFTER_FRAMES_REFILLED);

      is(gDebugger.gThreadClient.state, "paused",
        "Should only be getting stack frames while paused.");
      is(frames.itemCount, gDebugger.gCallStackPageSize,
        "Should have only the max limit of frames.");
      is(classicFrames.itemCount, gDebugger.gCallStackPageSize,
        "Should have only the max limit of frames in the mirrored view as well.");

      yield waitForDebuggerEvents(panel, gDebugger.EVENTS.AFTER_FRAMES_REFILLED);

      is(frames.itemCount, gDebugger.gCallStackPageSize * 2,
        "Should now have twice the max limit of frames.");
      is(classicFrames.itemCount, gDebugger.gCallStackPageSize * 2,
        "Should now have twice the max limit of frames in the mirrored view as well.");

      yield waitForDebuggerEvents(panel, gDebugger.EVENTS.AFTER_FRAMES_REFILLED);

      is(frames.itemCount, debuggee.gRecurseLimit,
        "Should have reached the recurse limit.");
      is(classicFrames.itemCount, debuggee.gRecurseLimit,
        "Should have reached the recurse limit in the mirrored view as well.");


      // Call stack frame scrolling should stop before
      // we resume the gDebugger as it could be a source of race conditions.
      window.clearInterval(framesScrollingInterval);
      resumeDebuggerThenCloseAndFinish(panel);
    });

    debuggee.gRecurseLimit = (gDebugger.gCallStackPageSize * 2) + 1;
    debuggee.recurse();
  });
}
