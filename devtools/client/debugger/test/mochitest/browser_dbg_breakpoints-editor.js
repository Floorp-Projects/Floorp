/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 723069: Test the debugger breakpoint API and connection to the
 * source editor.
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
    const constants = gDebugger.require('./content/constants');
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    Task.spawn(function*() {
      yield waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1);

      is(gDebugger.gThreadClient.state, "paused",
         "Should only be getting stack frames while paused.");
      is(queries.getSourceCount(getState()), 2,
         "Found the expected number of sources.");
      is(gEditor.getText().indexOf("debugger"), 166,
         "The correct source was loaded initially.");
      is(queries.getSelectedSource(getState()).actor, gSources.values[1],
         "The correct source is selected.");

      is(queries.getBreakpoints(getState()).length, 0,
         "No breakpoints currently added.");

      info("Add the first breakpoint.");
      gEditor.once("breakpointAdded", onEditorBreakpointAddFirst);
      let location = { actor: gSources.selectedValue, line: 6 };
      yield actions.addBreakpoint(location);
      checkFirstBreakpoint(location);

      info("Remove the first breakpoint.");
      gEditor.once("breakpointRemoved", onEditorBreakpointRemoveFirst);
      yield actions.removeBreakpoint(location);
      checkFirstBreakpointRemoved(location);
      checkBackgroundBreakpoint(yield testBreakpointAddBackground());

      info("Switch to the first source, which is not yet selected");
      gEditor.once("breakpointAdded", onEditorBreakpointAddSwitch);
      gEditor.once("change", onEditorTextChanged);
      actions.selectSource(gSources.items[0].attachment.source);
      yield waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
      onReadyForClick();
    });

    callInTab(gTab, "firstCall");

    let breakpointsAdded = 0;
    let breakpointsRemoved = 0;
    let editorBreakpointChanges = 0;

    function onEditorBreakpointAddFirst(aEvent, aLine) {
      editorBreakpointChanges++;

      ok(aEvent,
         "breakpoint1 added to the editor.");
      is(aLine, 5,
         "Editor breakpoint line is correct.");

      is(gEditor.getBreakpoints().length, 1,
         "editor.getBreakpoints().length is correct.");
    }

    function onEditorBreakpointRemoveFirst(aEvent, aLine) {
      editorBreakpointChanges++;

      ok(aEvent,
         "breakpoint1 removed from the editor.");
      is(aLine, 5,
         "Editor breakpoint line is correct.");

      is(gEditor.getBreakpoints().length, 0,
         "editor.getBreakpoints().length is correct.");
    }

    function checkFirstBreakpoint(location) {
      breakpointsAdded++;
      const bp = queries.getBreakpoint(getState(), location);

      ok(bp,
         "breakpoint1 exists");
      is(bp.location.actor, queries.getSelectedSource(getState()).actor,
         "breakpoint1 actor is correct.");
      is(bp.location.line, 6,
         "breakpoint1 line is correct.");

      is(queries.getBreakpoints(getState()).length, 1,
         "The list of added breakpoints holds only one breakpoint.");

      is(queries.getSelectedSource(getState()).actor, gSources.values[1],
         "The second source should be currently selected.");
    }

    function checkFirstBreakpointRemoved(location) {
      breakpointsRemoved++;
      const bp = queries.getBreakpoint(getState(), location);
      ok(!bp, "breakpoint1 removed");
    }

    function testBreakpointAddBackground() {
      is(queries.getBreakpoints(getState()).length, 0,
         "No breakpoints currently added.");

      is(gSources.values[1], gSources.selectedValue,
         "The second source should be currently selected.");

      info("Add a breakpoint to the first source, which is not selected.");
      let location = { actor: gSources.values[0], line: 5 };
      gEditor.on("breakpointAdded", onEditorBreakpointAddBackgroundTrap);
      return actions.addBreakpoint(location).then(() => location);
    }

    function onEditorBreakpointAddBackgroundTrap() {
      // Trap listener: no breakpoint must be added to the editor when a
      // breakpoint is added to a source that is not currently selected.
      editorBreakpointChanges++;
      ok(false, "breakpoint2 must not be added to the editor.");
    }

    function checkBackgroundBreakpoint(location) {
      breakpointsAdded++;
      const bp = queries.getBreakpoint(getState(), location);

      ok(bp,
        "breakpoint2 added, client received");
      is(bp.location.actor, gSources.values[0],
        "breakpoint2 client url is correct.");
      is(bp.location.line, 5,
        "breakpoint2 client line is correct.");

      ok(queries.getBreakpoint(getState(), bp.location),
         "breakpoint2 found in the list of added breakpoints.");

      is(queries.getBreakpoints(getState()).length, 1,
         "The list of added breakpoints holds only one breakpoint.");

      is(queries.getSelectedSource(getState()).actor, gSources.values[1],
         "The second source should be currently selected.");

      // Remove the trap listener.
      gEditor.off("breakpointAdded", onEditorBreakpointAddBackgroundTrap);
    }

    function onEditorBreakpointAddSwitch(aEvent, aLine) {
      editorBreakpointChanges++;

      ok(aEvent,
        "breakpoint2 added to the editor.");
      is(aLine, 4,
        "Editor breakpoint line is correct.");

      is(gEditor.getBreakpoints().length, 1,
        "editor.getBreakpoints().length is correct");
    }

    function onEditorTextChanged() {
      // Wait for the actual text to be shown.
      if (gEditor.getText() == gDebugger.L10N.getStr("loadingText"))
        return void gEditor.once("change", onEditorTextChanged);

      is(gEditor.getText().indexOf("debugger"), -1,
        "The second source is no longer displayed.");
      is(gEditor.getText().indexOf("firstCall"), 118,
        "The first source is displayed.");

      is(gSources.values[0], gSources.selectedValue,
        "The first source should be currently selected.");
    }

    function onReadyForClick() {
      info("Remove the second breakpoint using the mouse.");
      gEditor.once("breakpointRemoved", onEditorBreakpointRemoveSecond);

      let iframe = gEditor.container;
      let testWin = iframe.ownerDocument.defaultView;

      // Flush the layout for the iframe.
      info("rect " + iframe.contentDocument.documentElement.getBoundingClientRect());

      let utils = testWin
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils);

      let coords = gEditor.getCoordsFromPosition({ line: 4, ch: 0 });
      let rect = iframe.getBoundingClientRect();
      let left = rect.left + 10;
      let top = rect.top + coords.top + 4;
      utils.sendMouseEventToWindow("mousedown", left, top, 0, 1, 0, false, 0, 0);
      utils.sendMouseEventToWindow("mouseup", left, top, 0, 1, 0, false, 0, 0);
    }

    function onEditorBreakpointRemoveSecond(aEvent, aLine) {
      editorBreakpointChanges++;

      ok(aEvent,
        "breakpoint2 removed from the editor.");
      is(aLine, 4,
        "Editor breakpoint line is correct.");

      is(gEditor.getBreakpoints().length, 0,
        "editor.getBreakpoints().length is correct.");

      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.AFTER_FRAMES_CLEARED).then(() => {
        finalCheck();
        closeDebuggerAndFinish(gPanel);
      });

      gDebugger.gThreadClient.resume();
    }

    function finalCheck() {
      is(queries.getBreakpoints(getState()).length, 0,
         "No breakpoints currently added.");

      is(breakpointsAdded, 2,
         "Correct number of breakpoints have been added.");
      is(breakpointsRemoved, 1,
         "Correct number of breakpoints have been removed.");
      is(editorBreakpointChanges, 4,
         "Correct number of editor breakpoint changes.");
    }
  });
}
