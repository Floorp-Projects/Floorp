/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the debugger panes collapse properly.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

var gTab, gPanel, gDebugger;
var gPrefs, gOptions;

function test() {
  Task.spawn(function* () {
    let options = {
      source: TAB_URL,
      line: 1
    };

    let [aTab,, aPanel] = yield initDebugger(TAB_URL, options);

    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gPrefs = gDebugger.Prefs;
    gOptions = gDebugger.DebuggerView.Options;

    testPanesState();

    gDebugger.DebuggerView.toggleInstrumentsPane({ visible: true, animated: false });

    yield testInstrumentsPaneCollapse();
    testPanesStartupPref();

    closeDebuggerAndFinish(gPanel);
  });
}

function testPanesState() {
  let instrumentsPane =
    gDebugger.document.getElementById("instruments-pane");
  let instrumentsPaneToggleButton =
    gDebugger.document.getElementById("instruments-pane-toggle");

  ok(instrumentsPane.classList.contains("pane-collapsed") &&
     instrumentsPaneToggleButton.classList.contains("pane-collapsed"),
    "The debugger view instruments pane should initially be hidden.");
  is(gPrefs.panesVisibleOnStartup, false,
    "The debugger view instruments pane should initially be preffed as hidden.");
  isnot(gOptions._showPanesOnStartupItem.getAttribute("checked"), "true",
    "The options menu item should not be checked.");
}

function* testInstrumentsPaneCollapse () {
  let instrumentsPane =
    gDebugger.document.getElementById("instruments-pane");
  let instrumentsPaneToggleButton =
    gDebugger.document.getElementById("instruments-pane-toggle");

  let width = parseInt(instrumentsPane.getAttribute("width"));
  is(width, gPrefs.instrumentsWidth,
    "The instruments pane has an incorrect width.");
  is(instrumentsPane.style.marginLeft, "0px",
    "The instruments pane has an incorrect left margin.");
  is(instrumentsPane.style.marginRight, "0px",
    "The instruments pane has an incorrect right margin.");
  ok(!instrumentsPane.hasAttribute("animated"),
    "The instruments pane has an incorrect animated attribute.");
  ok(!instrumentsPane.classList.contains("pane-collapsed") &&
     !instrumentsPaneToggleButton.classList.contains("pane-collapsed"),
    "The instruments pane should at this point be visible.");

  // Trigger reflow to make sure the UI is in required state.
  gDebugger.document.documentElement.getBoundingClientRect();

  gDebugger.DebuggerView.toggleInstrumentsPane({ visible: false, animated: true });

  yield once(instrumentsPane, "transitionend");

  is(gPrefs.panesVisibleOnStartup, false,
    "The debugger view panes should still initially be preffed as hidden.");
  isnot(gOptions._showPanesOnStartupItem.getAttribute("checked"), "true",
    "The options menu item should still not be checked.");

  let margin = -(width + 1) + "px";
  is(width, gPrefs.instrumentsWidth,
    "The instruments pane has an incorrect width after collapsing.");
  is(instrumentsPane.style.marginLeft, margin,
    "The instruments pane has an incorrect left margin after collapsing.");
  is(instrumentsPane.style.marginRight, margin,
    "The instruments pane has an incorrect right margin after collapsing.");

  ok(!instrumentsPane.hasAttribute("animated"),
    "The instruments pane has an incorrect attribute after an animated collapsing.");
  ok(instrumentsPane.classList.contains("pane-collapsed") &&
     instrumentsPaneToggleButton.classList.contains("pane-collapsed"),
    "The instruments pane should not be visible after collapsing.");

  gDebugger.DebuggerView.toggleInstrumentsPane({ visible: true, animated: false });

  is(gPrefs.panesVisibleOnStartup, false,
    "The debugger view panes should still initially be preffed as hidden.");
  isnot(gOptions._showPanesOnStartupItem.getAttribute("checked"), "true",
    "The options menu item should still not be checked.");

  is(width, gPrefs.instrumentsWidth,
    "The instruments pane has an incorrect width after uncollapsing.");
  is(instrumentsPane.style.marginLeft, "0px",
    "The instruments pane has an incorrect left margin after uncollapsing.");
  is(instrumentsPane.style.marginRight, "0px",
    "The instruments pane has an incorrect right margin after uncollapsing.");
  ok(!instrumentsPane.hasAttribute("animated"),
    "The instruments pane has an incorrect attribute after an unanimated uncollapsing.");
  ok(!instrumentsPane.classList.contains("pane-collapsed") &&
     !instrumentsPaneToggleButton.classList.contains("pane-collapsed"),
    "The instruments pane should be visible again after uncollapsing.");
}

function testPanesStartupPref() {
  let instrumentsPane =
    gDebugger.document.getElementById("instruments-pane");
  let instrumentsPaneToggleButton =
    gDebugger.document.getElementById("instruments-pane-toggle");

  is(gPrefs.panesVisibleOnStartup, false,
    "The debugger view panes should still initially be preffed as hidden.");

  ok(!instrumentsPane.classList.contains("pane-collapsed") &&
     !instrumentsPaneToggleButton.classList.contains("pane-collapsed"),
    "The debugger instruments pane should at this point be visible.");
  is(gPrefs.panesVisibleOnStartup, false,
    "The debugger view panes should initially be preffed as hidden.");
  isnot(gOptions._showPanesOnStartupItem.getAttribute("checked"), "true",
    "The options menu item should still not be checked.");

  gOptions._showPanesOnStartupItem.setAttribute("checked", "true");
  gOptions._toggleShowPanesOnStartup();

  ok(!instrumentsPane.classList.contains("pane-collapsed") &&
     !instrumentsPaneToggleButton.classList.contains("pane-collapsed"),
    "The debugger instruments pane should at this point be visible.");
  is(gPrefs.panesVisibleOnStartup, true,
    "The debugger view panes should now be preffed as visible.");
  is(gOptions._showPanesOnStartupItem.getAttribute("checked"), "true",
    "The options menu item should now be checked.");

  gOptions._showPanesOnStartupItem.setAttribute("checked", "false");
  gOptions._toggleShowPanesOnStartup();

  ok(!instrumentsPane.classList.contains("pane-collapsed") &&
     !instrumentsPaneToggleButton.classList.contains("pane-collapsed"),
    "The debugger instruments pane should at this point be visible.");
  is(gPrefs.panesVisibleOnStartup, false,
    "The debugger view panes should now be preffed as hidden.");
  isnot(gOptions._showPanesOnStartupItem.getAttribute("checked"), "true",
    "The options menu item should now be unchecked.");
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gPrefs = null;
  gOptions = null;
});
