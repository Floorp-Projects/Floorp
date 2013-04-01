/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 723069: test the debugger breakpoint API and connection to the source
 * editor.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

let gPane = null;
let gTab = null;
let gDebuggee = null;
let gDebugger = null;
let gEditor = null;
let gSources = null;
let gBreakpoints = null;

function test()
{
  let scriptShown = false;
  let framesAdded = false;
  let resumed = false;
  let testStarted = false;

  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    resumed = true;

    gDebugger.addEventListener("Debugger:SourceShown", onSourceShown);

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      framesAdded = true;
      executeSoon(startTest);
    });

    executeSoon(function() {
      gDebuggee.firstCall();
    });
  });

  function onSourceShown(aEvent)
  {
    scriptShown = aEvent.detail.url.indexOf("-02.js") != -1;
    executeSoon(startTest);
  }

  function startTest()
  {
    if (scriptShown && framesAdded && !testStarted) {
      gDebugger.removeEventListener("Debugger:SourceShown", onSourceShown);
      testStarted = true;
      Services.tm.currentThread.dispatch({ run: performTest }, 0);
    }
  }

  function performTest()
  {
    gSources = gDebugger.DebuggerView.Sources;
    gEditor = gDebugger.editor;
    gBreakpoints = gPane.getAllBreakpoints();

    is(gDebugger.DebuggerController.activeThread.state, "paused",
      "Should only be getting stack frames while paused.");

    is(gSources.itemCount, 2,
      "Found the expected number of scripts.");

    isnot(gEditor.getText().indexOf("debugger"), -1,
      "The correct script was loaded initially.");

    isnot(gSources.selectedValue, gSources.values[0],
      "The correct script is selected");

    is(Object.keys(gBreakpoints), 0, "no breakpoints");
    ok(!gPane.getBreakpoint("foo", 3), "getBreakpoint('foo', 3) returns falsey");
    is(gEditor.getBreakpoints().length, 0, "no breakpoints in the editor");

    gEditor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddFirst);
    executeSoon(function() {
      gPane.addBreakpoint({url: gSources.selectedValue, line: 6}, onBreakpointAddFirst);
    });
  }

  let breakpointsAdded = 0;
  let breakpointsRemoved = 0;
  let editorBreakpointChanges = 0;

  function onEditorBreakpointAddFirst(aEvent)
  {
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddFirst);
    editorBreakpointChanges++;

    ok(aEvent, "breakpoint1 added to the editor");
    is(aEvent.added.length, 1, "one breakpoint added to the editor");
    is(aEvent.removed.length, 0, "no breakpoint was removed from the editor");
    is(aEvent.added[0].line, 5, "editor breakpoint line is correct");

    is(gEditor.getBreakpoints().length, 1,
      "editor.getBreakpoints().length is correct");
  }

  function onBreakpointAddFirst(aBreakpointClient, aResponseError)
  {
    breakpointsAdded++;

    ok(aBreakpointClient, "breakpoint1 added, client received");
    ok(!aResponseError, "breakpoint1 added without errors");
    is(aBreakpointClient.location.url, gSources.selectedValue,
       "breakpoint1 client url is correct");
    is(aBreakpointClient.location.line, 6,
       "breakpoint1 client line is correct");

    executeSoon(function() {
      ok(aBreakpointClient.actor in gBreakpoints,
        "breakpoint1 client found in the list of debugger breakpoints");
      is(Object.keys(gBreakpoints).length, 1,
        "the list of debugger breakpoints holds only one breakpoint");
      is(gPane.getBreakpoint(gSources.selectedValue, 6), aBreakpointClient,
        "getBreakpoint returns the correct breakpoint");

      info("remove the first breakpoint");
      gEditor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointRemoveFirst);
      gPane.removeBreakpoint(aBreakpointClient, onBreakpointRemoveFirst);
    });
  }

  function onBreakpointRemoveFirst(aLocation)
  {
    breakpointsRemoved++;

    ok(aLocation, "breakpoint1 removed");
    is(aLocation.url, gSources.selectedValue, "breakpoint1 remove: url is correct");
    is(aLocation.line, 6, "breakpoint1 remove: line is correct");

    executeSoon(testBreakpointAddBackground);
  }

  function onEditorBreakpointRemoveFirst(aEvent)
  {
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointRemoveFirst);
    editorBreakpointChanges++;

    ok(aEvent, "breakpoint1 removed from the editor");
    is(aEvent.added.length, 0, "no breakpoint was added to the editor");
    is(aEvent.removed.length, 1, "one breakpoint was removed from the editor");
    is(aEvent.removed[0].line, 5, "editor breakpoint line is correct");

    is(gEditor.getBreakpoints().length, 0,
      "editor.getBreakpoints().length is correct");
  }

  function testBreakpointAddBackground()
  {
    info("add a breakpoint to the second script which is not selected");

    is(Object.keys(gBreakpoints).length, 0,
      "no breakpoints in the debugger");
    ok(!gPane.getBreakpoint(gSources.selectedValue, 6),
      "getBreakpoint(selectedScript, 6) returns no breakpoint");
    isnot(gSources.values[0], gSources.selectedValue,
      "first script location is not the currently selected script");

    gEditor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddBackgroundTrap);
    gPane.addBreakpoint({url: gSources.values[0], line: 5}, onBreakpointAddBackground);
  }

  function onEditorBreakpointAddBackgroundTrap(aEvent)
  {
    // Trap listener: no breakpoint must be added to the editor when a
    // breakpoint is added to a script that is not currently selected.
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddBackgroundTrap);
    editorBreakpointChanges++;
    ok(false, "breakpoint2 must not be added to the editor");
  }

  function onBreakpointAddBackground(aBreakpointClient, aResponseError)
  {
    breakpointsAdded++;

    ok(aBreakpointClient, "breakpoint2 added, client received");
    ok(!aResponseError, "breakpoint2 added without errors");
    is(aBreakpointClient.location.url, gSources.values[0],
      "breakpoint2 client url is correct");
    is(aBreakpointClient.location.line, 5,
      "breakpoint2 client line is correct");

    executeSoon(function() {
      ok(aBreakpointClient.actor in gBreakpoints,
        "breakpoint2 client found in the list of debugger breakpoints");

      is(Object.keys(gBreakpoints).length, 1,
        "one breakpoint in the debugger");
      is(gPane.getBreakpoint(gSources.values[0], 5), aBreakpointClient,
        "getBreakpoint(locations[0], 5) returns the correct breakpoint");

      // remove the trap listener
      gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddBackgroundTrap);

      info("switch to the second script");
      gEditor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddSwitch);
      gEditor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onEditorTextChanged);
      gSources.selectedIndex = 0;
    });
  }

  function onEditorBreakpointAddSwitch(aEvent)
  {
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointAddSwitch);
    editorBreakpointChanges++;

    ok(aEvent, "breakpoint2 added to the editor");
    is(aEvent.added.length, 1, "one breakpoint added to the editor");
    is(aEvent.removed.length, 0, "no breakpoint was removed from the editor");
    is(aEvent.added[0].line, 4, "editor breakpoint line is correct");

    is(gEditor.getBreakpoints().length, 1,
      "editor.getBreakpoints().length is correct");
  }

  function onEditorTextChanged()
  {
    // Wait for the actual text to be shown.
    if (gDebugger.editor.getText() == gDebugger.L10N.getStr("loadingText")) {
      return;
    }
    // The requested source text has been shown, remove the event listener.
    gEditor.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onEditorTextChanged);

    is(gEditor.getText().indexOf("debugger"), -1,
      "The second script is no longer displayed.");

    isnot(gEditor.getText().indexOf("firstCall"), -1,
      "The first script is displayed.");

    executeSoon(function() {
      info("remove the second breakpoint using the mouse");
      gEditor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointRemoveSecond);

      let iframe = gEditor.editorElement;
      let testWin = iframe.ownerDocument.defaultView;

      // flush the layout for the iframe
      info("rect " + iframe.contentDocument.documentElement.getBoundingClientRect());

      let utils = testWin.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindowUtils);

      let rect = iframe.getBoundingClientRect();
      let left = rect.left + 10;
      let top = rect.top + 70;
      utils.sendMouseEventToWindow("mousedown", left, top, 0, 1, 0, false, 0, 0);
      utils.sendMouseEventToWindow("mouseup", left, top, 0, 1, 0, false, 0, 0);
    });

  }

  function onEditorBreakpointRemoveSecond(aEvent)
  {
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE, onEditorBreakpointRemoveSecond);
    editorBreakpointChanges++;

    ok(aEvent, "breakpoint2 removed from the editor");
    is(aEvent.added.length, 0, "no breakpoint was added to the editor");
    is(aEvent.removed.length, 1, "one breakpoint was removed from the editor");
    is(aEvent.removed[0].line, 4, "editor breakpoint line is correct");

    is(gEditor.getBreakpoints().length, 0,
      "editor.getBreakpoints().length is correct");

    executeSoon(function() {
      gDebugger.gClient.addOneTimeListener("resumed", function() {
        finalCheck();
        closeDebuggerAndFinish();
      });
      gDebugger.DebuggerController.activeThread.resume();
    });
  }

  function finalCheck() {
    is(Object.keys(gBreakpoints).length, 0, "no breakpoint in the debugger");
    ok(!gPane.getBreakpoint(gSources.values[0], 5),
      "getBreakpoint(locations[0], 5) returns no breakpoint");
  }

  registerCleanupFunction(function() {
    removeTab(gTab);
    is(breakpointsAdded, 2, "correct number of breakpoints have been added");
    is(breakpointsRemoved, 1, "correct number of breakpoints have been removed");
    is(editorBreakpointChanges, 4, "correct number of editor breakpoint changes");
    gPane = null;
    gTab = null;
    gDebuggee = null;
    gDebugger = null;
    gEditor = null;
    gSources = null;
    gBreakpoints = null;
  });
}
