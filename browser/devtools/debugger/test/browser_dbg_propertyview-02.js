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

      ok(testScope,
        "Should have created a scope.");

      is(testScope.id, "test-scope",
        "The newly created scope should have the default id set.");

      is(testScope.querySelector(".name").textContent, "test",
        "Any new scope should have the designated title.");

      is(testScope.querySelector(".details").childNodes.length, 0,
        "Any new scope should have a container with no child nodes.");

      is(gDebugger.DebuggerView.Properties._vars.childNodes.length, 5,
        "Should have 5 scopes created: global, local, with, closure and test.");


      ok(!testScope.expanded,
        "Any new created scope should be initially collapsed.");

      ok(testScope.visible,
        "Any new created scope should be initially visible.");

      let expandCallbackSender = null;
      let collapseCallbackSender = null;
      let toggleCallbackSender = null;
      let hideCallbackSender = null;
      let showCallbackSender = null;

      testScope.onexpand = function(sender) { expandCallbackSender = sender; };
      testScope.oncollapse = function(sender) { collapseCallbackSender = sender; };
      testScope.ontoggle = function(sender) { toggleCallbackSender = sender; };
      testScope.onhide = function(sender) { hideCallbackSender = sender; };
      testScope.onshow = function(sender) { showCallbackSender = sender; };

      testScope.expand();
      ok(testScope.expanded,
        "The testScope shouldn't be collapsed anymore.");
      is(expandCallbackSender, testScope,
        "The expandCallback wasn't called as it should.");

      testScope.collapse();
      ok(!testScope.expanded,
        "The testScope should be collapsed again.");
      is(collapseCallbackSender, testScope,
        "The collapseCallback wasn't called as it should.");

      testScope.expanded = true;
      ok(testScope.expanded,
        "The testScope shouldn't be collapsed anymore.");

      testScope.toggle();
      ok(!testScope.expanded,
        "The testScope should be collapsed again.");
      is(toggleCallbackSender, testScope,
        "The toggleCallback wasn't called as it should.");


      testScope.hide();
      ok(!testScope.visible,
        "The testScope should be invisible after hiding.");
      is(hideCallbackSender, testScope,
        "The hideCallback wasn't called as it should.");

      testScope.show();
      ok(testScope.visible,
        "The testScope should be visible again.");
      is(showCallbackSender, testScope,
        "The showCallback wasn't called as it should.");

      testScope.visible = false;
      ok(!testScope.visible,
        "The testScope should be invisible after hiding.");


      ok(!testScope.expanded,
        "The testScope should remember it is collapsed even if it is hidden.");

      EventUtils.sendMouseEvent({ type: "click" },
        testScope.querySelector(".title"),
        gDebugger);

      ok(testScope.expanded,
        "Clicking the testScope tilte should expand it.");

      EventUtils.sendMouseEvent({ type: "click" },
        testScope.querySelector(".title"),
        gDebugger);

      ok(!testScope.expanded,
        "Clicking again the testScope tilte should collapse it.");

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
