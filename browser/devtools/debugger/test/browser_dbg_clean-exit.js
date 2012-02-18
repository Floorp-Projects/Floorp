/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that closing a tab with the debugger in a paused state exits cleanly.

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;

const DEBUGGER_TAB_URL = "http://example.com/browser/browser/devtools/" +
                         "debugger/test/" +
                         "browser_dbg_debuggerstatement.html";

function test() {
  debug_tab_pane(DEBUGGER_TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
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

      gPane._client.addOneTimeListener("tabDetached", function () {
        finish();
      });
      removeTab(gTab);
    }}, 0);
  });

  gTab.linkedBrowser.contentWindow.wrappedJSObject.runDebuggerStatement();
}
