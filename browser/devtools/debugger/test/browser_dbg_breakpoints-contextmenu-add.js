/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test adding breakpoints from the source editor context menu
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let gTab, gDebuggee, gPanel, gDebugger;
  let gEditor, gSources, gContextMenu, gBreakpoints, gBreakpointsAdded;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    gBreakpointsAdded = gBreakpoints._added;
    gContextMenu = gDebugger.document.getElementById("sourceEditorContextMenu");

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1)
      .then(performTest)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    gDebuggee.firstCall();
  });

  function performTest() {
    is(gDebugger.gThreadClient.state, "paused",
       "Should only be getting stack frames while paused.");
    is(gSources.itemCount, 2,
       "Found the expected number of sources.");
    isnot(gEditor.getText().indexOf("debugger"), -1,
       "The correct source was loaded initially.");
    is(gSources.selectedValue, gSources.values[1],
       "The correct source is selected.");

    ok(gContextMenu,
       "The source editor's context menupopup is available.");

    gEditor.focus();
    gEditor.setSelection({ line: 1, ch: 0 }, { line: 1, ch: 10 });

    return testAddBreakpoint().then(testAddConditionalBreakpoint);
  }

  function testAddBreakpoint() {
    gContextMenu.openPopup(gEditor.container, "overlap", 0, 0, true, false);
    gEditor.emit("gutterClick", 6, 2);

    return once(gContextMenu, "popupshown").then(() => {
      is(gBreakpointsAdded.size, 0, "no breakpoints added");

      let cmd = gContextMenu.querySelector('menuitem[command=addBreakpointCommand]');
      let bpShown = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_SHOWN_IN_EDITOR);
      EventUtils.synthesizeMouseAtCenter(cmd, {}, gDebugger);
      return bpShown;
    }).then(() => {
      is(gBreakpointsAdded.size, 1,
         "1 breakpoint correctly added");
      is(gEditor.getBreakpoints().length, 1,
         "1 breakpoint currently shown in the editor.");
      ok(gBreakpoints._getAdded({ url: gSources.values[1], line: 7 }),
         "Breakpoint on line 7 exists");
    });
  }

  function testAddConditionalBreakpoint() {
    gContextMenu.openPopup(gEditor.container, "overlap", 0, 0, true, false);
    gEditor.emit("gutterClick", 7, 2);

    return once(gContextMenu, "popupshown").then(() => {
      is(gBreakpointsAdded.size, 1,
         "1 breakpoint correctly added");

      let cmd = gContextMenu.querySelector('menuitem[command=addConditionalBreakpointCommand]');
      let bpShown = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.CONDITIONAL_BREAKPOINT_POPUP_SHOWING);
      EventUtils.synthesizeMouseAtCenter(cmd, {}, gDebugger);
      return bpShown;
    }).then(() => {
      is(gBreakpointsAdded.size, 2,
         "2 breakpoints correctly added");
      is(gEditor.getBreakpoints().length, 2,
         "2 breakpoints currently shown in the editor.");
      ok(gBreakpoints._getAdded({ url: gSources.values[1], line: 8 }),
         "Breakpoint on line 8 exists");
    });
  }
}
