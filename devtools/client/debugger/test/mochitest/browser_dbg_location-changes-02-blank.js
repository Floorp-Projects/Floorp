/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that changing the tab location URL to a page with no sources works.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const gFrames = gDebugger.DebuggerView.StackFrames;
    const constants = gDebugger.require('./content/constants');

    Task.spawn(function*() {
      yield waitForSourceAndCaretAndScopes(gPanel, ".html", 14);

      navigateActiveTabTo(gPanel, "about:blank");
      yield waitForDispatch(gPanel, constants.LOAD_SOURCES);

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

    callInTab(gTab, "simpleCall");
  });
}
