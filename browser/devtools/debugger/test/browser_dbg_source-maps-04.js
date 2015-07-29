/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that bogus source maps don't break debugging.
 */

const TAB_URL = EXAMPLE_URL + "doc_minified_bogus_map.html";
const JS_URL = EXAMPLE_URL + "code_math_bogus_map.js";

// This test causes an error to be logged in the console, which appears in TBPL
// logs, so we are disabling that here.
DevToolsUtils.reportingDisabled = true;

let gPanel, gDebugger, gFrames, gSources, gPrefs;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gSources = gDebugger.DebuggerView.Sources;
    gPrefs = gDebugger.Prefs;

    is(gPrefs.pauseOnExceptions, false,
      "The pause-on-exceptions pref should be disabled by default.");

    waitForSourceShown(gPanel, JS_URL)
      .then(checkInitialSource)
      .then(clickToPauseOnAllExceptions)
      .then(clickToPauseOnUncaughtExceptions)
      .then(testSetBreakpoint)
      .then(reloadPage)
      .then(testHitBreakpoint)
      .then(clickToStopPauseOnExceptions)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function checkInitialSource() {
  isnot(gSources.selectedItem.attachment.source.url.indexOf("code_math_bogus_map.js"), -1,
    "The debugger should show the minified js file.");
}

function clickToPauseOnAllExceptions() {
  var deferred = promise.defer();
  var pauseOnExceptionsButton = getPauseOnExceptionsButton();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    is(pauseOnExceptionsButton.getAttribute("tooltiptext"),
      "Pause on uncaught exceptions",
      "The button's tooltip text should be 'Pause on uncaught exceptions'.");
    is(pauseOnExceptionsButton.getAttribute("state"), 1,
      "The pause on exceptions button state variable should be 1");

      deferred.resolve();
  });

  pauseOnExceptionsButton.click();

  return deferred.promise;
}

function clickToPauseOnUncaughtExceptions() {
  var deferred = promise.defer();
  var pauseOnExceptionsButton = getPauseOnExceptionsButton();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () =>{
    is(pauseOnExceptionsButton.getAttribute("tooltiptext"),
      "Do not pause on exceptions",
      "The button's tooltip text should be 'Do not pause on exceptions'.");
    is(pauseOnExceptionsButton.getAttribute("state"), 2,
      "The pause on exceptions button state variable should be 2");

      deferred.resolve();
  });
  pauseOnExceptionsButton.click();
  return deferred.promise;
}

function clickToStopPauseOnExceptions() {
  var deferred = promise.defer();
  var pauseOnExceptionsButton = getPauseOnExceptionsButton();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    is(pauseOnExceptionsButton.getAttribute("tooltiptext"),
      "Pause on all exceptions",
      "The button's tooltip text should be 'Pause on all exceptions'.");
    is(pauseOnExceptionsButton.getAttribute("state"), 0,
      "The pause on exceptions button state variable should be 0");

      deferred.resolve();
  });
  pauseOnExceptionsButton.click();
  return deferred.promise;
}

function getPauseOnExceptionsButton() {
  return gDebugger.document.getElementById("toggle-pause-exceptions");
}

function testSetBreakpoint() {
  let deferred = promise.defer();
  let sourceForm = getSourceForm(gSources, JS_URL);
  let source = gDebugger.gThreadClient.source(sourceForm);

  source.setBreakpoint({ line: 3, column: 18 }, aResponse => {
    ok(!aResponse.error,
      "Should be able to set a breakpoint in a js file.");
    ok(!aResponse.actualLocation,
      "Should be able to set a breakpoint on line 3 and column 18.");

    deferred.resolve();
  });

  return deferred.promise;
}

function reloadPage() {
  let loaded = waitForSourceAndCaret(gPanel, ".js", 3);
  gDebugger.DebuggerController._target.activeTab.reload();
  return loaded.then(() => ok(true, "Page was reloaded and execution resumed."));
}

function testHitBreakpoint() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.resume(aResponse => {
    ok(!aResponse.error, "Shouldn't get an error resuming.");
    is(aResponse.type, "resumed", "Type should be 'resumed'.");
    is(gFrames.itemCount, 2, "Should have two frames.");

    deferred.resolve();
  });

  return deferred.promise;
}

registerCleanupFunction(function() {
  gPanel = null;
  gDebugger = null;
  gFrames = null;
  gSources = null;
  gPrefs = null;
  DevToolsUtils.reportingDisabled = false;
});
