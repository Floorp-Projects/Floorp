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
  requestLongerTimeout(3);

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

      testVar.setGrip(1.618);

      is(testVar.target.querySelector(".value").getAttribute("value"), "1.618",
        "The grip information for the variable wasn't set correctly.");

      is(testVar.target.querySelector(".variables-view-element-details").childNodes.length, 0,
        "Adding a value property shouldn't add any new tree nodes.");


      testVar.setGrip({ "type": "object", "class": "Window" });

      is(testVar.target.querySelector(".variables-view-element-details").childNodes.length, 0,
        "Adding type and class properties shouldn't add any new tree nodes.");

      is(testVar.target.querySelector(".value").getAttribute("value"), "Window",
        "The information for the variable wasn't set correctly.");


      testVar.addItems({ "helloWorld": { "value": "hello world", "enumerable": true } });

      is(testVar.target.querySelector(".variables-view-element-details").childNodes.length, 1,
        "A new detail node should have been added in the variable tree.");


      testVar.addItems({ "helloWorld": { "value": "hello jupiter", "enumerable": true } });

      is(testVar.target.querySelector(".variables-view-element-details").childNodes.length, 1,
        "Shouldn't be able to duplicate nodes added in the variable tree.");


      testVar.addItems({ "someProp0": { "value": "random string", "enumerable": true },
                         "someProp1": { "value": "another string", "enumerable": true } });

      is(testVar.target.querySelector(".variables-view-element-details").childNodes.length, 3,
        "Two new detail nodes should have been added in the variable tree.");


      testVar.addItems({ "someProp2": { "value": { "type": "null" }, "enumerable": true },
                         "someProp3": { "value": { "type": "undefined" }, "enumerable": true },
                         "someProp4": {
                           "value": { "type": "object", "class": "Object" },
                           "enumerable": true
                         }
                       });

      is(testVar.target.querySelector(".variables-view-element-details").childNodes.length, 6,
        "Three new detail nodes should have been added in the variable tree.");


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
