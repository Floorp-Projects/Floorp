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

    testEvalCallResume();
  });
}

function testEvalCallResume() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      let frames = gDebugger.DebuggerView.StackFrames.widget._list;
      let childNodes = frames.childNodes;

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 2,
        "Should have two frames.");

      is(childNodes.length, frames.querySelectorAll(".dbg-stackframe").length,
        "All children should be frames.");


      gDebugger.addEventListener("Debugger:AfterFramesCleared", function listener() {
        gDebugger.removeEventListener("Debugger:AfterFramesCleared", listener, true);

        is(frames.querySelectorAll(".dbg-stackframe").length, 0,
          "Should have no frames after resume");

        is(childNodes.length, 0,
          "Should only have no children.");

        closeDebuggerAndFinish();
      }, true);

      gDebugger.DebuggerController.activeThread.resume();
    }}, 0);
  });

  gDebuggee.evalCall();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
