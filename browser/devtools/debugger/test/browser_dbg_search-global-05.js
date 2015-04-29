/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the global search results are expanded/collapsed on click, and
 * clicking matches makes the source editor shows the correct source and
 * makes a selection based on the match.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

let gTab, gPanel, gDebugger;
let gEditor, gSources, gSearchView, gSearchBox;

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
        "The editor shouldn't have jumped to a matching line yet.");
      ok(getSelectedSourceURL(gSources).includes("-02.js"),
        "The current source shouldn't have changed after a global search.");
      is(gSources.visibleItems.length, 2,
        "Not all the sources are shown after the global search.");

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
    "The first source results should be collapsed on click.")
  is(item1.instance.expanded, false,
    "The second source results should be collapsed on click.")

  EventUtils.sendMouseEvent({ type: "click" }, firstHeader);
  EventUtils.sendMouseEvent({ type: "click" }, secondHeader);

  is(item0.instance.expanded, true,
    "The first source results should be expanded on an additional click.");
  is(item1.instance.expanded, true,
    "The second source results should be expanded on an additional click.");
}

function testClickLineToJump() {
  let deferred = promise.defer();

  let sourceResults = gDebugger.document.querySelectorAll(".dbg-source-results");
  let firstHeader = sourceResults[0].querySelector(".dbg-results-header");
  let firstLine = sourceResults[0].querySelector(".dbg-results-line-contents");

  waitForSourceAndCaret(gPanel, "-01.js", 1, 1).then(() => {
    info("Current source url:\n" + getSelectedSourceURL(gSources));
    info("Debugger editor text:\n" + gEditor.getText());

    ok(isCaretPos(gPanel, 1, 1),
      "The editor didn't jump to the correct line (1).");
    is(gEditor.getSelection(), "",
      "The editor didn't select the correct text (1).");
    ok(getSelectedSourceURL(gSources).includes("-01.js"),
      "The currently shown source is incorrect (1).");
    is(gSources.visibleItems.length, 2,
      "Not all the sources are shown after the global search (1).");

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

  waitForSourceAndCaret(gPanel, "-02.js", 1, 1).then(() => {
    info("Current source url:\n" + getSelectedSourceURL(gSources));
    info("Debugger editor text:\n" + gEditor.getText());

    ok(isCaretPos(gPanel, 1, 1),
      "The editor didn't jump to the correct line (2).");
    is(gEditor.getSelection(), "",
      "The editor didn't select the correct text (2).");
    ok(getSelectedSourceURL(gSources).includes("-02.js"),
      "The currently shown source is incorrect (2).");
    is(gSources.visibleItems.length, 2,
      "Not all the sources are shown after the global search (2).");

    deferred.resolve();
  });

  EventUtils.sendMouseEvent({ type: "click" }, lastMatch);

  return deferred.promise;
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gSearchView = null;
  gSearchBox = null;
});
