/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view filter prefs work properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_with-frame.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gPrefs, gOptions, gVariables;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gPrefs = gDebugger.Prefs;
    gOptions = gDebugger.DebuggerView.Options;
    gVariables = gDebugger.DebuggerView.Variables;

    waitForSourceShown(gPanel, ".html").then(performTest);
  });
}

function performTest() {
  ok(!gVariables._searchboxNode,
    "There should not initially be a searchbox available in the variables view.");
  ok(!gVariables._searchboxContainer,
    "There should not initially be a searchbox container available in the variables view.");
  ok(!gVariables._parent.parentNode.querySelector(".variables-view-searchinput"),
    "The searchbox element should not be found.");

  is(gPrefs.variablesSearchboxVisible, false,
    "The debugger searchbox should be preffed as hidden.");
  isnot(gOptions._showVariablesFilterBoxItem.getAttribute("checked"), "true",
    "The options menu item should not be checked.");

  gOptions._showVariablesFilterBoxItem.setAttribute("checked", "true");
  gOptions._toggleShowVariablesFilterBox();

  ok(gVariables._searchboxNode,
    "There should be a searchbox available in the variables view.");
  ok(gVariables._searchboxContainer,
    "There should be a searchbox container available in the variables view.");
  ok(gVariables._parent.parentNode.querySelector(".variables-view-searchinput"),
    "There searchbox element should be found.");

  is(gPrefs.variablesSearchboxVisible, true,
    "The debugger searchbox should now be preffed as visible.");
  is(gOptions._showVariablesFilterBoxItem.getAttribute("checked"), "true",
    "The options menu item should now be checked.");

  gOptions._showVariablesFilterBoxItem.setAttribute("checked", "false");
  gOptions._toggleShowVariablesFilterBox();

  ok(!gVariables._searchboxNode,
    "There should not be a searchbox available in the variables view.");
  ok(!gVariables._searchboxContainer,
    "There should not be a searchbox container available in the variables view.");
  ok(!gVariables._parent.parentNode.querySelector(".variables-view-searchinput"),
    "There searchbox element should not be found.");

  is(gPrefs.variablesSearchboxVisible, false,
    "The debugger searchbox should now be preffed as hidden.");
  isnot(gOptions._showVariablesFilterBoxItem.getAttribute("checked"), "true",
    "The options menu item should now be unchecked.");

  closeDebuggerAndFinish(gPanel);
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gPrefs = null;
  gOptions = null;
  gVariables = null;
});
