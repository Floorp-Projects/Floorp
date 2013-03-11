/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gSearchBox = null;
var gSearchBoxPanel = null;

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    runTest();
  });
}

function runTest() {
  gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;
  gSearchBoxPanel = gDebugger.DebuggerView.Filtering._searchboxHelpPanel;

  focusSearchbox();
}

function focusSearchbox() {
  is(gSearchBoxPanel.state, "closed",
    "The search box panel shouldn't be visible yet.");

  gSearchBoxPanel.addEventListener("popupshown", function _onEvent(aEvent) {
    gSearchBoxPanel.removeEventListener(aEvent.type, _onEvent);

    is(gSearchBoxPanel.state, "open",
      "The search box panel should be visible after searching started.");

    closeDebuggerAndFinish();
  });

  EventUtils.sendMouseEvent({ type: "click" }, gSearchBox);
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
  gSearchBox = null;
  gSearchBoxPanel = null;
});
