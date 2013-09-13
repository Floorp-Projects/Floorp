/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can set breakpoints and step through source mapped
 * coffee script.
 */

const TAB_URL = EXAMPLE_URL + "doc_binary_search.html";
const COFFEE_URL = EXAMPLE_URL + "code_binary_search.coffee";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;

    checkSourceMapsEnabled();

    waitForSourceShown(gPanel, ".coffee")
      .then(checkInitialSource)
      .then(testSetBreakpoint)
      .then(testSetBreakpointBlankLine)
      .then(testHitBreakpoint)
      .then(testStepping)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function checkSourceMapsEnabled() {
  is(Services.prefs.getBoolPref("devtools.debugger.source-maps-enabled"), true,
    "The source maps functionality should be enabled by default.");
  is(gDebugger.Prefs.sourceMapsEnabled, true,
    "The source maps pref should be true from startup.");
  is(gDebugger.DebuggerView.Options._showOriginalSourceItem.getAttribute("checked"), "true",
    "Source maps should be enabled from startup.")
}

function checkInitialSource() {
  isnot(gSources.selectedValue.indexOf(".coffee"), -1,
    "The debugger should show the source mapped coffee source file.");
  is(gSources.selectedValue.indexOf(".js"), -1,
    "The debugger should not show the generated js source file.");
  is(gEditor.getText().indexOf("isnt"), 218,
    "The debugger's editor should have the coffee source source displayed.");
  is(gEditor.getText().indexOf("function"), -1,
    "The debugger's editor should not have the JS source displayed.");
}

function testSetBreakpoint() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.interrupt(aResponse => {
    gDebugger.gThreadClient.setBreakpoint({ url: COFFEE_URL, line: 5 }, aResponse => {
      ok(!aResponse.error,
        "Should be able to set a breakpoint in a coffee source file.");
      ok(!aResponse.actualLocation,
        "Should be able to set a breakpoint on line 5.");

      deferred.resolve();
    });
  });

  return deferred.promise;
}

function testSetBreakpointBlankLine() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.setBreakpoint({ url: COFFEE_URL, line: 3 }, aResponse => {
    ok(!aResponse.error,
      "Should be able to set a breakpoint in a coffee source file on a blank line.");
    ok(aResponse.actualLocation,
      "Because 3 is empty, we should have an actualLocation.");
    is(aResponse.actualLocation.url, COFFEE_URL,
      "actualLocation.url should be source mapped to the coffee file.");
    is(aResponse.actualLocation.line, 2,
      "actualLocation.line should be source mapped back to 2.");

    deferred.resolve();
  });

  return deferred.promise;
}

function testHitBreakpoint() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.resume(aResponse => {
    ok(!aResponse.error, "Shouldn't get an error resuming.");
    is(aResponse.type, "resumed", "Type should be 'resumed'.");

    gDebugger.gThreadClient.addOneTimeListener("paused", (aEvent, aPacket) => {
      is(aPacket.type, "paused",
        "We should now be paused again.");
      is(aPacket.why.type, "breakpoint",
        "and the reason we should be paused is because we hit a breakpoint.");

      // Check that we stopped at the right place, by making sure that the
      // environment is in the state that we expect.
      is(aPacket.frame.environment.bindings.variables.start.value, 0,
         "'start' is 0.");
      is(aPacket.frame.environment.bindings.variables.stop.value.type, "undefined",
         "'stop' hasn't been assigned to yet.");
      is(aPacket.frame.environment.bindings.variables.pivot.value.type, "undefined",
         "'pivot' hasn't been assigned to yet.");

      waitForCaretUpdated(gPanel, 5).then(deferred.resolve);
    });

    // This will cause the breakpoint to be hit, and put us back in the
    // paused state.
    gDebuggee.binary_search([0, 2, 3, 5, 7, 10], 5);
  });

  return deferred.promise;
}

function testStepping() {
  let deferred = promise.defer();

  gDebugger.gThreadClient.stepIn(aResponse => {
    ok(!aResponse.error, "Shouldn't get an error resuming.");
    is(aResponse.type, "resumed", "Type should be 'resumed'.");

    // After stepping, we will pause again, so listen for that.
    gDebugger.gThreadClient.addOneTimeListener("paused", (aEvent, aPacket) => {
      is(aPacket.type, "paused",
        "We should now be paused again.");
      is(aPacket.why.type, "resumeLimit",
        "and the reason we should be paused is because we hit the resume limit.");

      // Check that we stopped at the right place, by making sure that the
      // environment is in the state that we expect.
      is(aPacket.frame.environment.bindings.variables.start.value, 0,
         "'start' is 0.");
      is(aPacket.frame.environment.bindings.variables.stop.value, 5,
         "'stop' hasn't been assigned to yet.");
      is(aPacket.frame.environment.bindings.variables.pivot.value.type, "undefined",
         "'pivot' hasn't been assigned to yet.");

      waitForCaretUpdated(gPanel, 6).then(deferred.resolve);
    });
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
});
