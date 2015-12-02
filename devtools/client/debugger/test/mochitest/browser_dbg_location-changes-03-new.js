/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that changing the tab location URL to a page with other sources works.
 */

const TAB_URL_1 = EXAMPLE_URL + "doc_recursion-stack.html";
const TAB_URL_2 = EXAMPLE_URL + "doc_iframes.html";

function test() {
  initDebugger(TAB_URL_1).then(([aTab, aDebuggee, aPanel]) => {
    const gTab = aTab;
    const gDebuggee = aDebuggee;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const gFrames = gDebugger.DebuggerView.StackFrames;
    const constants = gDebugger.require('./content/constants');

    Task.spawn(function*() {
      yield waitForSourceAndCaretAndScopes(gPanel, ".html", 14);

      const startedLoading = waitForNextDispatch(gDebugger.DebuggerController,
                                                 constants.LOAD_SOURCE_TEXT);
      navigateActiveTabTo(gPanel, TAB_URL_2);
      yield startedLoading;

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
         "The source editor text should be 'Loading...'");

      is(gDebugger.document.querySelectorAll("#sources .side-menu-widget-empty-text").length, 0,
         "The sources widget should not display any notice at this point.");

      yield waitForDispatch(gPanel, constants.LOAD_SOURCE_TEXT);
      closeDebuggerAndFinish(gPanel);
    });

    gDebuggee.simpleCall();
  });
}
