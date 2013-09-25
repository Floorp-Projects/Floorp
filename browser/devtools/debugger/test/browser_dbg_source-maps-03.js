/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can debug minified javascript with source maps.
 */

const TAB_URL = EXAMPLE_URL + "doc_minified.html";
const JS_URL = EXAMPLE_URL + "code_math.js";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources, gFrames;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gFrames = gDebugger.DebuggerView.StackFrames;

    waitForSourceShown(gPanel, JS_URL)
      .then(checkInitialSource)
      .then(testSetBreakpoint)
      .then(testHitBreakpoint)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function checkInitialSource() {
  isnot(gSources.selectedValue.indexOf(".js"), -1,
    "The debugger should not show the minified js file.");
  is(gSources.selectedValue.indexOf(".min.js"), -1,
    "The debugger should show the original js file.");
  is(gEditor.getText().split("\n").length, 46,
    "The debugger's editor should have the original source displayed, " +
    "not the whitespace stripped minified version.");
}

function testSetBreakpoint() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.interrupt(aResponse => {
    gDebugger.gThreadClient.setBreakpoint({ url: JS_URL, line: 30, column: 21 }, aResponse => {
      ok(!aResponse.error,
        "Should be able to set a breakpoint in a js file.");
      ok(!aResponse.actualLocation,
        "Should be able to set a breakpoint on line 30 and column 10.");

      deferred.resolve();
    });
  });

  return deferred.promise;
}

function testHitBreakpoint() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.resume(aResponse => {
    ok(!aResponse.error, "Shouldn't get an error resuming.");
    is(aResponse.type, "resumed", "Type should be 'resumed'.");

    waitForCaretAndScopes(gPanel, 30).then(() => {
      // Make sure that we have the right stack frames.
      is(gFrames.itemCount, 9,
        "Should have nine frames.");
      is(gFrames.getItemAtIndex(0).description.indexOf(".min.js"), -1,
        "First frame should not be a minified JS frame.");
      isnot(gFrames.getItemAtIndex(0).description.indexOf(".js"), -1,
        "First frame should be a JS frame.");

      deferred.resolve();
    });

    // This will cause the breakpoint to be hit, and put us back in the
    // paused state.
    gDebuggee.arithmetic();
  });

  return deferred.promise;
}


registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gFrames = null;
});
