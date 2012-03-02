/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that switching the displayed script in the UI works as advertised.
 */

const TAB_URL = "http://example.com/browser/browser/devtools/debugger/" +
                "test/browser_dbg_script-switching.html";
let tempScope = {};
Cu.import("resource:///modules/source-editor.jsm", tempScope);
let SourceEditor = tempScope.SourceEditor;

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gScripts = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.debuggerWindow;

    testScriptsDisplay();
  });
}

function testScriptsDisplay() {
  gPane.activeThread.addOneTimeListener("scriptsadded", function() {
    Services.tm.currentThread.dispatch({ run: function() {
      gScripts = gDebugger.DebuggerView.Scripts._scripts;

      is(gDebugger.StackFrames.activeThread.state, "paused",
        "Should only be getting stack frames while paused.");

      is(gScripts.itemCount, 2, "Found the expected number of scripts.");

      ok(gDebugger.editor.getText().search(/debugger/) != -1,
        "The correct script was loaded initially.");

      gDebugger.editor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED,
                                        function onChange() {
        gDebugger.editor.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED,
                                             onChange);
        testSwitchPaused();
      });
      gScripts.selectedIndex = 0;
      gDebugger.SourceScripts.onChange({ target: gScripts });
    }}, 0);
  });

  gDebuggee.firstCall();
}

function testSwitchPaused()
{
  ok(gDebugger.editor.getText().search(/debugger/) == -1,
    "The second script is no longer displayed.");

  ok(gDebugger.editor.getText().search(/firstCall/) != -1,
    "The first script is displayed.");

  gDebugger.StackFrames.activeThread.resume(function() {
    gDebugger.editor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED,
                                      function onSecondChange() {
      gDebugger.editor.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED,
                                           onSecondChange);
      testSwitchRunning();
    });
    gScripts.selectedIndex = 1;
    gDebugger.SourceScripts.onChange({ target: gScripts });
  });
}

function testSwitchRunning()
{
  ok(gDebugger.editor.getText().search(/debugger/) != -1,
    "The second script is displayed again.");

  ok(gDebugger.editor.getText().search(/firstCall/) == -1,
    "The first script is no longer displayed.");

  removeTab(gTab);
  finish();
}
