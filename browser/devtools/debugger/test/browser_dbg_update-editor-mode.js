/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that updating the editor mode sets the right highlighting engine,
 * and script URIs with extra query parameters also get the right engine.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_update-editor-mode.html";

let tempScope = {};
Cu.import("resource:///modules/source-editor.jsm", tempScope);
let SourceEditor = tempScope.SourceEditor;

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gScripts = null;

function test()
{
  let scriptShown = false;
  let framesAdded = false;
  let testStarted = false;
  let resumed = false;

  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.contentWindow;
    resumed = true;

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      framesAdded = true;
      executeSoon(startTest);
    });

    executeSoon(function() {
      gDebuggee.firstCall();
    });
  });

  function onScriptShown(aEvent) {
    scriptShown = aEvent.detail.url.indexOf("test-editor-mode") != -1;
    executeSoon(startTest);
  }

  window.addEventListener("Debugger:ScriptShown", onScriptShown);

  function startTest()
  {
    if (scriptShown && framesAdded && resumed && !testStarted) {
      window.removeEventListener("Debugger:ScriptShown", onScriptShown);
      testStarted = true;
      Services.tm.currentThread.dispatch({ run: testScriptsDisplay }, 0);
    }
  }
}

function testScriptsDisplay() {
  gScripts = gDebugger.DebuggerView.Scripts._scripts;

  is(gDebugger.DebuggerController.activeThread.state, "paused",
    "Should only be getting stack frames while paused.");

  is(gScripts.itemCount, 2, "Found the expected number of scripts.");

  is(gDebugger.editor.getMode(), SourceEditor.MODES.HTML,
     "Found the expected editor mode.");

  ok(gDebugger.editor.getText().search(/debugger/) != -1,
    "The correct script was loaded initially.");

  window.addEventListener("Debugger:ScriptShown", function _onEvent(aEvent) {
    let url = aEvent.detail.url;
    if (url.indexOf("switching-01.js") != -1) {
      window.removeEventListener(aEvent.type, _onEvent);
      testSwitchPaused();
    }
  });

  let url = gDebuggee.document.querySelector("script").src;
  gDebugger.DebuggerView.Scripts.selectScript(url);
}

function testSwitchPaused()
{
  ok(gDebugger.editor.getText().search(/debugger/) == -1,
    "The second script is no longer displayed.");

  ok(gDebugger.editor.getText().search(/firstCall/) != -1,
    "The first script is displayed.");

  is(gDebugger.editor.getMode(), SourceEditor.MODES.JAVASCRIPT,
     "Found the expected editor mode.");

  gDebugger.DebuggerController.activeThread.resume(function() {
    closeDebuggerAndFinish();
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
  gScripts = null;
});
