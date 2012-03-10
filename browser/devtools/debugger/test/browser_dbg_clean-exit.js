/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that closing a tab with the debugger in a paused state exits cleanly.

var gPane = null;
var gTab = null;
var gDebugger = null;

const DEBUGGER_TAB_URL = EXAMPLE_URL + "browser_dbg_debuggerstatement.html";

function test() {
  debug_tab_pane(DEBUGGER_TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.debuggerWindow;

    testCleanExit();
  });
}

function testCleanExit() {
  gPane.activeThread.addOneTimeListener("framesadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {
      is(gDebugger.StackFrames.activeThread.paused, true,
        "Should be paused after the debugger statement.");

      closeDebuggerAndFinish(gTab);
    }}, 0);
  });

  gTab.linkedBrowser.contentWindow.wrappedJSObject.runDebuggerStatement();
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
});
