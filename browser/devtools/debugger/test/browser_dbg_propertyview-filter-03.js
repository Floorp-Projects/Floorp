/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view filter prefs work properly.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_with-frame.html";

var gPane = null;
var gTab = null;
var gDebugger = null;
var gDebuggee = null;
var gPrevPref = null;

function test()
{
  gPrevPref = Services.prefs.getBoolPref(
    "devtools.debugger.ui.variables-searchbox-visible");
  Services.prefs.setBoolPref(
    "devtools.debugger.ui.variables-searchbox-visible", false);

  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gDebuggee = aDebuggee;

    testSearchbox();
    testPref();
  });
}

function testSearchbox()
{
  ok(!gDebugger.DebuggerView.Variables._searchboxNode,
    "There should not initially be a searchbox available in the variables view.");
  ok(!gDebugger.DebuggerView.Variables._parent.parentNode.querySelector(".variables-searchinput.devtools-searchinput"),
    "There searchbox element should not be found.");
}

function testPref()
{
  is(gDebugger.Prefs.variablesSearchboxVisible, false,
    "The debugger searchbox should be preffed as hidden.");
  isnot(gDebugger.DebuggerView.Options._showVariablesFilterBoxItem.getAttribute("checked"), "true",
    "The options menu item should not be checked.");

  gDebugger.DebuggerView.Options._showVariablesFilterBoxItem.setAttribute("checked", "true");
  gDebugger.DebuggerView.Options._toggleShowVariablesFilterBox();

  executeSoon(function() {
    ok(gDebugger.DebuggerView.Variables._searchboxNode,
      "There should be a searchbox available in the variables view.");
    ok(gDebugger.DebuggerView.Variables._parent.parentNode.querySelector(".variables-searchinput.devtools-searchinput"),
      "There searchbox element should be found.");
    is(gDebugger.Prefs.variablesSearchboxVisible, true,
      "The debugger searchbox should now be preffed as visible.");
    is(gDebugger.DebuggerView.Options._showVariablesFilterBoxItem.getAttribute("checked"), "true",
      "The options menu item should now be checked.");

    gDebugger.DebuggerView.Options._showVariablesFilterBoxItem.setAttribute("checked", "false");
    gDebugger.DebuggerView.Options._toggleShowVariablesFilterBox();

    executeSoon(function() {
      ok(!gDebugger.DebuggerView.Variables._searchboxNode,
        "There should not be a searchbox available in the variables view.");
      ok(!gDebugger.DebuggerView.Variables._parent.parentNode.querySelector(".variables-searchinput.devtools-searchinput"),
        "There searchbox element should not be found.");
      is(gDebugger.Prefs.variablesSearchboxVisible, false,
        "The debugger searchbox should now be preffed as hidden.");
      isnot(gDebugger.DebuggerView.Options._showVariablesFilterBoxItem.getAttribute("checked"), "true",
        "The options menu item should now be unchecked.");

      executeSoon(function() {
        Services.prefs.setBoolPref(
          "devtools.debugger.ui.variables-searchbox-visible", gPrevPref);

        closeDebuggerAndFinish();
      });
    });
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
  gDebuggee = null;
  gPrevPref = null;
});
