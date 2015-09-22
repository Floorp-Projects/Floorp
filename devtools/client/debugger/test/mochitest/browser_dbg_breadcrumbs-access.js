/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the stackframe breadcrumbs are keyboard accessible.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gSources, gFrames;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gFrames = gDebugger.DebuggerView.StackFrames;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 6)
      .then(checkNavigationWhileNotFocused)
      .then(focusCurrentStackFrame)
      .then(checkNavigationWhileFocused)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    callInTab(gTab, "firstCall");
  });

  function checkNavigationWhileNotFocused() {
    checkState({ frame: 1, source: 1, line: 6 });

    EventUtils.sendKey("DOWN", gDebugger);
    checkState({ frame: 1, source: 1, line: 7 });

    EventUtils.sendKey("UP", gDebugger);
    checkState({ frame: 1, source: 1, line: 6 });
  }

  function focusCurrentStackFrame() {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      gFrames.selectedItem.target,
      gDebugger);
  }

  function checkNavigationWhileFocused() {
    return Task.spawn(function*() {
      yield promise.all([
        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES),
        waitForSourceAndCaret(gPanel, "-01.js", 5),
        waitForEditorLocationSet(gPanel),
        EventUtils.sendKey("UP", gDebugger)
      ]);
      checkState({ frame: 0, source: 0, line: 5 });

      yield promise.all([
        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES),
        waitForSourceAndCaret(gPanel, "-02.js", 6),
        waitForEditorLocationSet(gPanel),
        EventUtils.sendKey("END", gDebugger)
      ]);
      checkState({ frame: 1, source: 1, line: 6 });

      yield promise.all([
        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES),
        waitForSourceAndCaret(gPanel, "-01.js", 5),
        waitForEditorLocationSet(gPanel),
        EventUtils.sendKey("HOME", gDebugger)
      ]);

      checkState({ frame: 0, source: 0, line: 5 });
    });
  }

  function checkState({ frame, source, line, column }) {
    is(gFrames.selectedIndex, frame,
      "The currently selected stackframe is incorrect.");
    is(gSources.selectedIndex, source,
      "The currently selected source is incorrect.");
    ok(isCaretPos(gPanel, line, column),
      "The source editor caret position was incorrect.");
  }
}
