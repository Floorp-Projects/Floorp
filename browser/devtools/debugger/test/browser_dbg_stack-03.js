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

    testRecurse();
  });
}

function testRecurse() {
  gDebuggee.gRecurseLimit = (gDebugger.StackFrames.pageSize * 2) + 1;

  gPane.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      let frames = gDebugger.DebuggerView.Stackframes._frames;
      let pageSize = gDebugger.StackFrames.pageSize;
      let recurseLimit = gDebuggee.gRecurseLimit;
      let childNodes = frames.childNodes;

      is(frames.querySelectorAll(".dbg-stackframe").length, pageSize,
        "Should have the max limit of frames.");

      is(childNodes.length, frames.querySelectorAll(".dbg-stackframe").length,
        "All children should be frames.");


      gPane.activeThread.addOneTimeListener("framesadded", function() {

        is(frames.querySelectorAll(".dbg-stackframe").length, pageSize * 2,
          "Should now have twice the max limit of frames.");

        gPane.activeThread.addOneTimeListener("framesadded", function() {
          is(frames.querySelectorAll(".dbg-stackframe").length, recurseLimit,
            "Should have reached the recurse limit.");

          gDebugger.StackFrames.activeThread.resume(function() {
            closeDebuggerAndFinish(gTab);
          });
        });

        frames.scrollTop = frames.scrollHeight;
      });

      frames.scrollTop = frames.scrollHeight;
    }}, 0);
  });

  gDebuggee.recurse();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
});
