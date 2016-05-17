/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the global search results are expanded/collapsed on click, and
 * clicking matches makes the source editor shows the correct source and
 * makes a selection based on the match.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

var gTab, gPanel, gDebugger;
var gEditor, gSources, gSearchView, gSearchBox;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gSearchView = gDebugger.DebuggerView.GlobalSearch;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1)
      .then(doSearch)
      .then(testExpandCollapse)
      .then(testClickLineToJump)
      .then(testClickMatchToJump)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    callInTab(gTab, "firstCall");
  });
}

function doSearch() {
  let deferred = promise.defer();

  gDebugger.once(gDebugger.EVENTS.GLOBAL_SEARCH_MATCH_FOUND, () => {
    // Some operations are synchronously dispatched on the main thread,
    // to avoid blocking UI, thus giving the impression of faster searching.
    executeSoon(() => {
      info("Current source url:\n" + getSelectedSourceURL(gSources));
      info("Debugger editor text:\n" + gEditor.getText());

      ok(isCaretPos(gPanel, 6),
        "The editor shouldn't have jumped to a matching line yet. (1)");
      ok(getSelectedSourceURL(gSources).includes("-02.js"),
        "The current source shouldn't have changed after a global search. (2)");
      is(gSources.visibleItems.length, 2,
        "Not all the sources are shown after the global search. (3)");

      deferred.resolve();
    });
  });

  setText(gSearchBox, "!a");

  return deferred.promise;
}

function testExpandCollapse() {
  let sourceResults = gDebugger.document.querySelectorAll(".dbg-source-results");
  let item0 = gDebugger.SourceResults.getItemForElement(sourceResults[0]);
  let item1 = gDebugger.SourceResults.getItemForElement(sourceResults[1]);
  let firstHeader = sourceResults[0].querySelector(".dbg-results-header");
  let secondHeader = sourceResults[1].querySelector(".dbg-results-header");

  EventUtils.sendMouseEvent({ type: "click" }, firstHeader);
  EventUtils.sendMouseEvent({ type: "click" }, secondHeader);

  is(item0.instance.expanded, false,
    "The first source results should be collapsed on click. (2)");
  is(item1.instance.expanded, false,
    "The second source results should be collapsed on click. (2)");

  EventUtils.sendMouseEvent({ type: "click" }, firstHeader);
  EventUtils.sendMouseEvent({ type: "click" }, secondHeader);

  is(item0.instance.expanded, true,
    "The first source results should be expanded on an additional click. (3)");
  is(item1.instance.expanded, true,
    "The second source results should be expanded on an additional click. (3)");
}

function testClickLineToJump() {
  let deferred = promise.defer();

  let sourceResults = gDebugger.document.querySelectorAll(".dbg-source-results");
  let firstHeader = sourceResults[0].querySelector(".dbg-results-header");
  let firstLine = sourceResults[0].querySelector(".dbg-results-line-contents");

  waitForSourceAndCaret(gPanel, "-01.js", 1, 1).then(() => {
    info("Current source url:\n" + getSelectedSourceURL(gSources));
    info("Debugger editor text:\n" + gEditor.getText());

    ok(isCaretPos(gPanel, 1, 5),
      "The editor didn't jump to the correct line (4).");
    is(gEditor.getSelection(), "A",
      "The editor didn't select the correct text (4).");
    ok(getSelectedSourceURL(gSources).includes("-01.js"),
      "The currently shown source is incorrect (4).");
    is(gSources.visibleItems.length, 2,
      "Not all the sources are shown after the global search (4).");

    deferred.resolve();
  });

  EventUtils.sendMouseEvent({ type: "click" }, firstLine);

  return deferred.promise;
}

function testClickMatchToJump() {
  let deferred = promise.defer();

  let sourceResults = gDebugger.document.querySelectorAll(".dbg-source-results");
  let secondHeader = sourceResults[1].querySelector(".dbg-results-header");
  let secondMatches = sourceResults[1].querySelectorAll(".dbg-results-line-contents-string[match=true]");
  let lastMatch = Array.slice(secondMatches).pop();

  waitForSourceAndCaret(gPanel, "-02.js", 13, 3).then(() => {
    info("Current source url:\n" + getSelectedSourceURL(gSources));
    info("Debugger editor text:\n" + gEditor.getText());

    ok(isCaretPos(gPanel, 13, 3),
      "The editor didn't jump to the correct line (5).");
    is(gEditor.getSelection(), "a",
      "The editor didn't select the correct text (5).");
    ok(getSelectedSourceURL(gSources).includes("-02.js"),
      "The currently shown source is incorrect (5).");
    is(gSources.visibleItems.length, 2,
      "Not all the sources are shown after the global search (5).");

    deferred.resolve();
  });

  EventUtils.sendMouseEvent({ type: "click" }, lastMatch);

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
