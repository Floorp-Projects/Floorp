/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure the right text shows when the page has no sources.
 */

const TAB_URL = EXAMPLE_URL + "doc_no-page-sources.html";

var gTab, gDebuggee, gPanel, gDebugger;
var gEditor, gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    const constants = gDebugger.require('./content/constants');

    reloadActiveTab(gPanel);
    waitForNavigation(gPanel)
      .then(testSourcesEmptyText)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function testSourcesEmptyText() {
    is(gSources.itemCount, 0,
      "Found no entries in the sources widget.");

    is(gEditor.getText().length, 0,
      "The source editor should not have any text displayed.");

    is(gDebugger.document.querySelector("#sources .side-menu-widget-empty-text").getAttribute("value"),
       gDebugger.L10N.getStr("noSourcesText"),
      "The sources widget should now display 'This page has no sources'.");
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
});
