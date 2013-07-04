/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that changing the tab location URL works.
 */

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;

function test()
{
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
    Services.tm.currentThread.dispatch({
      run: function() {
        var frames = gDebugger.DebuggerView.StackFrames.widget._list,
            childNodes = frames.childNodes;

        is(gDebugger.DebuggerController.activeThread.state, "paused",
          "Should only be getting stack frames while paused.");

        is(frames.querySelectorAll(".dbg-stackframe").length, 1,
          "Should have only one frame.");

        is(childNodes.length, frames.querySelectorAll(".dbg-stackframe").length,
          "All children should be frames.");

        testLocationChange();
      }
    }, 0);
  });

  gDebuggee.simpleCall();
}

function testLocationChange()
{
  gDebugger.DebuggerController.activeThread.resume(function() {
    gDebugger.DebuggerController._target.once("navigate", function onTabNavigated(aEvent, aPacket) {
      ok(true, "tabNavigated event was fired.");
      info("Still attached to the tab.");

      closeDebuggerAndFinish();
    });
    content.location = TAB1_URL;
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
