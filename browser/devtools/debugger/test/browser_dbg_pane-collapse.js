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
    gDebugger = gPane.contentWindow;
    gView = gDebugger.DebuggerView;

    testPanesState();

    gView.toggleStackframesAndBreakpointsPane({ visible: true });
    gView.toggleVariablesPane({ visible: true });
    testPaneCollapse1();
    testPaneCollapse2();

    closeDebuggerAndFinish();
  });
}

function testPanesState() {
  let togglePanesButton =
    gDebugger.document.getElementById("toggle-panes");

  ok(togglePanesButton.getAttribute("stackframesAndBreakpointsHidden"),
    "The stackframes and breakpoints pane should initially be invisible.");
  is(gDebugger.Prefs.stackframesPaneVisible, true,
    "The stackframes and breakpoints pane should initially be preffed as visible.");

  ok(togglePanesButton.getAttribute("variablesHidden"),
    "The stackframes and breakpoints pane should initially be invisible.");
  is(gDebugger.Prefs.variablesPaneVisible, true,
    "The stackframes and breakpoints pane should initially be preffed as visible.");
}

function testPaneCollapse1() {
  let stackframesAndBrekpoints =
    gDebugger.document.getElementById("stackframes+breakpoints");
  let togglePanesButton =
    gDebugger.document.getElementById("toggle-panes");

  let width = parseInt(stackframesAndBrekpoints.getAttribute("width"));
  is(width, gDebugger.Prefs.stackframesWidth,
    "The stackframes and breakpoints pane has an incorrect width.");
  is(stackframesAndBrekpoints.style.marginLeft, "0px",
    "The stackframes and breakpoints pane has an incorrect left margin.");
  ok(!stackframesAndBrekpoints.hasAttribute("animated"),
    "The stackframes and breakpoints pane has an incorrect animated attribute.");
  ok(!togglePanesButton.getAttribute("stackframesAndBreakpointsHidden"),
    "The stackframes and breakpoints pane should at this point be visible.");

  is(gDebugger.Prefs.stackframesPaneVisible, true,
    "The stackframes and breakpoints pane should at this point be visible.");

  gView.toggleStackframesAndBreakpointsPane({ visible: false, animated: true });

  is(gDebugger.Prefs.stackframesPaneVisible, false,
    "The stackframes and breakpoints pane should be hidden after collapsing.");

  let margin = -(width + 1) + "px";
  is(width, gDebugger.Prefs.stackframesWidth,
    "The stackframes and breakpoints pane has an incorrect width after collapsing.");
  is(stackframesAndBrekpoints.style.marginLeft, margin,
    "The stackframes and breakpoints pane has an incorrect left margin after collapsing.");
  ok(stackframesAndBrekpoints.hasAttribute("animated"),
    "The stackframes and breakpoints pane has an incorrect attribute after an animated collapsing.");
  ok(togglePanesButton.hasAttribute("stackframesAndBreakpointsHidden"),
    "The stackframes and breakpoints pane should not be visible after collapsing.");

  is(gDebugger.Prefs.stackframesPaneVisible, false,
    "The stackframes and breakpoints pane should be hidden before uncollapsing.");

  gView.toggleStackframesAndBreakpointsPane({ visible: true, animated: false });

  is(gDebugger.Prefs.stackframesPaneVisible, true,
    "The stackframes and breakpoints pane should be visible after uncollapsing.");

  is(width, gDebugger.Prefs.stackframesWidth,
    "The stackframes and breakpoints pane has an incorrect width after uncollapsing.");
  is(stackframesAndBrekpoints.style.marginLeft, "0px",
    "The stackframes and breakpoints pane has an incorrect left margin after uncollapsing.");
  ok(!stackframesAndBrekpoints.hasAttribute("animated"),
    "The stackframes and breakpoints pane has an incorrect attribute after an unanimated uncollapsing.");
  ok(!togglePanesButton.getAttribute("stackframesAndBreakpointsHidden"),
    "The stackframes and breakpoints pane should be visible again after uncollapsing.");
}

function testPaneCollapse2() {
  let variables =
    gDebugger.document.getElementById("variables");
  let togglePanesButton =
    gDebugger.document.getElementById("toggle-panes");

  let width = parseInt(variables.getAttribute("width"));
  is(width, gDebugger.Prefs.variablesWidth,
    "The variables pane has an incorrect width.");
  is(variables.style.marginRight, "0px",
    "The variables pane has an incorrect right margin.");
  ok(!variables.hasAttribute("animated"),
    "The variables pane has an incorrect animated attribute.");
  ok(!togglePanesButton.getAttribute("variablesHidden"),
    "The variables pane should at this point be visible.");

  is(gDebugger.Prefs.variablesPaneVisible, true,
    "The variables pane should at this point be visible.");

  gView.toggleVariablesPane({ visible: false, animated: true });

  is(gDebugger.Prefs.variablesPaneVisible, false,
    "The variables pane should be hidden after collapsing.");

  let margin = -(width + 1) + "px";
  is(width, gDebugger.Prefs.variablesWidth,
    "The variables pane has an incorrect width after collapsing.");
  is(variables.style.marginRight, margin,
    "The variables pane has an incorrect right margin after collapsing.");
  ok(variables.hasAttribute("animated"),
    "The variables pane has an incorrect attribute after an animated collapsing.");
  ok(togglePanesButton.hasAttribute("variablesHidden"),
    "The variables pane should not be visible after collapsing.");

  is(gDebugger.Prefs.variablesPaneVisible, false,
    "The variables pane should be hidden before uncollapsing.");

  gView.toggleVariablesPane({ visible: true, animated: false });

  is(gDebugger.Prefs.variablesPaneVisible, true,
    "The variables pane should be visible after uncollapsing.");

  is(width, gDebugger.Prefs.variablesWidth,
    "The variables pane has an incorrect width after uncollapsing.");
  is(variables.style.marginRight, "0px",
    "The variables pane has an incorrect right margin after uncollapsing.");
  ok(!variables.hasAttribute("animated"),
    "The variables pane has an incorrect attribute after an unanimated uncollapsing.");
  ok(!togglePanesButton.getAttribute("variablesHidden"),
    "The variables pane should be visible again after uncollapsing.");
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
  gView = null;
});
