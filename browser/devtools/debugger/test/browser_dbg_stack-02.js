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

    testEvalCall();
  });
}

function testEvalCall() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      let frames = gDebugger.DebuggerView.StackFrames._frames;
      let childNodes = frames.childNodes;

      is(gDebugger.DebuggerController.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 2,
        "Should have two frames.");

      is(childNodes.length, frames.querySelectorAll(".dbg-stackframe").length,
        "All children should be frames.");

      is(frames.querySelector("#stackframe-0 .dbg-stackframe-name").textContent,
        "(eval)", "Frame name should be (eval)");

      ok(frames.querySelector("#stackframe-0").classList.contains("selected"),
        "First frame should be selected by default.");

      ok(!frames.querySelector("#stackframe-1").classList.contains("selected"),
        "Second frame should not be selected.");


      EventUtils.sendMouseEvent({ type: "click" },
        frames.querySelector("#stackframe-1"),
        gDebugger);

      ok(!frames.querySelector("#stackframe-0").classList.contains("selected"),
         "First frame should not be selected after click.");

      ok(frames.querySelector("#stackframe-1").classList.contains("selected"),
         "Second frame should be selected after click.");


      EventUtils.sendMouseEvent({ type: "click" },
        frames.querySelector("#stackframe-0 .dbg-stackframe-name"),
        gDebugger);

      ok(frames.querySelector("#stackframe-0").classList.contains("selected"),
         "First frame should be selected after click inside the first frame.");

      ok(!frames.querySelector("#stackframe-1").classList.contains("selected"),
         "Second frame should not be selected after click inside the first frame.");

      gDebugger.DebuggerController.activeThread.resume(function() {
        closeDebuggerAndFinish(gTab);
      });
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
