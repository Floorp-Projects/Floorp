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
  let scriptShown = false;
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

    executeSoon(startTest);
  });

  function onScriptShown(aEvent)
  {
    scriptShown = aEvent.detail.url.indexOf("-01.js") != -1;
    scriptShownUrl = aEvent.detail.url;
    executeSoon(startTest);
  }

  function onUlteriorScriptShown(aEvent)
  {
    scriptShownUrl = aEvent.detail.url;
    executeSoon(testScriptShown);
  }

  window.addEventListener("Debugger:ScriptShown", onScriptShown);

  function startTest()
  {
    if (scriptShown && resumed && !testStarted) {
      window.removeEventListener("Debugger:ScriptShown", onScriptShown);
      window.addEventListener("Debugger:ScriptShown", onUlteriorScriptShown);
      testStarted = true;
      Services.tm.currentThread.dispatch({ run: performTest }, 0);
    }
  }

  function finishTest()
  {
    if (scriptShown && resumed && testStarted) {
      window.removeEventListener("Debugger:ScriptShown", onUlteriorScriptShown);
      closeDebuggerAndFinish();
    }
  }

  function performTest()
  {
    testCurrentScript("-01.js", step);
    step = 1;
    reloadPage();
  }

  function testScriptShown()
  {
    if (step === 1) {
      testCurrentScript("-01.js", step);
      step = 2;
      reloadPage();
    }
    else if (step === 2) {
      testCurrentScript("-01.js", step);
      step = 3;
      gView.Scripts.selectScript(gView.Scripts.scriptLocations[1]);
    }
    else if (step === 3) {
      testCurrentScript("-02.js", step);
      step = 4;
      reloadPage();
    }
    else if (step === 4) {
      testCurrentScript("-02.js", step);
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

  function reloadPage()
  {
    executeSoon(function() {
      gDebuggee.location.reload();
    });
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
