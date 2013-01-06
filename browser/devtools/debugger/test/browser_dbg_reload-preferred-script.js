/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the preferred script is shown when a page is loaded.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

let gPane = null;
let gTab = null;
let gDebuggee = null;
let gDebugger = null;
let gView = null;

requestLongerTimeout(2);

function test()
{
  let expectedScript = "test-script-switching-02.js";
  let expectedScriptShown = false;
  let scriptShownUrl = null;
  let resumed = false;
  let testStarted = false;

  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gView = gDebugger.DebuggerView;
    resumed = true;

    gDebugger.addEventListener("Debugger:SourceShown", onScriptShown);

    gView.Sources.preferredSource = EXAMPLE_URL + expectedScript;
    startTest();
  });

  function onScriptShown(aEvent)
  {
    expectedScriptShown = aEvent.detail.url.indexOf(expectedScript) != -1;
    scriptShownUrl = aEvent.detail.url;
    startTest();
  }

  function startTest()
  {
    if (expectedScriptShown && resumed && !testStarted) {
      gDebugger.removeEventListener("Debugger:SourceShown", onScriptShown);
      testStarted = true;
      Services.tm.currentThread.dispatch({ run: performTest }, 0);
    }
  }

  function performTest()
  {
    info("Currently preferred script: " + gView.Sources.preferredValue);
    info("Currently selected script: " + gView.Sources.selectedValue);

    isnot(gView.Sources.preferredValue.indexOf(expectedScript), -1,
      "The preferred script url wasn't set correctly.");
    isnot(gView.Sources.selectedValue.indexOf(expectedScript), -1,
      "The selected script isn't the correct one.");
    is(gView.Sources.selectedValue, scriptShownUrl,
      "The shown script is not the the correct one.");

    closeDebuggerAndFinish();
  }

  registerCleanupFunction(function() {
    removeTab(gTab);
    gPane = null;
    gTab = null;
    gDebuggee = null;
    gDebugger = null;
    gView = null;
  });
}
