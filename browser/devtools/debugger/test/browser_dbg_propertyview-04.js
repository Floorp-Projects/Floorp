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
    gDebugger = gPane.contentWindow;

    testSimpleCall();
  });
}

function testSimpleCall() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      let testScope = gDebugger.DebuggerView.Properties._addScope("test");
      let testVar = testScope.addVar("something");

      let properties = testVar.addProperties({ "child": { "value": { "type": "object",
                                                                     "class": "Object" } } });

      is(testVar.querySelector(".details").childNodes.length, 1,
        "A new detail node should have been added in the variable tree.");

      ok(testVar.child,
        "The added detail property should be accessible from the variable.");

      is(testVar.child, properties.child,
        "Adding a detail property should return that exact property.");


      let properties2 = testVar.child.addProperties({ "grandchild": { "value": { "type": "object",
                                                                      "class": "Object" } } });

      is(testVar.child.querySelector(".details").childNodes.length, 1,
        "A new detail node should have been added in the variable tree.");

      ok(testVar.child.grandchild,
        "The added detail property should be accessible from the variable.");

      is(testVar.child.grandchild, properties2.grandchild,
        "Adding a detail property should return that exact property.");


      testVar.child.empty();

      is(testVar.child.querySelector(".details").childNodes.length, 0,
        "The child should remove all it's details container tree children.");

      testVar.child.remove();

      is(testVar.querySelector(".details").childNodes.length, 0,
        "The child should have been removed from the parent container tree.");


      testVar.empty();

      is(testVar.querySelector(".details").childNodes.length, 0,
        "The var should remove all it's details container tree children.");

      testVar.remove();

      is(testScope.querySelector(".details").childNodes.length, 0,
        "The var should have been removed from the parent container tree.");

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
