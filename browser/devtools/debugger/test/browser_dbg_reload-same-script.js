/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the same script is shown after a page is reloaded.
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
  let step = 0;
  let expectedScript = "";
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

    startTest();
  });

  function onScriptShown(aEvent)
  {
    expectedScriptShown = aEvent.detail.url.indexOf("-01.js") != -1;
    scriptShownUrl = aEvent.detail.url;
    startTest();
  }

  function onUlteriorScriptShown(aEvent)
  {
    ok(expectedScript,
      "The expected script to show up should have been specified.");

    info("The expected script for this ScriptShown event is: " + expectedScript);
    info("The current script for this ScriptShown event is: " + aEvent.detail.url);

    expectedScriptShown = aEvent.detail.url.indexOf(expectedScript) != -1;
    scriptShownUrl = aEvent.detail.url;
    testScriptShown();
  }

  function startTest()
  {
    if (expectedScriptShown && resumed && !testStarted) {
      gDebugger.removeEventListener("Debugger:SourceShown", onScriptShown);
      gDebugger.addEventListener("Debugger:SourceShown", onUlteriorScriptShown);
      testStarted = true;
      Services.tm.currentThread.dispatch({ run: performTest }, 0);
    }
  }

  function finishTest()
  {
    if (expectedScriptShown && resumed && testStarted) {
      gDebugger.removeEventListener("Debugger:SourceShown", onUlteriorScriptShown);
      closeDebuggerAndFinish();
    }
  }

  function performTest()
  {
    testCurrentScript("-01.js", step);
    expectedScript = "-01.js";
    performAction(reloadPage);
  }

  function testScriptShown()
  {
    if (!expectedScriptShown) {
      return;
    }
    step++;

    if (step === 1) {
      testCurrentScript("-01.js", step, true);
      expectedScript = "-01.js";
      performAction(reloadPage);
    }
    else if (step === 2) {
      testCurrentScript("-01.js", step, true);
      expectedScript = "-02.js";
      performAction(switchScript, 1);
    }
    else if (step === 3) {
      testCurrentScript("-02.js", step);
      expectedScript = "-02.js";
      performAction(reloadPage);
    }
    else if (step === 4) {
      testCurrentScript("-02.js", step, true);
      expectedScript = "-01.js";
      performAction(switchScript, 0);
    }
    else if (step === 5) {
      testCurrentScript("-01.js", step);
      expectedScript = "-01.js";
      performAction(reloadPage);
    }
    else if (step === 6) {
      testCurrentScript("-01.js", step, true);
      expectedScript = "-01.js";
      performAction(reloadPage);
    }
    else if (step === 7) {
      testCurrentScript("-01.js", step, true);
      expectedScript = "-01.js";
      performAction(reloadPage);
    }
    else if (step === 8) {
      testCurrentScript("-01.js", step, true);
      expectedScript = "-02.js";
      performAction(switchScript, 1);
    }
    else if (step === 9) {
      testCurrentScript("-02.js", step);
      expectedScript = "-02.js";
      performAction(reloadPage);
    }
    else if (step === 10) {
      testCurrentScript("-02.js", step, true);
      expectedScript = "-02.js";
      performAction(reloadPage);
    }
    else if (step === 11) {
      testCurrentScript("-02.js", step, true);
      expectedScript = "-02.js";
      performAction(reloadPage);
    }
    else if (step === 12) {
      testCurrentScript("-02.js", step, true);
      expectedScript = "-01.js";
      performAction(switchScript, 0);
    }
    else if (step === 13) {
      testCurrentScript("-01.js", step);
      finishTest();
    }
  }

  function testCurrentScript(part, step, isAfterReload)
  {
    info("Currently preferred script: " + gView.Sources.preferredValue);
    info("Currently selected script: " + gView.Sources.selectedValue);

    if (step < 1) {
      is(gView.Sources.preferredValue, null,
        "The preferred script url should be initially null");
    }
    else if (isAfterReload) {
      isnot(gView.Sources.preferredValue.indexOf(part), -1,
        "The preferred script url wasn't set correctly. (" + step + ")");
    }

    isnot(gView.Sources.selectedValue.indexOf(part), -1,
      "The selected script isn't the correct one. (" + step + ")");
    is(gView.Sources.selectedValue, scriptShownUrl,
      "The shown script is not the the correct one. (" + step + ")");
  }

  function performAction(callback, data)
  {
    // Poll every few milliseconds until the scripts are retrieved.
    let count = 0;
    let intervalID = window.setInterval(function() {
      info("count: " + count + " ");
      if (++count > 50) {
        ok(false, "Timed out while polling for the scripts.");
        window.clearInterval(intervalID);
        return closeDebuggerAndFinish();
      }
      if (gView.Sources.values.length !== 2) {
        return;
      }
      info("Available scripts: " + gView.Sources.values);

      // We got all the scripts, it's safe to callback.
      window.clearInterval(intervalID);
      callback(data);
    }, 100);
  }

  function switchScript(index)
  {
    gView.Sources.selectedValue = gView.Sources.values[index];
  }

  function reloadPage()
  {
    gDebuggee.location.reload();
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
