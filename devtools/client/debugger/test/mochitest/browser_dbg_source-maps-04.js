/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that bogus source maps don't break debugging.
 */

const TAB_URL = EXAMPLE_URL + "doc_minified_bogus_map.html";
const JS_URL = EXAMPLE_URL + "code_math_bogus_map.js";

// This test causes an error to be logged in the console, which appears in TBPL
// logs, so we are disabling that here.
DevToolsUtils.reportingDisabled = true;

var gPanel, gDebugger, gFrames, gSources, gPrefs, gOptions;

function test() {
  let options = {
    source: JS_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
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

    checkInitialSource();
    enablePauseOnExceptions()
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
  isnot(gSources.selectedItem.attachment.source.url.indexOf("code_math_bogus_map.js"), -1,
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

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES).then(() => {
      is(gFrames.itemCount, 2, "Should have two frames.");

      // This is weird, but we need to let the debugger a chance to
      // update first
      executeSoon(() => {
        gDebugger.gThreadClient.resume(() => {
          gDebugger.gThreadClient.addOneTimeListener("paused", () => {
            gDebugger.gThreadClient.resume(() => {
              // We also need to make sure the next step doesn't add a
              // "resumed" handler until this is completely finished
              executeSoon(() => {
                deferred.resolve();
              });
            });
          });
        });
      });
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

registerCleanupFunction(function () {
  gPanel = null;
  gDebugger = null;
  gFrames = null;
  gSources = null;
  gPrefs = null;
  gOptions = null;
  DevToolsUtils.reportingDisabled = false;
});
