/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that bogus source maps don't break debugging.
 */

const TAB_URL = EXAMPLE_URL + "doc_minified_bogus_map.html";
const JS_URL = EXAMPLE_URL + "code_math_bogus_map.min.js";

// This test causes an error to be logged in the console, which appears in TBPL
// logs, so we are disabling that here.
let { DevToolsUtils } = Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm", {});
DevToolsUtils.reportingDisabled = true;

let gPanel, gDebugger, gFrames, gSources, gPrefs, gOptions;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;
    gSources = gDebugger.DebuggerView.Sources;
    gPrefs = gDebugger.Prefs;
    gOptions = gDebugger.DebuggerView.Options;

    is(gPrefs.pauseOnExceptions, false,
      "The pause-on-exceptions pref should be disabled by default.");
    isnot(gOptions._pauseOnExceptionsItem.getAttribute("checked"), "true",
      "The pause-on-exceptions menu item should not be checked.");

    waitForSourceShown(gPanel, JS_URL)
      .then(checkInitialSource)
      .then(enablePauseOnExceptions)
      .then(disableIgnoreCaughtExceptions)
      .then(testSetBreakpoint)
      .then(reloadPage)
      .then(testHitBreakpoint)
      .then(enableIgnoreCaughtExceptions)
      .then(disablePauseOnExceptions)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function checkInitialSource() {
  isnot(gSources.selectedValue.indexOf(".min.js"), -1,
    "The debugger should show the minified js file.");
}

function enablePauseOnExceptions() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    is(gPrefs.pauseOnExceptions, true,
      "The pause-on-exceptions pref should now be enabled.");

    ok(true, "Pausing on exceptions was enabled.");
    deferred.resolve();
  });

  gOptions._pauseOnExceptionsItem.setAttribute("checked", "true");
  gOptions._togglePauseOnExceptions();

  return deferred.promise;
}

function disableIgnoreCaughtExceptions() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    is(gPrefs.ignoreCaughtExceptions, false,
      "The ignore-caught-exceptions pref should now be disabled.");

    ok(true, "Ignore caught exceptions was disabled.");
    deferred.resolve();
  });

  gOptions._ignoreCaughtExceptionsItem.setAttribute("checked", "false");
  gOptions._toggleIgnoreCaughtExceptions();

  return deferred.promise;
}

function testSetBreakpoint() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.setBreakpoint({ url: JS_URL, line: 3, column: 61 }, aResponse => {
    ok(!aResponse.error,
      "Should be able to set a breakpoint in a js file.");
    ok(!aResponse.actualLocation,
      "Should be able to set a breakpoint on line 3 and column 61.");

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

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES).then(() => {
      is(gFrames.itemCount, 1, "Should have one frame.");

      gDebugger.gThreadClient.resume(deferred.resolve);
    });
  });

  return deferred.promise;
}

function enableIgnoreCaughtExceptions() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    is(gPrefs.ignoreCaughtExceptions, true,
      "The ignore-caught-exceptions pref should now be enabled.");

    ok(true, "Ignore caught exceptions was enabled.");
    deferred.resolve();
  });

  gOptions._ignoreCaughtExceptionsItem.setAttribute("checked", "true");
  gOptions._toggleIgnoreCaughtExceptions();

  return deferred.promise;
}

function disablePauseOnExceptions() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.addOneTimeListener("resumed", () => {
    is(gPrefs.pauseOnExceptions, false,
      "The pause-on-exceptions pref should now be disabled.");

    ok(true, "Pausing on exceptions was disabled.");
    deferred.resolve();
  });

  gOptions._pauseOnExceptionsItem.setAttribute("checked", "false");
  gOptions._togglePauseOnExceptions();

  return deferred.promise;
}

registerCleanupFunction(function() {
  gPanel = null;
  gDebugger = null;
  gFrames = null;
  gSources = null;
  gPrefs = null;
  gOptions = null;
  DevToolsUtils.reportingDisabled = false;
});
