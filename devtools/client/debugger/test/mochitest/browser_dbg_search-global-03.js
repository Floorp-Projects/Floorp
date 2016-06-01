/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the global search results are cleared on location changes, and
 * the expected UI behaviors are triggered.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

var gTab, gPanel, gDebugger;
var gEditor, gSources, gSearchView, gSearchBox;

function test() {
  let options = {
    source: "-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gSearchView = gDebugger.DebuggerView.GlobalSearch;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1)
      .then(firstSearch)
      .then(performTest)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    callInTab(gTab, "firstCall");
  });
}

function firstSearch() {
  let deferred = promise.defer();

  is(gSearchView.itemCount, 0,
    "The global search pane shouldn't have any entries yet.");
  is(gSearchView.widget._parent.hidden, true,
    "The global search pane shouldn't be visible yet.");
  is(gSearchView._splitter.hidden, true,
    "The global search pane splitter shouldn't be visible yet.");

  gDebugger.once(gDebugger.EVENTS.GLOBAL_SEARCH_MATCH_FOUND, () => {
    // Some operations are synchronously dispatched on the main thread,
    // to avoid blocking UI, thus giving the impression of faster searching.
    executeSoon(() => {
      info("Current source url:\n" + getSelectedSourceURL(gSources));
      info("Debugger editor text:\n" + gEditor.getText());

      ok(isCaretPos(gPanel, 6),
        "The editor shouldn't have jumped to a matching line yet.");
      ok(getSelectedSourceURL(gSources).includes("-02.js"),
        "The current source shouldn't have changed after a global search.");
      is(gSources.visibleItems.length, 2,
        "Not all the sources are shown after the global search.");

      deferred.resolve();
    });
  });

  setText(gSearchBox, "!function");

  return deferred.promise;
}

function performTest() {
  let deferred = promise.defer();

  is(gSearchView.itemCount, 2,
    "The global search pane should have some entries from the previous search.");
  is(gSearchView.widget._parent.hidden, false,
    "The global search pane should be visible from the previous search.");
  is(gSearchView._splitter.hidden, false,
    "The global search pane splitter should be visible from the previous search.");

  reloadActiveTab(gPanel, gDebugger.EVENTS.SOURCE_SHOWN).then(() => {
    info("Current source url:\n" + getSelectedSourceURL(gSources));
    info("Debugger editor text:\n" + gEditor.getText());

    is(gSearchView.itemCount, 0,
      "The global search pane shouldn't have any entries after a page navigation.");
    is(gSearchView.widget._parent.hidden, true,
      "The global search pane shouldn't be visible after a page navigation.");
    is(gSearchView._splitter.hidden, true,
      "The global search pane splitter shouldn't be visible after a page navigation.");

    deferred.resolve();
  });

  return deferred.promise;
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gSearchView = null;
  gSearchBox = null;
});
