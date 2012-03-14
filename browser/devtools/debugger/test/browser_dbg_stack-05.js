/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that switching between stack frames properly sets the current debugger
// location in the source editor.

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;

function test() {
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.debuggerWindow;

    testRecurse();
  });
}

function testRecurse() {
  gPane.activeThread.addOneTimeListener("scriptsadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {
      let frames = gDebugger.DebuggerView.Stackframes._frames;
      let childNodes = frames.childNodes;

      is(frames.querySelectorAll(".dbg-stackframe").length, 4,
        "Correct number of frames.");

      is(childNodes.length, frames.querySelectorAll(".dbg-stackframe").length,
        "All children should be frames.");

      ok(frames.querySelector("#stackframe-0").classList.contains("selected"),
        "First frame should be selected by default.");

      ok(!frames.querySelector("#stackframe-2").classList.contains("selected"),
        "Third frame should not be selected.");

      is(gDebugger.editor.getDebugLocation(), 5,
         "editor debugger location is correct.");

      EventUtils.sendMouseEvent({ type: "click" },
        frames.querySelector("#stackframe-2"),
        gDebugger);

      ok(!frames.querySelector("#stackframe-0").classList.contains("selected"),
         "First frame should not be selected after click.");

      ok(frames.querySelector("#stackframe-2").classList.contains("selected"),
         "Third frame should be selected after click.");

      is(gDebugger.editor.getDebugLocation(), 4,
         "editor debugger location is correct after click.");

      EventUtils.sendMouseEvent({ type: "click" },
        frames.querySelector("#stackframe-0 .dbg-stackframe-name"),
        gDebugger);

      ok(frames.querySelector("#stackframe-0").classList.contains("selected"),
         "First frame should be selected after click inside the first frame.");

      ok(!frames.querySelector("#stackframe-2").classList.contains("selected"),
         "Third frame should not be selected after click inside the first frame.");

      is(gDebugger.editor.getDebugLocation(), 5,
         "editor debugger location is correct (frame 0 again).");

      gDebugger.StackFrames.activeThread.resume(function() {
        is(gDebugger.editor.getDebugLocation(), -1,
           "editor debugger location is correct after resume.");
        closeDebuggerAndFinish(gTab);
      });
    }}, 0);
  });

  gDebuggee.firstCall();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
