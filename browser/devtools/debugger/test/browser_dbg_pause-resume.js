/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gPane = null;
var gTab = null;
var gDebugger = null;

function test() {
  debug_tab_pane(STACK_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.contentWindow;

    testPause();
  });
}

function testPause() {
  is(gDebugger.DebuggerController.activeThread.paused, false,
    "Should be running after debug_tab_pane.");

  let button = gDebugger.document.getElementById("resume");
  is(button.label, gDebugger.L10N.getStr("pauseLabel"),
    "Button label should be pause when running.");

  gDebugger.DebuggerController.activeThread.addOneTimeListener("paused", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      let frames = gDebugger.DebuggerView.StackFrames._frames;
      let childNodes = frames.childNodes;

      is(gDebugger.DebuggerController.activeThread.paused, true,
        "Should be paused after an interrupt request.");

      is(button.label, gDebugger.L10N.getStr("resumeLabel"),
        "Button label should be resume when paused.");

      is(frames.querySelectorAll(".dbg-stackframe").length, 0,
        "Should have no frames when paused in the main loop.");

      testResume();
    }}, 0);
  });

  EventUtils.sendMouseEvent({ type: "click" },
    gDebugger.document.getElementById("resume"),
    gDebugger);
}

function testResume() {
  gDebugger.DebuggerController.activeThread.addOneTimeListener("resumed", function() {
    Services.tm.currentThread.dispatch({ run: function() {

      is(gDebugger.DebuggerController.activeThread.paused, false,
        "Should be paused after an interrupt request.");

      let button = gDebugger.document.getElementById("resume");
      is(button.label, gDebugger.L10N.getStr("pauseLabel"),
        "Button label should be pause when running.");

      closeDebuggerAndFinish(gTab);
    }}, 0);
  });

  EventUtils.sendMouseEvent({ type: "click" },
    gDebugger.document.getElementById("resume"),
    gDebugger);
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
});
