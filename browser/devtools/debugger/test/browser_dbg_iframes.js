/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that iframes can be added as debuggees.

var gPane = null;
var gTab = null;

const TEST_URL = EXAMPLE_URL + "browser_dbg_iframes.html";

function test() {
  debug_tab_pane(TEST_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    let gDebugger = gPane.debuggerWindow;

    is(gDebugger.DebuggerController.activeThread.paused, false,
      "Should be running after debug_tab_pane.");

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      Services.tm.currentThread.dispatch({ run: function() {

        let frames = gDebugger.DebuggerView.StackFrames._frames;
        let childNodes = frames.childNodes;

        is(gDebugger.DebuggerController.activeThread.paused, true,
          "Should be paused after an interrupt request.");

        is(frames.querySelectorAll(".dbg-stackframe").length, 1,
          "Should have one frame in the stack.");

        gDebugger.DebuggerController.activeThread.addOneTimeListener("resumed", function() {
          Services.tm.currentThread.dispatch({ run: function() {
            closeDebuggerAndFinish(gTab);
          }}, 0);
        });

        EventUtils.sendMouseEvent({ type: "click" },
          gDebugger.document.getElementById("resume"),
          gDebugger);
      }}, 0);
    });

    let iframe = gTab.linkedBrowser.contentWindow.wrappedJSObject.frames[0];

    is(iframe.document.title, "Browser Debugger Test Tab", "Found the iframe");

    iframe.runDebuggerStatement();
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
});
