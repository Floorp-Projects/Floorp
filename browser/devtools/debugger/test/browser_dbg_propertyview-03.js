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

      let testScope = gDebugger.DebuggerView.Variables.addScope("test-scope");
      let testVar = testScope.addItem("something");
      let duplVar = testScope.addItem("something");

      info("Scope id: " + testScope.target.id);
      info("Scope name: " + testScope.target.name);
      info("Variable id: " + testVar.target.id);
      info("Variable name: " + testVar.target.name);

      ok(testScope,
        "Should have created a scope.");
      ok(testVar,
        "Should have created a variable.");

      ok(testScope.id.contains("test-scope"),
        "Should have the correct scope id.");
      ok(testScope.target.id.contains("test-scope"),
        "Should have the correct scope id on the element.");
      is(testScope.name, "test-scope",
        "Should have the correct scope name.");

      ok(testVar.id.contains("something"),
        "Should have the correct variable id.");
      ok(testVar.target.id.contains("something"),
        "Should have the correct variable id on the element.");
      is(testVar.name, "something",
        "Should have the correct variable name.");

      is(duplVar, null,
        "Shouldn't be able to duplicate variables in the same scope.");

      is(testVar.target.querySelector(".name").getAttribute("value"), "something",
        "Any new variable should have the designated title.");

      is(testVar.target.querySelector(".variables-view-element-details").childNodes.length, 0,
        "Any new variable should have a details container with no child nodes.");


      let properties = testVar.addItems({ "child": { "value": { "type": "object",
                                                                "class": "Object" } } });


      ok(!testVar.expanded,
        "Any new created variable should be initially collapsed.");

      ok(testVar.visible,
        "Any new created variable should be initially visible.");


      testVar.expand();
      ok(testVar.expanded,
        "The testVar shouldn't be collapsed anymore.");

      testVar.collapse();
      ok(!testVar.expanded,
        "The testVar should be collapsed again.");

      testVar.expanded = true;
      ok(testVar.expanded,
        "The testVar shouldn't be collapsed anymore.");

      testVar.toggle();
      ok(!testVar.expanded,
        "The testVar should be collapsed again.");


      testVar.hide();
      ok(!testVar.visible,
        "The testVar should be invisible after hiding.");

      testVar.show();
      ok(testVar.visible,
        "The testVar should be visible again.");

      testVar.visible = false;
      ok(!testVar.visible,
        "The testVar should be invisible after hiding.");


      ok(!testVar.expanded,
        "The testVar should remember it is collapsed even if it is hidden.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
        testVar.target.querySelector(".name"),
        gDebugger);

      ok(testVar.expanded,
        "Clicking the testVar name should expand it.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
        testVar.target.querySelector(".name"),
        gDebugger);

      ok(!testVar.expanded,
        "Clicking again the testVar name should collapse it.");


      EventUtils.sendMouseEvent({ type: "mousedown" },
        testVar.target.querySelector(".arrow"),
        gDebugger);

      ok(testVar.expanded,
        "Clicking the testVar arrow should expand it.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
        testVar.target.querySelector(".arrow"),
        gDebugger);

      ok(!testVar.expanded,
        "Clicking again the testVar arrow should collapse it.");


      EventUtils.sendMouseEvent({ type: "mousedown" },
        testVar.target.querySelector(".title"),
        gDebugger);

      ok(testVar.expanded,
        "Clicking the testVar title div should expand it again.");


      testScope.show();
      testScope.expand();
      testVar.show();
      testVar.expand();

      ok(!testVar.get("child").expanded,
        "The testVar child property should remember it is collapsed even if it is hidden.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
        testVar.get("child").target.querySelector(".name"),
        gDebugger);

      ok(testVar.get("child").expanded,
        "Clicking the testVar child property name should expand it.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
        testVar.get("child").target.querySelector(".name"),
        gDebugger);

      ok(!testVar.get("child").expanded,
        "Clicking again the testVar child property name should collapse it.");


      EventUtils.sendMouseEvent({ type: "mousedown" },
        testVar.get("child").target.querySelector(".arrow"),
        gDebugger);

      ok(testVar.get("child").expanded,
        "Clicking the testVar child property arrow should expand it.");

      EventUtils.sendMouseEvent({ type: "mousedown" },
        testVar.get("child").target.querySelector(".arrow"),
        gDebugger);

      ok(!testVar.get("child").expanded,
        "Clicking again the testVar child property arrow should collapse it.");


      EventUtils.sendMouseEvent({ type: "mousedown" },
        testVar.get("child").target.querySelector(".title"),
        gDebugger);

      ok(testVar.get("child").expanded,
        "Clicking the testVar child property title div should expand it again.");


      gDebugger.DebuggerView.Variables.empty();
      is(gDebugger.DebuggerView.Variables._list.childNodes.length, 0,
        "The scopes should have been removed from the parent container tree.");

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
