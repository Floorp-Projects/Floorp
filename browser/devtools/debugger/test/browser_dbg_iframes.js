/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that iframes can be added as debuggees.

var gPane = null;
var gTab = null;
var gDebugger = null;

const TEST_URL = EXAMPLE_URL + "browser_dbg_iframes.html";

function test() {
  debug_tab_pane(TEST_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    is(gDebugger.DebuggerController.activeThread.paused, false,
      "Should be running after debug_tab_pane.");

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      Services.tm.currentThread.dispatch({ run: function() {

        let frames = gDebugger.DebuggerView.StackFrames.widget._list;
        let childNodes = frames.childNodes;

        is(gDebugger.DebuggerController.activeThread.paused, true,
          "Should be paused after an interrupt request.");

        is(frames.querySelectorAll(".dbg-stackframe").length, 1,
          "Should have one frame in the stack.");

        gDebugger.DebuggerController.activeThread.addOneTimeListener("resumed", function() {
          Services.tm.currentThread.dispatch({ run: function() {
            closeDebuggerAndFinish();
          }}, 0);
        });

        EventUtils.sendMouseEvent({ type: "mousedown" },
          gDebugger.document.getElementById("resume"),
          gDebugger);
      }}, 0);
    });

    let iframe = gTab.linkedBrowser.contentWindow.wrappedJSObject.frames[0];
    is(iframe.document.title, "Browser Debugger Test Tab", "Found the iframe");

    function handler() {
      if (iframe.document.readyState != "complete") {
        return;
      }
      iframe.window.removeEventListener("load", handler, false);
      executeSoon(iframe.runDebuggerStatement);
    };
    iframe.window.addEventListener("load", handler, false);
    handler();
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
});
