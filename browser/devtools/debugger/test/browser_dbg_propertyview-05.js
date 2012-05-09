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

      testVar.setGrip(1.618);

      is(testVar.querySelector(".info").textContent, "1.618",
        "The grip information for the variable wasn't set correctly.");

      is(testVar.querySelector(".details").childNodes.length, 0,
        "Adding a value property shouldn't add any new tree nodes.");


      testVar.setGrip({ "type": "object", "class": "Window" });

      is(testVar.querySelector(".details").childNodes.length, 0,
        "Adding type and class properties shouldn't add any new tree nodes.");

      is(testVar.querySelector(".info").textContent, "[object Window]",
        "The information for the variable wasn't set correctly.");


      testVar.addProperties({ "helloWorld": { "value": "hello world" } });

      is(testVar.querySelector(".details").childNodes.length, 1,
        "A new detail node should have been added in the variable tree.");


      testVar.addProperties({ "helloWorld": { "value": "hello jupiter" } });

      is(testVar.querySelector(".details").childNodes.length, 1,
        "Shouldn't be able to duplicate nodes added in the variable tree.");


      testVar.addProperties({ "someProp0": { "value": "random string" },
                              "someProp1": { "value": "another string" } });

      is(testVar.querySelector(".details").childNodes.length, 3,
        "Two new detail nodes should have been added in the variable tree.");


      testVar.addProperties({ "someProp2": { "value": { "type": "null" } },
                              "someProp3": { "value": { "type": "undefined" } },
                              "someProp4": { "value": { "type": "object", "class": "Object" } } });

      is(testVar.querySelector(".details").childNodes.length, 6,
        "Three new detail nodes should have been added in the variable tree.");


      testVar.empty();

      is(testVar.querySelector(".details").childNodes.length, 0,
        "The var should remove all it's details container tree children.");

      testVar.remove();

      is(testScope.querySelector(".details").childNodes.length, 0,
        "The var should have been removed from the parent container tree.");

      gDebugger.DebuggerController.activeThread.resume(function() {
        closeDebuggerAndFinish(gTab);
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
