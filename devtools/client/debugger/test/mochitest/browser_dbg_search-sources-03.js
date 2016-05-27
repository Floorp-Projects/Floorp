/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that while searching for files, the sources list remains unchanged.
 */

const TAB_URL = EXAMPLE_URL + "doc_editor-mode.html";

var gTab, gPanel, gDebugger;
var gSources, gSearchBox;

function test() {
  let options = {
    source: "-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    superGenericSearch()
      .then(verifySourcesPane)
      .then(kindaInterpretableSearch)
      .then(verifySourcesPane)
      .then(incrediblySpecificSearch)
      .then(verifySourcesPane)
      .then(returnAndHide)
      .then(verifySourcesPane)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function waitForMatchFoundAndResultsShown() {
  return promise.all([
    once(gDebugger, "popupshown"),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FILE_SEARCH_MATCH_FOUND)
  ]).then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1)
  ]));
}

function waitForResultsHidden() {
  return once(gDebugger, "popuphidden").then(() => promise.all([
    ensureSourceIs(gPanel, "-01.js"),
    ensureCaretAt(gPanel, 1)
  ]));
}

function superGenericSearch() {
  let finished = waitForMatchFoundAndResultsShown();
  setText(gSearchBox, ".");
  return finished;
}

function kindaInterpretableSearch() {
  let finished = waitForMatchFoundAndResultsShown();
  typeText(gSearchBox, "-0");
  return finished;
}

function incrediblySpecificSearch() {
  let finished = waitForMatchFoundAndResultsShown();
  typeText(gSearchBox, "1.js");
  return finished;
}

function returnAndHide() {
  let finished = waitForResultsHidden();
  EventUtils.sendKey("RETURN", gDebugger);
  return finished;
}

function verifySourcesPane() {
  is(gSources.itemCount, 3,
    "There should be 3 items present in the sources container.");
  is(gSources.visibleItems.length, 3,
    "There should be no hidden items in the sources container.");

  ok(gSources.getItemForAttachment(e => e.label == "code_script-switching-01.js"),
    "The first source's label should be correct.");
  ok(gSources.getItemForAttachment(e => e.label == "code_test-editor-mode"),
    "The second source's label should be correct.");
  ok(gSources.getItemForAttachment(e => e.label == "doc_editor-mode.html"),
    "The third source's label should be correct.");
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gSources = null;
  gSearchBox = null;
});
