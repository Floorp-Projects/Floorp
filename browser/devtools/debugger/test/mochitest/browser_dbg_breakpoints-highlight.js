/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if breakpoints are highlighted when they should.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gEditor, gSources;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceShown(gPanel, "-01.js")
      .then(addBreakpoints)
      .then(() => clickBreakpointAndCheck(0, 0, 5))
      .then(() => clickBreakpointAndCheck(1, 1, 6))
      .then(() => clickBreakpointAndCheck(2, 1, 7))
      .then(() => clickBreakpointAndCheck(3, 1, 8))
      .then(() => clickBreakpointAndCheck(4, 1, 9))
      .then(() => ensureThreadClientState(gPanel, "resumed"))
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function addBreakpoints() {
    return promise.resolve(null)
      .then(() => initialChecks(0, 1))
      .then(() => gPanel.addBreakpoint({ actor: gSources.values[0], line: 5 }))
      .then(() => initialChecks(0, 5))
      .then(() => gPanel.addBreakpoint({ actor: gSources.values[1], line: 6 }))
      .then(() => waitForSourceShown(gPanel, "-02.js"))
      .then(() => waitForCaretUpdated(gPanel, 6))
      .then(() => initialChecks(1, 6))
      .then(() => gPanel.addBreakpoint({ actor: gSources.values[1], line: 7 }))
      .then(() => initialChecks(1, 7))
      .then(() => gPanel.addBreakpoint({ actor: gSources.values[1], line: 8 }))
      .then(() => initialChecks(1, 8))
      .then(() => gPanel.addBreakpoint({ actor: gSources.values[1], line: 9 }))
      .then(() => initialChecks(1, 9));
  }

  function initialChecks(aSourceIndex, aCaretLine) {
    checkEditorContents(aSourceIndex);

    is(gSources.selectedLabel, gSources.items[aSourceIndex].label,
      "The currently selected source label is incorrect (0).");
    is(gSources.selectedValue, gSources.items[aSourceIndex].value,
      "The currently selected source value is incorrect (0).");
    ok(isCaretPos(gPanel, aCaretLine),
      "The editor caret line and column were incorrect (0).");
  }

  function clickBreakpointAndCheck(aBreakpointIndex, aSourceIndex, aCaretLine) {
    let finished = waitForCaretUpdated(gPanel, aCaretLine).then(() => {
      checkHighlight(gSources.values[aSourceIndex], aCaretLine);
      checkEditorContents(aSourceIndex);

      is(gSources.selectedLabel, gSources.items[aSourceIndex].label,
        "The currently selected source label is incorrect (1).");
      is(gSources.selectedValue, gSources.items[aSourceIndex].value,
        "The currently selected source value is incorrect (1).");
      ok(isCaretPos(gPanel, aCaretLine),
        "The editor caret line and column were incorrect (1).");
    });

    EventUtils.sendMouseEvent({ type: "click" },
      gDebugger.document.querySelectorAll(".dbg-breakpoint")[aBreakpointIndex],
      gDebugger);

    return finished;
  }

  function checkHighlight(aActor, aLine) {
    is(gSources._selectedBreakpointItem, gSources.getBreakpoint({ actor: aActor, line: aLine }),
      "The currently selected breakpoint item is incorrect.");
    is(gSources._selectedBreakpointItem.attachment.actor, aActor,
      "The selected breakpoint item's source location attachment is incorrect.");
    is(gSources._selectedBreakpointItem.attachment.line, aLine,
      "The selected breakpoint item's source line number is incorrect.");
    ok(gSources._selectedBreakpointItem.target.classList.contains("selected"),
      "The selected breakpoint item's target should have a selected class.");
  }

  function checkEditorContents(aSourceIndex) {
    if (aSourceIndex == 0) {
      is(gEditor.getText().indexOf("firstCall"), 118,
        "The first source is correctly displayed.");
    } else {
      is(gEditor.getText().indexOf("debugger"), 166,
        "The second source is correctly displayed.");
    }
  }
}
