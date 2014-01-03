/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that changing the tab location URL to a page with no sources works.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources, gFrames;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gFrames = gDebugger.DebuggerView.StackFrames;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 14).then(testLocationChange);
    gDebuggee.simpleCall();
  });
}

function testLocationChange() {
  navigateActiveTabTo(gPanel, "about:blank", gDebugger.EVENTS.SOURCES_ADDED).then(() => {
    isnot(gDebugger.gThreadClient.state, "paused",
      "Should not be paused after a tab navigation.");

    is(gFrames.itemCount, 0,
      "Should have no frames.");

    is(gSources.itemCount, 0,
      "Found no entries in the sources widget.");

    is(gSources.selectedValue, "",
      "There should be no selected source value.");
    is(gEditor.getText().length, 0,
      "The source editor should not have any text displayed.");

    is(gDebugger.document.querySelectorAll("#sources .side-menu-widget-empty-text").length, 1,
      "The sources widget should now display a notice (1).");
    is(gDebugger.document.querySelectorAll("#sources .side-menu-widget-empty-text")[0].getAttribute("value"),
       gDebugger.L10N.getStr("noSourcesText"),
      "The sources widget should now display a notice (2).");

    closeDebuggerAndFinish(gPanel);
  });
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gFrames = null;
});
