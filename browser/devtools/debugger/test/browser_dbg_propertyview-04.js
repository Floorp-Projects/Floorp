/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;

function test() {
  debug_tab_pane(STACK_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    testSimpleCall();
  });
}

function testSimpleCall() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      let testScope = gDebugger.DebuggerView.Variables.addScope("test");
      let testVar = testScope.addItem("something");

      let properties = testVar.addItems({
        "child": {
          "value": {
            "type": "object",
            "class": "Object"
          },
          "enumerable": true
        }
      });

      is(testVar.target.querySelector(".variables-view-element-details").childNodes.length, 1,
        "A new detail node should have been added in the variable tree.");

      ok(testVar.get("child"),
        "The added detail property should be accessible from the variable.");


      let properties2 = testVar.get("child").addItems({
        "grandchild": {
          "value": {
            "type": "object",
            "class": "Object"
          },
          "enumerable": true
        }
      });

      is(testVar.get("child").target.querySelector(".variables-view-element-details").childNodes.length, 1,
        "A new detail node should have been added in the variable tree.");

      ok(testVar.get("child").get("grandchild"),
        "The added detail property should be accessible from the variable.");


      gDebugger.DebuggerController.activeThread.resume(function() {
        closeDebuggerAndFinish();
      });
    }}, 0);
  });

  gDebuggee.simpleCall();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
