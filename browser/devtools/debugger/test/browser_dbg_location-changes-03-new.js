/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that changing the tab location URL to a page with other sources works.
 */

const TAB_URL_1 = EXAMPLE_URL + "doc_recursion-stack.html";
const TAB_URL_2 = EXAMPLE_URL + "doc_iframes.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources, gFrames;

function test() {
  initDebugger(TAB_URL_1).then(([aTab, aDebuggee, aPanel]) => {
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
  navigateActiveTabTo(gPanel, TAB_URL_2, gDebugger.EVENTS.SOURCES_ADDED).then(() => {
    isnot(gDebugger.gThreadClient.state, "paused",
      "Should not be paused after a tab navigation.");

    is(gFrames.itemCount, 0,
      "Should have no frames.");

    is(gSources.itemCount, 1,
      "Found the expected number of entries in the sources widget.");

    is(getSelectedSourceURL(gSources), EXAMPLE_URL + "doc_inline-debugger-statement.html",
      "There should be a selected source value.");
    isnot(gEditor.getText().length, 0,
      "The source editor should have some text displayed.");
    is(gEditor.getText(), gDebugger.L10N.getStr("loadingText"),
      "The source editor text should not be 'Loading...'");

    is(gDebugger.document.querySelectorAll("#sources .side-menu-widget-empty-text").length, 0,
      "The sources widget should not display any notice at this point.");

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
