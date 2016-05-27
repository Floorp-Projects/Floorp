/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that switching between stack frames properly sets the current debugger
 * location in the source editor.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let options = {
    source: "-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const gFrames = gDebugger.DebuggerView.StackFrames;
    const gClassicFrames = gDebugger.DebuggerView.StackFramesClassicList;

    function initialChecks() {
      is(gDebugger.gThreadClient.state, "paused",
         "Should only be getting stack frames while paused.");
      is(gFrames.itemCount, 2,
         "Should have four frames.");
      is(gClassicFrames.itemCount, 2,
         "Should also have four frames in the mirrored view.");
    }

    function testNewestFrame() {
      is(gFrames.selectedIndex, 1,
         "Newest frame should be selected by default.");
      is(gClassicFrames.selectedIndex, 0,
         "Newest frame should be selected in the mirrored view as well.");
      is(gSources.selectedIndex, 1,
         "The second source is selected in the widget.");
      ok(isCaretPos(gPanel, 6),
         "Editor caret location is correct.");
      is(gEditor.getDebugLocation(), 5,
         "Editor debug location is correct.");
    }

    function testOldestFrame() {
      const shown = waitForSourceAndCaret(gPanel, "-01.js", 5).then(() => {
        is(gFrames.selectedIndex, 0,
           "Second frame should be selected after click.");
        is(gClassicFrames.selectedIndex, 1,
           "Second frame should be selected in the mirrored view as well.");
        is(gSources.selectedIndex, 0,
           "The first source is now selected in the widget.");
        ok(isCaretPos(gPanel, 5),
           "Editor caret location is correct (3).");
        is(gEditor.getDebugLocation(), 4,
           "Editor debug location is correct.");
      });

      EventUtils.sendMouseEvent({ type: "mousedown" },
                                gDebugger.document.querySelector("#stackframe-1"),
                                gDebugger);

      return shown;
    }

    function testAfterResume() {
      let deferred = promise.defer();

      gDebugger.once(gDebugger.EVENTS.AFTER_FRAMES_CLEARED, () => {
        is(gFrames.itemCount, 0,
           "Should have no frames after resume.");
        is(gClassicFrames.itemCount, 0,
           "Should have no frames in the mirrored view as well.");
        ok(isCaretPos(gPanel, 5),
           "Editor caret location is correct after resume.");
        is(gEditor.getDebugLocation(), null,
           "Editor debug location is correct after resume.");

        deferred.resolve();
      }, true);

      gDebugger.gThreadClient.resume();

      return deferred.promise;
    }

    Task.spawn(function* () {
      yield waitForSourceAndCaretAndScopes(gPanel, "-02.js", 6);
      yield initialChecks();
      yield testNewestFrame();
      yield testOldestFrame();
      yield testAfterResume();
      closeDebuggerAndFinish(gPanel);
    });

    callInTab(gTab, "firstCall");
  });
}
