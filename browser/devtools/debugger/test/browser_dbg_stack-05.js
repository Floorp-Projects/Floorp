/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that switching between stack frames properly sets the current debugger
 * location in the source editor.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources, gFrames, gClassicFrames;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gClassicFrames = gDebugger.DebuggerView.StackFramesClassicList;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1)
      .then(initialChecks)
      .then(testNewestTwoFrames)
      .then(testOldestTwoFrames)
      .then(testAfterResume)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    gDebuggee.firstCall();
  });
}

function initialChecks() {
  is(gDebugger.gThreadClient.state, "paused",
    "Should only be getting stack frames while paused.");
  is(gFrames.itemCount, 4,
    "Should have four frames.");
  is(gClassicFrames.itemCount, 4,
    "Should also have four frames in the mirrored view.");
}

function testNewestTwoFrames() {
  let deferred = promise.defer();

  is(gFrames.selectedIndex, 3,
    "Newest frame should be selected by default.");
  is(gClassicFrames.selectedIndex, 0,
    "Newest frame should be selected in the mirrored view as well.");
  is(gSources.selectedIndex, 1,
    "The second source is selected in the widget.");
  ok(isCaretPos(gPanel, 1),
    "Editor caret location is correct (1).");

  // The editor's debug location takes a tick to update.
  executeSoon(() => {
    is(gEditor.getDebugLocation(), 0,
      "Editor debug location is correct.");

    EventUtils.sendMouseEvent({ type: "mousedown" },
      gFrames.getItemAtIndex(2).target,
      gDebugger);

    is(gFrames.selectedIndex, 2,
      "Third frame should be selected after click.");
    is(gClassicFrames.selectedIndex, 1,
      "Third frame should be selected in the mirrored view as well.");
    is(gSources.selectedIndex, 1,
      "The second source is still selected in the widget.");
    ok(isCaretPos(gPanel, 6),
      "Editor caret location is correct (2).");

    // The editor's debug location takes a tick to update.
    executeSoon(() => {
      is(gEditor.getDebugLocation(), 5,
        "Editor debug location is correct.");

      deferred.resolve();
    });
  });

  return deferred.promise;
}

function testOldestTwoFrames() {
  let deferred = promise.defer();

  waitForSourceAndCaret(gPanel, "-01.js", 1).then(waitForTick).then(() => {
    is(gFrames.selectedIndex, 1,
      "Second frame should be selected after click.");
    is(gClassicFrames.selectedIndex, 2,
      "Second frame should be selected in the mirrored view as well.");
    is(gSources.selectedIndex, 0,
      "The first source is now selected in the widget.");
    ok(isCaretPos(gPanel, 1),
      "Editor caret location is correct (3).");

    // The editor's debug location takes a tick to update.
    executeSoon(() => {
      is(gEditor.getDebugLocation(), 0,
        "Editor debug location is correct.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
        gFrames.getItemAtIndex(0).target,
        gDebugger);

      is(gFrames.selectedIndex, 0,
        "Oldest frame should be selected after click.");
      is(gClassicFrames.selectedIndex, 3,
        "Oldest frame should be selected in the mirrored view as well.");
      is(gSources.selectedIndex, 0,
        "The first source is still selected in the widget.");
      ok(isCaretPos(gPanel, 5),
        "Editor caret location is correct (4).");

      // The editor's debug location takes a tick to update.
      executeSoon(() => {
        is(gEditor.getDebugLocation(), 4,
          "Editor debug location is correct.");

        deferred.resolve();
      });
    });
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.querySelector("#stackframe-2"),
    gDebugger);

  return deferred.promise;
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

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gFrames = null;
  gClassicFrames = null;
});

