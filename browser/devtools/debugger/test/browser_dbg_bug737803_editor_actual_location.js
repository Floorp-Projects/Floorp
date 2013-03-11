/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 737803: Setting a breakpoint in a line without code should move
 * the icon to the actual location.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

let gPane = null;
let gTab = null;
let gDebuggee = null;
let gDebugger = null;
let gSources = null;
let gEditor = null;
let gBreakpoints = null;

function test() {
  let scriptShown = false;
  let framesAdded = false;
  let testStarted = false;
  let resumed = false;

  debug_tab_pane(TAB_URL, function (aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebuggee = aDebuggee;
    gDebugger = gPane.panelWin;
    resumed = true;

    gDebugger.addEventListener("Debugger:SourceShown", onSourceShown);

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function () {
      framesAdded = true;
      executeSoon(startTest);
    });

    executeSoon(function () {
      gDebuggee.firstCall();
    });
  });

  function onSourceShown(aEvent) {
    scriptShown = aEvent.detail.url.indexOf("-02.js") != -1;
    executeSoon(startTest);
  }

  function startTest() {
    if (scriptShown && framesAdded && resumed && !testStarted) {
      gDebugger.removeEventListener("Debugger:SourceShown", onSourceShown);
      testStarted = true;
      Services.tm.currentThread.dispatch({ run: performTest }, 0);
    }
  }

  function performTest() {
    gSources = gDebugger.DebuggerView.Sources;
    gEditor = gDebugger.editor;
    gBreakpoints = gPane.getAllBreakpoints();
    is(Object.keys(gBreakpoints), 0, "There are no breakpoints");

    gEditor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE,
      onEditorBreakpointAdd);

    let location = { url: gSources.selectedValue, line: 4 };
    executeSoon(function () {
      gPane.addBreakpoint(location, onBreakpointAdd);
    });
  }

  let onBpDebuggerAdd = false;
  let onBpEditorAdd = false;

  function onBreakpointAdd(aBpClient) {
    is(aBpClient.location.url, gSources.selectedValue, "URL is the same");
    is(aBpClient.location.line, 6, "Line number is new");
    is(aBpClient.requestedLocation.line, 4, "Requested location is correct");

    onBpDebuggerAdd = true;
    tryFinish();
  }

  function onEditorBreakpointAdd(aEvent) {
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE,
      onEditorBreakpointAdd);

    is(gEditor.getBreakpoints().length, 1,
      "There is only one breakpoint in the editor");

    ok(!gPane.getBreakpoint(gSources.selectedValue, 4),
      "There are no breakpoints on an invalid line");

    let br = gPane.getBreakpoint(gSources.selectedValue, 6);
    is(br.location.url, gSources.selectedValue, "URL is correct");
    is(br.location.line, 6, "Line number is correct");

    onBpEditorAdd = true;
    tryFinish();
  }

  function tryFinish() {
    info("onBpDebuggerAdd: " + onBpDebuggerAdd);
    info("onBpEditorAdd: " + onBpEditorAdd);

    if (onBpDebuggerAdd && onBpEditorAdd) {
      closeDebuggerAndFinish();
    }
  }

  registerCleanupFunction(function () {
    removeTab(gTab);
    gPane = null;
    gTab = null;
    gDebuggee = null;
    gDebugger = null;
    gSources = null;
    gEditor = null;
    gBreakpoints = null;
  });
}
