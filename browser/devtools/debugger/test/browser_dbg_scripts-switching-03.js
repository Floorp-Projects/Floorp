/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the Debugger-View error loading source text is correct
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gView = null;
var gL10N = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane){
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gView = gDebugger.DebuggerView;
    gL10N = gDebugger.L10N;

    gDebugger.addEventListener("Debugger:SourceShown", onScriptShown);
  });
}

function onScriptShown(aEvent)
{
  gView.editorSource = { url: "http://example.com/fake.js", actor: "fake.actor" };
  gDebugger.removeEventListener("Debugger:SourceShown", onScriptShown);
  gDebugger.addEventListener("Debugger:SourceErrorShown", onScriptErrorShown);
}

function onScriptErrorShown(aEvent)
{
  gDebugger.removeEventListener("Debugger:SourceErrorShown", onScriptErrorShown);
  testDebuggerLoadingError();
}

function testDebuggerLoadingError()
{
  ok(gDebugger.editor.getText().search(new RegExp(gL10N.getStr("errorLoadingText")) != -1),
    "The valid error loading message is displayed.");
  closeDebuggerAndFinish();
}

registerCleanupFunction(function()
{
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
  gView = null;
  gDebuggee = null;
  gL10N = null;
});
