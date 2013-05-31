/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that switching the displayed script in the UI works as advertised.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gSources = null;

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

    gDebugger.addEventListener("Debugger:SourceShown", onScriptShown);

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      framesAdded = true;
      executeSoon(startTest);
    });

    executeSoon(function() {
      gDebuggee.firstCall();
    });
  });

  function onScriptShown(aEvent)
  {
    scriptShown = aEvent.detail.url.indexOf("-02.js") != -1;
    executeSoon(startTest);
  }

  function startTest()
  {
    if (scriptShown && framesAdded && resumed && !testStarted) {
      gDebugger.removeEventListener("Debugger:SourceShown", onScriptShown);
      testStarted = true;
      Services.tm.currentThread.dispatch({ run: testScriptsDisplay }, 0);
    }
  }
}

function testScriptsDisplay() {
  gSources = gDebugger.DebuggerView.Sources;

  is(gDebugger.DebuggerController.activeThread.state, "paused",
    "Should only be getting stack frames while paused.");

  is(gSources.itemCount, 2,
    "Found the expected number of scripts.");

  for (let i = 0; i < gSources.itemCount; i++) {
    info("label: " + i + " " + gSources.getItemAtIndex(i).target.getAttribute("label"));
  }

  let label1 = "test-script-switching-01.js";
  let label2 = "test-script-switching-02.js";

  ok(gDebugger.DebuggerView.Sources.containsValue(EXAMPLE_URL +
    label1), "First script url is incorrect.");
  ok(gDebugger.DebuggerView.Sources.containsValue(EXAMPLE_URL +
    label2), "Second script url is incorrect.");

  ok(gDebugger.DebuggerView.Sources.containsLabel(
    label1), "First script label is incorrect.");
  ok(gDebugger.DebuggerView.Sources.containsLabel(
    label2), "Second script label is incorrect.");

  ok(gDebugger.DebuggerView.Sources.selectedItem,
    "There should be a selected item in the sources pane.");
  is(gDebugger.DebuggerView.Sources.selectedLabel,
    label2, "The selected label is the sources pane is incorrect.");
  is(gDebugger.DebuggerView.Sources.selectedValue, EXAMPLE_URL +
    label2, "The selected value is the sources pane is incorrect.");

  ok(gDebugger.editor.getText().search(/debugger/) != -1,
    "The correct script was loaded initially.");

  is(gDebugger.editor.getDebugLocation(), 5,
     "editor debugger location is correct.");

  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    let url = aEvent.detail.url;
    if (url.indexOf("-01.js") != -1) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);
      testSwitchPaused();
    }
  });

  gDebugger.DebuggerView.Sources.selectedValue = EXAMPLE_URL + label1;
}

function testSwitchPaused()
{
  dump("Debugger editor text:\n" + gDebugger.editor.getText() + "\n");

  let label1 = "test-script-switching-01.js";
  let label2 = "test-script-switching-02.js";

  ok(gDebugger.DebuggerView.Sources.selectedItem,
    "There should be a selected item in the sources pane.");
  is(gDebugger.DebuggerView.Sources.selectedLabel,
    label1, "The selected label is the sources pane is incorrect.");
  is(gDebugger.DebuggerView.Sources.selectedValue, EXAMPLE_URL +
    label1, "The selected value is the sources pane is incorrect.");

  ok(gDebugger.editor.getText().search(/debugger/) == -1,
    "The second script is no longer displayed.");

  ok(gDebugger.editor.getText().search(/firstCall/) != -1,
    "The first script is displayed.");

  is(gDebugger.editor.getDebugLocation(), -1,
    "Editor debugger location has been cleared.");

  gDebugger.DebuggerController.activeThread.resume(function() {
    gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
      let url = aEvent.detail.url;
      if (url.indexOf("-02.js") != -1) {
        gDebugger.removeEventListener(aEvent.type, _onEvent);
        testSwitchRunning();
      }
    });

    gDebugger.DebuggerView.Sources.selectedValue = EXAMPLE_URL + label2;
  });
}

function testSwitchRunning()
{
  dump("Debugger editor text:\n" + gDebugger.editor.getText() + "\n");

  let label1 = "test-script-switching-01.js";
  let label2 = "test-script-switching-02.js";

  ok(gDebugger.DebuggerView.Sources.selectedItem,
    "There should be a selected item in the sources pane.");
  is(gDebugger.DebuggerView.Sources.selectedLabel,
    label2, "The selected label is the sources pane is incorrect.");
  is(gDebugger.DebuggerView.Sources.selectedValue, EXAMPLE_URL +
    label2, "The selected value is the sources pane is incorrect.");

  ok(gDebugger.editor.getText().search(/debugger/) != -1,
    "The second script is displayed again.");

  ok(gDebugger.editor.getText().search(/firstCall/) == -1,
    "The first script is no longer displayed.");

  is(gDebugger.editor.getDebugLocation(), -1,
    "Editor debugger location is still -1.");

  closeDebuggerAndFinish();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
  gSources = null;
});
