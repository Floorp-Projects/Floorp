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
      let duplVar = testScope.addVar("something");

      ok(testVar,
        "Should have created a variable.");

      is(duplVar, null,
        "Shouldn't be able to duplicate variables in the same scope.");

      is(testVar.id, "test-scope->something-variable",
        "The newly created scope should have the default id set.");

      is(testVar.querySelector(".name").textContent, "something",
        "Any new variable should have the designated title.");

      is(testVar.querySelector(".details").childNodes.length, 0,
        "Any new variable should have a details container with no child nodes.");


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

      EventUtils.sendMouseEvent({ type: "click" },
        testVar.querySelector(".title"),
        gDebugger);

      ok(testVar.expanded,
        "Clicking the testScope tilte should expand it.");

      EventUtils.sendMouseEvent({ type: "click" },
        testVar.querySelector(".title"),
        gDebugger);

      ok(!testVar.expanded,
        "Clicking again the testScope tilte should collapse it.");


      let emptyCallbackSender = null;
      let removeCallbackSender = null;

      testScope.onempty = function(sender) { emptyCallbackSender = sender; };
      testScope.onremove = function(sender) { removeCallbackSender = sender; };

      testScope.empty();
      is(emptyCallbackSender, testScope,
        "The emptyCallback wasn't called as it should.");

      is(testScope.querySelector(".details").childNodes.length, 0,
        "The scope should remove all it's details container tree children.");

      testScope.remove();
      is(removeCallbackSender, testScope,
        "The removeCallback wasn't called as it should.");

      is(gDebugger.DebuggerView.Properties._vars.childNodes.length, 4,
        "The scope should have been removed from the parent container tree.");

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
