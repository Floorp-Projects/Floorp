/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can set breakpoints and step through source mapped coffee
 * script.
 */

const TAB_URL = EXAMPLE_URL + "binary_search.html";

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;

function test()
{
  let scriptShown = false;
  let framesAdded = false;
  let resumed = false;
  let testStarted = false;

  Services.prefs.setBoolPref("devtools.debugger.source-maps-enabled", true);

  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    resumed = true;
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    gDebugger.addEventListener("Debugger:SourceShown", function _onSourceShown(aEvent) {
      gDebugger.removeEventListener("Debugger:SourceShown", _onSourceShown);
      ok(aEvent.detail.url.indexOf(".coffee") != -1,
         "The debugger should show the source mapped coffee script file.");
      ok(gDebugger.editor.getText().search(/isnt/) != -1,
         "The debugger's editor should have the coffee script source displayed.");

      testSetBreakpoint();
    });
  });
}

function testSetBreakpoint() {
  let { activeThread } = gDebugger.DebuggerController;
  activeThread.interrupt(function (aResponse) {
    activeThread.setBreakpoint({
      url: EXAMPLE_URL + "binary_search.coffee",
      line: 5
    }, function (aResponse, bpClient) {
      ok(!aResponse.error,
         "Should be able to set a breakpoint in a coffee script file.");
      testSetBreakpointBlankLine();
    });
  });
}

function testSetBreakpointBlankLine() {
  let { activeThread } = gDebugger.DebuggerController;
  activeThread.setBreakpoint({
      url: EXAMPLE_URL + "binary_search.coffee",
      line: 3
  }, function (aResponse, bpClient) {
    ok(aResponse.actualLocation,
       "Because 3 is empty, we should have an actualLocation");
    is(aResponse.actualLocation.url, EXAMPLE_URL + "binary_search.coffee",
       "actualLocation.url should be source mapped to the coffee file");
    is(aResponse.actualLocation.line, 2,
       "actualLocation.line should be source mapped back to 2");
    testHitBreakpoint();
  });
}

function testHitBreakpoint() {
  let { activeThread } = gDebugger.DebuggerController;
  activeThread.resume(function (aResponse) {
    ok(!aResponse.error, "Shouldn't get an error resuming");
    is(aResponse.type, "resumed", "Type should be 'resumed'");

    activeThread.addOneTimeListener("paused", function (aEvent, aPacket) {
      is(aPacket.type, "paused",
         "We should now be paused again");
      is(aPacket.why.type, "breakpoint",
         "and the reason we should be paused is because we hit a breakpoint");

      // Check that we stopped at the right place, by making sure that the
      // environment is in the state that we expect.
      is(aPacket.frame.environment.bindings.variables.start.value, 0,
         "'start' is 0");
      is(aPacket.frame.environment.bindings.variables.stop.value.type, "undefined",
         "'stop' hasn't been assigned to yet");
      is(aPacket.frame.environment.bindings.variables.pivot.value.type, "undefined",
         "'pivot' hasn't been assigned to yet");

      waitForCaretPos(4, testStepping);
    });

    // This will cause the breakpoint to be hit, and put us back in the paused
    // state.
    executeSoon(function() {
      gDebuggee.binary_search([0, 2, 3, 5, 7, 10], 5);
    });
  });
}

function testStepping() {
  let { activeThread } = gDebugger.DebuggerController;
  activeThread.stepIn(function (aResponse) {
    ok(!aResponse.error, "Shouldn't get an error resuming");
    is(aResponse.type, "resumed", "Type should be 'resumed'");

    // After stepping, we will pause again, so listen for that.
    activeThread.addOneTimeListener("paused", function (aEvent, aPacket) {

      // Check that we stopped at the right place, by making sure that the
      // environment is in the state that we expect.
      is(aPacket.frame.environment.bindings.variables.start.value, 0,
         "'start' is 0");
      is(aPacket.frame.environment.bindings.variables.stop.value, 5,
         "'stop' hasn't been assigned to yet");
      is(aPacket.frame.environment.bindings.variables.pivot.value.type, "undefined",
         "'pivot' hasn't been assigned to yet");

      waitForCaretPos(5, closeDebuggerAndFinish);
    });
  });
}

function waitForCaretPos(number, callback)
{
  // Poll every few milliseconds until the source editor line is active.
  let count = 0;
  let intervalID = window.setInterval(function() {
    info("count: " + count + " ");
    if (++count > 50) {
      ok(false, "Timed out while polling for the line.");
      window.clearInterval(intervalID);
      return closeDebuggerAndFinish();
    }
    if (gDebugger.DebuggerView.editor.getCaretPosition().line != number) {
      return;
    }
    // We got the source editor at the expected line, it's safe to callback.
    window.clearInterval(intervalID);
    callback();
  }, 100);
}

registerCleanupFunction(function() {
  Services.prefs.setBoolPref("devtools.debugger.source-maps-enabled", false);
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
