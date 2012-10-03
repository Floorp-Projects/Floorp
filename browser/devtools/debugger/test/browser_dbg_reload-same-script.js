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
    gDebugger = gPane.contentWindow;
    gView = gDebugger.DebuggerView;
    resumed = true;

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

  window.addEventListener("Debugger:ScriptShown", onScriptShown);

  function startTest()
  {
    if (expectedScriptShown && resumed && !testStarted) {
      window.removeEventListener("Debugger:ScriptShown", onScriptShown);
      window.addEventListener("Debugger:ScriptShown", onUlteriorScriptShown);
      testStarted = true;
      Services.tm.currentThread.dispatch({ run: performTest }, 0);
    }
  }

  function finishTest()
  {
    if (expectedScriptShown && resumed && testStarted) {
      window.removeEventListener("Debugger:ScriptShown", onUlteriorScriptShown);
      closeDebuggerAndFinish();
    }
  }

  function performTest()
  {
    testCurrentScript("-01.js", step);
    expectedScript = "-01.js";
    reloadPage();
  }

  function testScriptShown()
  {
    if (!expectedScriptShown) {
      return;
    }
    step++;

    if (step === 1) {
      testCurrentScript("-01.js", step);
      expectedScript = "-01.js";
      reloadPage();
    }
    else if (step === 2) {
      testCurrentScript("-01.js", step);
      expectedScript = "-02.js";
      switchScript(1);
    }
    else if (step === 3) {
      testCurrentScript("-02.js", step);
      expectedScript = "-02.js";
      reloadPage();
    }
    else if (step === 4) {
      testCurrentScript("-02.js", step);
      expectedScript = "-01.js";
      switchScript(0);
    }
    else if (step === 5) {
      testCurrentScript("-01.js", step);
      expectedScript = "-01.js";
      reloadPage();
    }
    else if (step === 6) {
      testCurrentScript("-01.js", step);
      expectedScript = "-01.js";
      reloadPage();
    }
    else if (step === 7) {
      testCurrentScript("-01.js", step);
      expectedScript = "-01.js";
      reloadPage();
    }
    else if (step === 8) {
      testCurrentScript("-01.js", step);
      expectedScript = "-02.js";
      switchScript(1);
    }
    else if (step === 9) {
      testCurrentScript("-02.js", step);
      expectedScript = "-02.js";
      reloadPage();
    }
    else if (step === 10) {
      testCurrentScript("-02.js", step);
      expectedScript = "-02.js";
      reloadPage();
    }
    else if (step === 11) {
      testCurrentScript("-02.js", step);
      expectedScript = "-02.js";
      reloadPage();
    }
    else if (step === 12) {
      testCurrentScript("-02.js", step);
      expectedScript = "-01.js";
      switchScript(0);
    }
    else if (step === 13) {
      testCurrentScript("-01.js", step);
      finishTest();
    }
  }

  function testCurrentScript(part, step)
  {
    info("Currently preferred script: " + gView.Scripts.preferredScriptUrl);
    info("Currently selected script: " + gView.Scripts.selected);

    isnot(gView.Scripts.preferredScriptUrl.indexOf(part), -1,
      "The preferred script url wasn't set correctly. (" + step + ")");
    isnot(gView.Scripts.selected.indexOf(part), -1,
      "The selected script isn't the correct one. (" + step + ")");
    is(gView.Scripts.selected, scriptShownUrl,
      "The shown script is not the the correct one. (" + step + ")");
  }

  function switchScript(index)
  {
    let scriptsView = gView.Scripts;
    let scriptLocations = scriptsView.scriptLocations;

    // Poll every few milliseconds until the scripts are retrieved.
    let count = 0;
    let intervalID = window.setInterval(function() {
      dump("count: " + count + " ");
      if (++count > 50) {
        ok(false, "Timed out while polling for the scripts.");
        closeDebuggerAndFinish();
      }
      if (scriptLocations.length !== 2) {
        return;
      }
      info("Available scripts: " + scriptLocations);

      // We got all the scripts, it's safe to switch.
      window.clearInterval(intervalID);
      scriptsView.selectScript(scriptLocations[index]);
    }, 100);
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
