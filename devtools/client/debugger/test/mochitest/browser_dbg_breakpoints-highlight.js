/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if breakpoints are highlighted when they should.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require('./content/queries');
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    const addBreakpoints = Task.async(function*() {
      yield actions.addBreakpoint({ actor: gSources.values[0], line: 5 });
      yield actions.addBreakpoint({ actor: gSources.values[1], line: 6 });
      yield actions.addBreakpoint({ actor: gSources.values[1], line: 7 });
      yield actions.addBreakpoint({ actor: gSources.values[1], line: 8 });
      yield actions.addBreakpoint({ actor: gSources.values[1], line: 9 })
    });

    function clickBreakpointAndCheck(aBreakpointIndex, aSourceIndex, aCaretLine) {
      let finished = waitForCaretUpdated(gPanel, aCaretLine).then(() => {
        checkHighlight(gSources.values[aSourceIndex], aCaretLine);
        checkEditorContents(aSourceIndex);

        is(queries.getSelectedSource(getState()).actor,
           gSources.items[aSourceIndex].value,
           "The currently selected source value is incorrect (1).");
        ok(isCaretPos(gPanel, aCaretLine),
           "The editor caret line and column were incorrect (1).");
      });

      EventUtils.sendMouseEvent(
        { type: "click" },
        gDebugger.document.querySelectorAll(".dbg-breakpoint")[aBreakpointIndex],
        gDebugger
      );

      return finished;
    }

    function checkHighlight(actor, line) {
      let breakpoint = gSources._selectedBreakpoint;
      let breakpointItem = gSources._getBreakpoint(breakpoint);

      is(breakpoint.location.actor, actor,
         "The currently selected breakpoint actor is incorrect.");
      is(breakpoint.location.line, line,
         "The currently selected breakpoint line is incorrect.");
      is(breakpointItem.attachment.actor, actor,
         "The selected breakpoint item's source location attachment is incorrect.");
      ok(breakpointItem.target.classList.contains("selected"),
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

    Task.spawn(function*() {
      yield waitForSourceShown(gPanel, "-01.js");

      yield addBreakpoints();
      yield clickBreakpointAndCheck(0, 0, 5);
      yield clickBreakpointAndCheck(1, 1, 6);
      yield clickBreakpointAndCheck(2, 1, 7);
      yield clickBreakpointAndCheck(3, 1, 8);
      yield clickBreakpointAndCheck(4, 1, 9);
      closeDebuggerAndFinish(gPanel);
    });
  });
}
