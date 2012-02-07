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
    gDebugger = gPane.debuggerWindow;

    testSimpleCall();
  });
}

function testSimpleCall() {
  gPane.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      let globalScope = gDebugger.DebuggerView.Properties._globalScope;
      let localScope = gDebugger.DebuggerView.Properties._localScope;
      let withScope = gDebugger.DebuggerView.Properties._withScope;
      let closureScope = gDebugger.DebuggerView.Properties._closureScope;

      ok(globalScope,
        "Should have a globalScope container for the variables property view.");

      ok(localScope,
        "Should have a localScope container for the variables property view.");

      ok(withScope,
        "Should have a withScope container for the variables property view.");

      ok(closureScope,
        "Should have a closureScope container for the variables property view.");

      is(globalScope, gDebugger.DebuggerView.Properties.globalScope,
        "The globalScope object should be equal to the corresponding getter.");

      is(localScope, gDebugger.DebuggerView.Properties.localScope,
        "The localScope object should be equal to the corresponding getter.");

      is(withScope, gDebugger.DebuggerView.Properties.withScope,
        "The withScope object should be equal to the corresponding getter.");

      is(closureScope, gDebugger.DebuggerView.Properties.closureScope,
        "The closureScope object should be equal to the corresponding getter.");


      ok(!globalScope.expanded,
        "The globalScope should be initially collapsed.");

      ok(localScope.expanded,
        "The localScope should be initially expanded.");

      ok(!withScope.expanded,
        "The withScope should be initially collapsed.");

      ok(!withScope.visible,
        "The withScope should be initially hidden.");

      ok(!closureScope.expanded,
        "The closureScope should be initially collapsed.");

      ok(!closureScope.visible,
        "The closureScope should be initially hidden.");

      is(gDebugger.DebuggerView.Properties._vars.childNodes.length, 4,
        "Should have only 4 scopes created: global, local, with and scope.");

      resumeAndFinish();
    }}, 0);
  });

  gDebuggee.simpleCall();
}

function resumeAndFinish() {
  gDebugger.StackFrames.activeThread.resume(function() {
    removeTab(gTab);
    finish();
  });
}
