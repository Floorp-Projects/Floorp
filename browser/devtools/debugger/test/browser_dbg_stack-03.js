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

    testRecurse();
  });
}

function testRecurse() {
  gDebuggee.gRecurseLimit = (gDebugger.gCallStackPageSize * 2) + 1;

  gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      let frames = gDebugger.DebuggerView.StackFrames.widget._list;
      let pageSize = gDebugger.gCallStackPageSize;
      let recurseLimit = gDebuggee.gRecurseLimit;
      let childNodes = frames.childNodes;

      is(frames.querySelectorAll(".dbg-stackframe").length, pageSize,
        "Should have the max limit of frames.");

      is(childNodes.length, frames.querySelectorAll(".dbg-stackframe").length,
        "All children should be frames.");


      gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
        is(frames.querySelectorAll(".dbg-stackframe").length, pageSize * 2,
          "Should now have twice the max limit of frames.");

        gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
          is(frames.querySelectorAll(".dbg-stackframe").length, recurseLimit,
            "Should have reached the recurse limit.");

          gDebugger.DebuggerController.activeThread.resume(function() {
            window.clearInterval(scrollingInterval);
            closeDebuggerAndFinish();
          });
        });
      });

      let scrollingInterval = window.setInterval(function() {
        frames.scrollByIndex(-1);
      }, 100);
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
