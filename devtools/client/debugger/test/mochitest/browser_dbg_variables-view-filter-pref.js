/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view filter prefs work properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_with-frame.html";

var gTab, gPanel, gDebugger;
var gPrefs, gOptions, gVariables;

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gPrefs = gDebugger.Prefs;
    gOptions = gDebugger.DebuggerView.Options;
    gVariables = gDebugger.DebuggerView.Variables;

    performTest();
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

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gPrefs = null;
  gOptions = null;
  gVariables = null;
});
