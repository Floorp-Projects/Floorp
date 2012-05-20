/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;

const TAB_URL = EXAMPLE_URL + "browser_dbg_frame-parameters.html";

function test() {
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.contentWindow;

    testFrameEval();
  });
}

function testFrameEval() {
  gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
    gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
    Services.tm.currentThread.dispatch({ run: function() {

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      var localScope = gDebugger.DebuggerView.Properties.localScope,
          localNodes = localScope.querySelector(".details").childNodes,
          varA = localNodes[7];

      is(varA.querySelector(".name").getAttribute("value"), "a",
        "Should have the right name for 'a'.");

      is(varA.querySelector(".value").getAttribute("value"), 1,
        "Should have the right initial value for 'a'.");

      testModification(varA, function(aVar) {
        testModification(aVar, function(aVar) {
          testModification(aVar, function(aVar) {
            resumeAndFinish();
          }, "document.title", '"Debugger Function Call Parameter Test"');
        }, "b", "[object Object]");
      }, "{ a: 1 }", "[object Object]");
    }}, 0);
  }, false);

  EventUtils.sendMouseEvent({ type: "click" },
    content.document.querySelector("button"),
    content.window);
}

function testModification(aVar, aCallback, aNewValue, aNewResult) {
  function makeChangesAndExitInputMode() {
    EventUtils.sendString(aNewValue);
    EventUtils.sendKey("RETURN");
  }

  EventUtils.sendMouseEvent({ type: "click" },
    aVar.querySelector(".value"),
    gDebugger);

  executeSoon(function() {
    ok(aVar.querySelector(".element-input"),
      "There should be an input element created.");

    gDebugger.addEventListener("Debugger:FetchedVariables", function test() {
      gDebugger.removeEventListener("Debugger:FetchedVariables", test, false);
      // Get the variable reference anew, since the old ones were discarded when
      // we resumed.
      var localScope = gDebugger.DebuggerView.Properties.localScope,
          localNodes = localScope.querySelector(".details").childNodes,
          varA = localNodes[7];

      is(varA.querySelector(".value").getAttribute("value"), aNewResult,
        "Should have the right value for 'a'.");

      executeSoon(function() {
        aCallback(varA);
      });
    }, false);

    makeChangesAndExitInputMode();
  });
}

function resumeAndFinish() {
  gDebugger.DebuggerController.activeThread.resume(function() {
    closeDebuggerAndFinish(gTab);
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
