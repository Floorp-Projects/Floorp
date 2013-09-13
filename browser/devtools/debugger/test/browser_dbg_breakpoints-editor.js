/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 723069: Test the debugger breakpoint API and connection to the
 * source editor.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let gTab, gDebuggee, gPanel, gDebugger;
  let gEditor, gSources, gBreakpoints, gBreakpointsAdded, gBreakpointsRemoving;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    gBreakpointsAdded = gBreakpoints._added;
    gBreakpointsRemoving = gBreakpoints._removing;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 6).then(performTest);
    gDebuggee.firstCall();
  });

  function performTest() {
    is(gDebugger.gThreadClient.state, "paused",
      "Should only be getting stack frames while paused.");
    is(gSources.itemCount, 2,
      "Found the expected number of sources.");
    is(gEditor.getText().indexOf("debugger"), 172,
      "The correct source was loaded initially.");
    is(gSources.selectedValue, gSources.values[1],
      "The correct source is selected.");

    is(gBreakpointsAdded.size, 0,
      "No breakpoints currently added.");
    is(gBreakpointsRemoving.size, 0,
      "No breakpoints currently being removed.");
    is(gEditor.getBreakpoints().length, 0,
      "No breakpoints currently shown in the editor.");

    ok(!gBreakpoints._getAdded({ url: "foo", line: 3 }),
      "_getAdded('foo', 3) returns falsey.");
    ok(!gBreakpoints._getRemoving({ url: "bar", line: 3 }),
      "_getRemoving('bar', 3) returns falsey.");

    is(gSources.values[1], gSources.selectedValue,
      "The second source should be currently selected.");

    info("Add the first breakpoint.");
    let location = { url: gSources.selectedValue, line: 6 };
    gEditor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddFirst);
    gPanel.addBreakpoint(location).then(onBreakpointAddFirst);
  }

  let breakpointsAdded = 0;
  let breakpointsRemoved = 0;
  let editorBreakpointChanges = 0;

  function onEditorBreakpointAddFirst(aEvent) {
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddFirst);
    editorBreakpointChanges++;

    ok(aEvent,
      "breakpoint1 added to the editor.");
    is(aEvent.added.length, 1,
      "One breakpoint added to the editor.");
    is(aEvent.removed.length, 0,
      "No breakpoint was removed from the editor.");
    is(aEvent.added[0].line, 5,
      "Editor breakpoint line is correct.");

    is(gEditor.getBreakpoints().length, 1,
      "editor.getBreakpoints().length is correct.");
  }

  function onBreakpointAddFirst(aBreakpointClient) {
    breakpointsAdded++;

    ok(aBreakpointClient,
      "breakpoint1 added, client received.");
    is(aBreakpointClient.location.url, gSources.selectedValue,
      "breakpoint1 client url is correct.");
    is(aBreakpointClient.location.line, 6,
      "breakpoint1 client line is correct.");

    ok(gBreakpoints._getAdded(aBreakpointClient.location),
      "breakpoint1 client found in the list of added breakpoints.");
    ok(!gBreakpoints._getRemoving(aBreakpointClient.location),
      "breakpoint1 client found in the list of removing breakpoints.");

    is(gBreakpointsAdded.size, 1,
      "The list of added breakpoints holds only one breakpoint.");
    is(gBreakpointsRemoving.size, 0,
      "The list of removing breakpoints holds no breakpoint.");

    gBreakpoints._getAdded(aBreakpointClient.location).then(aClient => {
      is(aClient, aBreakpointClient,
        "_getAdded() returns the correct breakpoint.");
    });

    is(gSources.values[1], gSources.selectedValue,
      "The second source should be currently selected.");

    info("Remove the first breakpoint.");
    gEditor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointRemoveFirst);
    gPanel.removeBreakpoint(aBreakpointClient.location).then(onBreakpointRemoveFirst);
  }

  function onEditorBreakpointRemoveFirst(aEvent) {
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointRemoveFirst);
    editorBreakpointChanges++;

    ok(aEvent,
      "breakpoint1 removed from the editor.");
    is(aEvent.added.length, 0,
      "No breakpoint was added to the editor.");
    is(aEvent.removed.length, 1,
      "One breakpoint was removed from the editor.");
    is(aEvent.removed[0].line, 5,
      "Editor breakpoint line is correct.");

    is(gEditor.getBreakpoints().length, 0,
      "editor.getBreakpoints().length is correct.");
  }

  function onBreakpointRemoveFirst(aLocation) {
    breakpointsRemoved++;

    ok(aLocation,
      "breakpoint1 removed");
    is(aLocation.url, gSources.selectedValue,
      "breakpoint1 removal url is correct.");
    is(aLocation.line, 6,
      "breakpoint1 removal line is correct.");

    testBreakpointAddBackground();
  }

  function testBreakpointAddBackground() {
    is(gBreakpointsAdded.size, 0,
      "No breakpoints currently added.");
    is(gBreakpointsRemoving.size, 0,
      "No breakpoints currently being removed.");
    is(gEditor.getBreakpoints().length, 0,
      "No breakpoints currently shown in the editor.");

    ok(!gBreakpoints._getAdded({ url: gSources.selectedValue, line: 6 }),
      "_getAdded('gSources.selectedValue', 6) returns falsey.");
    ok(!gBreakpoints._getRemoving({ url: gSources.selectedValue, line: 6 }),
      "_getRemoving('gSources.selectedValue', 6) returns falsey.");

    is(gSources.values[1], gSources.selectedValue,
      "The second source should be currently selected.");

    info("Add a breakpoint to the first source, which is not selected.");
    let location = { url: gSources.values[0], line: 5 };
    let options = { noEditorUpdate: true };
    gEditor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddBackgroundTrap);
    gPanel.addBreakpoint(location, options).then(onBreakpointAddBackground);
  }

  function onEditorBreakpointAddBackgroundTrap(aEvent) {
    // Trap listener: no breakpoint must be added to the editor when a
    // breakpoint is added to a source that is not currently selected.
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddBackgroundTrap);
    editorBreakpointChanges++;

    ok(false, "breakpoint2 must not be added to the editor.");
  }

  function onBreakpointAddBackground(aBreakpointClient, aResponseError) {
    breakpointsAdded++;

    ok(aBreakpointClient,
      "breakpoint2 added, client received");
    is(aBreakpointClient.location.url, gSources.values[0],
      "breakpoint2 client url is correct.");
    is(aBreakpointClient.location.line, 5,
      "breakpoint2 client line is correct.");

    ok(gBreakpoints._getAdded(aBreakpointClient.location),
      "breakpoint2 client found in the list of added breakpoints.");
    ok(!gBreakpoints._getRemoving(aBreakpointClient.location),
      "breakpoint2 client found in the list of removing breakpoints.");

    is(gBreakpointsAdded.size, 1,
      "The list of added breakpoints holds only one breakpoint.");
    is(gBreakpointsRemoving.size, 0,
      "The list of removing breakpoints holds no breakpoint.");

    gBreakpoints._getAdded(aBreakpointClient.location).then(aClient => {
      is(aClient, aBreakpointClient,
        "_getAdded() returns the correct breakpoint.");
    });

    is(gSources.values[1], gSources.selectedValue,
      "The second source should be currently selected.");

    // Remove the trap listener.
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddBackgroundTrap);

    info("Switch to the first source, which is not yet selected");
    gEditor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddSwitch);
    gEditor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onEditorTextChanged);
    gSources.selectedIndex = 0;
  }

  function onEditorBreakpointAddSwitch(aEvent) {
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddSwitch);
    editorBreakpointChanges++;

    ok(aEvent,
      "breakpoint2 added to the editor.");
    is(aEvent.added.length, 1,
      "One breakpoint added to the editor.");
    is(aEvent.removed.length, 0,
      "No breakpoint was removed from the editor.");
    is(aEvent.added[0].line, 4,
      "Editor breakpoint line is correct.");

    is(gEditor.getBreakpoints().length, 1,
      "editor.getBreakpoints().length is correct");
  }

  function onEditorTextChanged() {
    // Wait for the actual text to be shown.
    if (gEditor.getText() != gDebugger.L10N.getStr("loadingText")) {
      onEditorTextReallyChanged();
    }
  }

  function onEditorTextReallyChanged() {
    gEditor.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onEditorTextChanged);

    is(gEditor.getText().indexOf("debugger"), -1,
      "The second source is no longer displayed.");
    is(gEditor.getText().indexOf("firstCall"), 118,
      "The first source is displayed.");

    is(gSources.values[0], gSources.selectedValue,
      "The first source should be currently selected.");

    let window = gEditor.editorElement.contentWindow;
    executeSoon(() => window.mozRequestAnimationFrame(onReadyForClick));
  }

  function onReadyForClick() {
    info("Remove the second breakpoint using the mouse.");
    gEditor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointRemoveSecond);

    let iframe = gEditor.editorElement;
    let testWin = iframe.ownerDocument.defaultView;

    // Flush the layout for the iframe.
    info("rect " + iframe.contentDocument.documentElement.getBoundingClientRect());

    let utils = testWin
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);

    let lineOffset = gEditor.getLineStart(4);
    let coords = gEditor.getLocationAtOffset(lineOffset);

    let rect = iframe.getBoundingClientRect();
    let left = rect.left + 10;
    let top = rect.top + coords.y + 4;
    utils.sendMouseEventToWindow("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEventToWindow("mouseup", left, top, 0, 1, 0, false, 0, 0);
  }

  function onEditorBreakpointRemoveSecond(aEvent) {
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointRemoveSecond);
    editorBreakpointChanges++;

    ok(aEvent,
      "breakpoint2 removed from the editor.");
    is(aEvent.added.length, 0,
      "No breakpoint was added to the editor.");
    is(aEvent.removed.length, 1,
      "One breakpoint was removed from the editor.");
    is(aEvent.removed[0].line, 4,
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
    is(gBreakpointsAdded.size, 0,
      "No breakpoints currently added.");
    is(gBreakpointsRemoving.size, 0,
      "No breakpoints currently being removed.");
    is(gEditor.getBreakpoints().length, 0,
      "No breakpoints currently shown in the editor.");

    ok(!gBreakpoints._getAdded({ url: gSources.values[0], line: 5 }),
      "_getAdded('gSources.values[0]', 5) returns falsey.");
    ok(!gBreakpoints._getRemoving({ url: gSources.values[0], line: 5 }),
      "_getRemoving('gSources.values[0]', 5) returns falsey.");

    ok(!gBreakpoints._getAdded({ url: gSources.values[1], line: 6 }),
      "_getAdded('gSources.values[1]', 6) returns falsey.");
    ok(!gBreakpoints._getRemoving({ url: gSources.values[1], line: 6 }),
      "_getRemoving('gSources.values[1]', 6) returns falsey.");

    ok(!gBreakpoints._getAdded({ url: "foo", line: 3 }),
      "_getAdded('foo', 3) returns falsey.");
    ok(!gBreakpoints._getRemoving({ url: "bar", line: 3 }),
      "_getRemoving('bar', 3) returns falsey.");

    is(breakpointsAdded, 2,
      "Correct number of breakpoints have been added.");
    is(breakpointsRemoved, 1,
      "Correct number of breakpoints have been removed.");
    is(editorBreakpointChanges, 4,
      "Correct number of editor breakpoint changes.");
  }
}
