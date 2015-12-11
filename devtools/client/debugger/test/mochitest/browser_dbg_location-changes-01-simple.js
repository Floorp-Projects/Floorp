/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that changing the tab location URL works.
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

      is(gDebugger.gThreadClient.state, "paused",
         "Should only be getting stack frames while paused.");

      is(gFrames.itemCount, 1,
         "Should have only one frame.");

      is(gSources.itemCount, 1,
         "Found the expected number of entries in the sources widget.");

      isnot(gSources.selectedValue, null,
            "There should be a selected source value.");
      isnot(gEditor.getText().length, 0,
            "The source editor should have some text displayed.");
      isnot(gEditor.getText(), gDebugger.L10N.getStr("loadingText"),
            "The source editor text should not be 'Loading...'");

      is(gDebugger.document.querySelectorAll("#sources .side-menu-widget-empty-notice-container").length, 0,
         "The sources widget should not display any notice at this point (1).");
      is(gDebugger.document.querySelectorAll("#sources .side-menu-widget-empty-notice").length, 0,
         "The sources widget should not display any notice at this point (2).");
      is(gDebugger.document.querySelector("#sources .side-menu-widget-empty-notice > label"), null,
         "The sources widget should not display a notice at this point (3).");

      yield doResume(gPanel);
      navigateActiveTabTo(gPanel, "about:blank");
      yield waitForDispatch(gPanel, constants.UNLOAD);
      closeDebuggerAndFinish(gPanel);
    });

    callInTab(gTab, "simpleCall");
  });
}
