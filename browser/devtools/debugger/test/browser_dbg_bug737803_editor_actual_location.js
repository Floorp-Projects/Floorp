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
let gScripts = null;
let gEditor = null;
let gBreakpoints = null;

function test() {
  let tempScope = {};
  Cu.import("resource:///modules/source-editor.jsm", tempScope);
  let SourceEditor = tempScope.SourceEditor;

  let scriptShown = false;
  let framesAdded = false;
  let testStarted = false;
  let resumed = false;

  debug_tab_pane(TAB_URL, function (aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebuggee = aDebuggee;
    gDebugger = gPane.contentWindow;
    resumed = true;

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function () {
      framesAdded = true;
      executeSoon(startTest);
    });

    executeSoon(function () {
      gDebuggee.firstCall();
    });
  });

  function onScriptShown(aEvent) {
    scriptShown = aEvent.detail.url.indexOf("-02.js") != -1;
    executeSoon(startTest);
  }

  window.addEventListener("Debugger:ScriptShown", onScriptShown);

  function startTest() {
    if (scriptShown && framesAdded && resumed && !testStarted) {
      window.removeEventListener("Debugger:ScriptShown", onScriptShown);
      testStarted = true;
      Services.tm.currentThread.dispatch({ run: performTest }, 0);
    }
  }

  function performTest() {
    gScripts = gDebugger.DebuggerView.Scripts;
    gEditor = gDebugger.editor;
    gBreakpoints = gPane.breakpoints;
    is(Object.keys(gBreakpoints), 0, "There are no breakpoints");

    gEditor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE,
      onEditorBreakpointAdd);

    let location = { url: gScripts.selected, line: 4 };
    executeSoon(function () {
      gPane.addBreakpoint(location, onBreakpointAdd);
    });
  }

  function onBreakpointAdd(aBpClient) {
    is(aBpClient.location.url, gScripts.selected, "URL is the same");
    is(aBpClient.location.line, 6, "Line number is new");
    is(aBpClient.requestedLocation.line, 4, "Requested location is correct");
  }

  function onEditorBreakpointAdd(aEvent) {
    gEditor.removeEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE,
      onEditorBreakpointAdd);

    is(gEditor.getBreakpoints().length, 1,
      "There is only one breakpoint in the editor");

    ok(!gPane.getBreakpoint(gScripts.selected, 4),
      "There are no breakpoints on an invalid line");

    let br = gPane.getBreakpoint(gScripts.selected, 6);
    is(br.location.url, gScripts.selected, "URL is correct");
    is(br.location.line, 6, "Line number is correct");

    closeDebuggerAndFinish();
  }

  registerCleanupFunction(function () {
    removeTab(gTab);
    gPane = null;
    gTab = null;
    gDebuggee = null;
    gDebugger = null;
    gScripts = null;
    gEditor = null;
    gBreakpoints = null;
  });
}
