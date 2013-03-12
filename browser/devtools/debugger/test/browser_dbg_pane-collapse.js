/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the debugger panes collapse properly.

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gView = null;

function test() {
  debug_tab_pane(STACK_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gView = gDebugger.DebuggerView;

    testPanesState();

    gView.toggleInstrumentsPane({ visible: true, animated: false });
    testInstrumentsPaneCollapse();
    testPanesStartupPref();
  });
}

function testPanesState() {
  let instrumentsPane =
    gDebugger.document.getElementById("instruments-pane");
  let instrumentsPaneToggleButton =
    gDebugger.document.getElementById("instruments-pane-toggle");

  ok(instrumentsPane.hasAttribute("pane-collapsed") &&
     instrumentsPaneToggleButton.hasAttribute("pane-collapsed"),
    "The debugger view instruments pane should initially be hidden.");
  is(gDebugger.Prefs.panesVisibleOnStartup, false,
    "The debugger view instruments pane should initially be preffed as hidden.");
  isnot(gDebugger.DebuggerView.Options._showPanesOnStartupItem.getAttribute("checked"), "true",
    "The options menu item should not be checked.");
}

function testInstrumentsPaneCollapse() {
  let instrumentsPane =
    gDebugger.document.getElementById("instruments-pane");
  let instrumentsPaneToggleButton =
    gDebugger.document.getElementById("instruments-pane-toggle");

  let width = parseInt(instrumentsPane.getAttribute("width"));
  is(width, gDebugger.Prefs.instrumentsWidth,
    "The instruments pane has an incorrect width.");
  is(instrumentsPane.style.marginLeft, "0px",
    "The instruments pane has an incorrect left margin.");
  is(instrumentsPane.style.marginRight, "0px",
    "The instruments pane has an incorrect right margin.");
  ok(!instrumentsPane.hasAttribute("animated"),
    "The instruments pane has an incorrect animated attribute.");
  ok(!instrumentsPane.hasAttribute("pane-collapsed") &&
     !instrumentsPaneToggleButton.hasAttribute("pane-collapsed"),
    "The instruments pane should at this point be visible.");

  gView.toggleInstrumentsPane({ visible: false, animated: true });

  is(gDebugger.Prefs.panesVisibleOnStartup, false,
    "The debugger view panes should still initially be preffed as hidden.");
  isnot(gDebugger.DebuggerView.Options._showPanesOnStartupItem.getAttribute("checked"), "true",
    "The options menu item should still not be checked.");

  let margin = -(width + 1) + "px";
  is(width, gDebugger.Prefs.instrumentsWidth,
    "The instruments pane has an incorrect width after collapsing.");
  is(instrumentsPane.style.marginLeft, margin,
    "The instruments pane has an incorrect left margin after collapsing.");
  is(instrumentsPane.style.marginRight, margin,
    "The instruments pane has an incorrect right margin after collapsing.");
  ok(instrumentsPane.hasAttribute("animated"),
    "The instruments pane has an incorrect attribute after an animated collapsing.");
  ok(instrumentsPane.hasAttribute("pane-collapsed") &&
     instrumentsPaneToggleButton.hasAttribute("pane-collapsed"),
    "The instruments pane should not be visible after collapsing.");

  gView.toggleInstrumentsPane({ visible: true, animated: false });

  is(gDebugger.Prefs.panesVisibleOnStartup, false,
    "The debugger view panes should still initially be preffed as hidden.");
  isnot(gDebugger.DebuggerView.Options._showPanesOnStartupItem.getAttribute("checked"), "true",
    "The options menu item should still not be checked.");

  is(width, gDebugger.Prefs.instrumentsWidth,
    "The instruments pane has an incorrect width after uncollapsing.");
  is(instrumentsPane.style.marginLeft, "0px",
    "The instruments pane has an incorrect left margin after uncollapsing.");
  is(instrumentsPane.style.marginRight, "0px",
    "The instruments pane has an incorrect right margin after uncollapsing.");
  ok(!instrumentsPane.hasAttribute("animated"),
    "The instruments pane has an incorrect attribute after an unanimated uncollapsing.");
  ok(!instrumentsPane.hasAttribute("pane-collapsed") &&
     !instrumentsPaneToggleButton.hasAttribute("pane-collapsed"),
    "The instruments pane should be visible again after uncollapsing.");
}

function testPanesStartupPref() {
  let instrumentsPane =
    gDebugger.document.getElementById("instruments-pane");
  let instrumentsPaneToggleButton =
    gDebugger.document.getElementById("instruments-pane-toggle");

  is(gDebugger.Prefs.panesVisibleOnStartup, false,
    "The debugger view panes should still initially be preffed as hidden.");

  ok(!instrumentsPane.hasAttribute("pane-collapsed") &&
     !instrumentsPaneToggleButton.hasAttribute("pane-collapsed"),
    "The debugger instruments pane should at this point be visible.");
  is(gDebugger.Prefs.panesVisibleOnStartup, false,
    "The debugger view panes should initially be preffed as hidden.");
  isnot(gDebugger.DebuggerView.Options._showPanesOnStartupItem.getAttribute("checked"), "true",
    "The options menu item should still not be checked.");

  gDebugger.DebuggerView.Options._showPanesOnStartupItem.setAttribute("checked", "true");
  gDebugger.DebuggerView.Options._toggleShowPanesOnStartup();

  executeSoon(function() {
    ok(!instrumentsPane.hasAttribute("pane-collapsed") &&
       !instrumentsPaneToggleButton.hasAttribute("pane-collapsed"),
      "The debugger instruments pane should at this point be visible.");
    is(gDebugger.Prefs.panesVisibleOnStartup, true,
      "The debugger view panes should now be preffed as visible.");
    is(gDebugger.DebuggerView.Options._showPanesOnStartupItem.getAttribute("checked"), "true",
      "The options menu item should now be checked.");

    gDebugger.DebuggerView.Options._showPanesOnStartupItem.setAttribute("checked", "false");
    gDebugger.DebuggerView.Options._toggleShowPanesOnStartup();

    executeSoon(function() {
      ok(!instrumentsPane.hasAttribute("pane-collapsed") &&
         !instrumentsPaneToggleButton.hasAttribute("pane-collapsed"),
        "The debugger instruments pane should at this point be visible.");
      is(gDebugger.Prefs.panesVisibleOnStartup, false,
        "The debugger view panes should now be preffed as hidden.");
      isnot(gDebugger.DebuggerView.Options._showPanesOnStartupItem.getAttribute("checked"), "true",
        "The options menu item should now be unchecked.");

      executeSoon(function() {
        closeDebuggerAndFinish();
      });
    });
  });
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
  gView = null;
});
