/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that changing the tab location URL to a page with no sources works.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const gFrames = gDebugger.DebuggerView.StackFrames;
    const constants = gDebugger.require("./content/constants");

    Task.spawn(function* () {
      let onCaretUpdated = waitForCaretUpdated(gPanel, 14);
      callInTab(gTab, "simpleCall");
      yield onCaretUpdated;

      navigateActiveTabTo(gPanel, "about:blank");
      yield waitForNavigation(gPanel);

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
  });
}
