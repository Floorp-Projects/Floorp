/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the stackframe breadcrumbs are keyboard accessible.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let gTab, gDebuggee, gPanel, gDebugger;
  let gSources, gFrames;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
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

    gDebuggee.firstCall();
  });

  function checkNavigationWhileNotFocused() {
    checkState({ frame: 3, source: 1, line: 6 });

    EventUtils.sendKey("DOWN", gDebugger);
    checkState({ frame: 3, source: 1, line: 7 });

    EventUtils.sendKey("UP", gDebugger);
    checkState({ frame: 3, source: 1, line: 6 });
  }

  function focusCurrentStackFrame() {
    EventUtils.sendMouseEvent({ type: "mousedown" },
      gFrames.selectedItem.target,
      gDebugger);
  }

  function checkNavigationWhileFocused() {
    let deferred = promise.defer();

    EventUtils.sendKey("UP", gDebugger);
    checkState({ frame: 2, source: 1, line: 6 });

    waitForSourceAndCaret(gPanel, "-01.js", 5).then(() => {
      checkState({ frame: 1, source: 0, line: 5 });

      EventUtils.sendKey("UP", gDebugger);
      checkState({ frame: 0, source: 0, line: 5 });

      waitForSourceAndCaret(gPanel, "-02.js", 6).then(() => {
        checkState({ frame: 3, source: 1, line: 6 });

        waitForSourceAndCaret(gPanel, "-01.js", 5).then(() => {
          checkState({ frame: 0, source: 0, line: 5 });
          deferred.resolve();
        });

        EventUtils.sendKey("HOME", gDebugger)
      });

      EventUtils.sendKey("END", gDebugger)
    });

    EventUtils.sendKey("UP", gDebugger)

    return deferred.promise;
  }

  function checkState({ frame, source, line }) {
    is(gFrames.selectedIndex, frame,
      "The currently selected stackframe is incorrect.");
    is(gSources.selectedIndex, source,
      "The currently selected source is incorrect.");
    ok(isCaretPos(gPanel, line),
      "The source editor caret position was incorrect.");
  }
}
