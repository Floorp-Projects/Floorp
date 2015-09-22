/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests basic functionality of global search (lowercase + upper case, expected
 * UI behavior, number of results found etc.)
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
      .then(firstSearch)
      .then(secondSearch)
      .then(clearSearch)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
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

      let sourceResults = gDebugger.document.querySelectorAll(".dbg-source-results");
      is(sourceResults.length, 2,
        "There should be matches found in two sources.");

      let item0 = gDebugger.SourceResults.getItemForElement(sourceResults[0]);
      let item1 = gDebugger.SourceResults.getItemForElement(sourceResults[1]);
      is(item0.instance.expanded, true,
        "The first source results should automatically be expanded.")
      is(item1.instance.expanded, true,
        "The second source results should automatically be expanded.")

      let searchResult0 = sourceResults[0].querySelectorAll(".dbg-search-result");
      let searchResult1 = sourceResults[1].querySelectorAll(".dbg-search-result");
      is(searchResult0.length, 1,
        "There should be one line result for the first url.");
      is(searchResult1.length, 2,
        "There should be two line results for the second url.");

      let firstLine0 = searchResult0[0];
      is(firstLine0.querySelector(".dbg-results-line-number").getAttribute("value"), "1",
        "The first result for the first source doesn't have the correct line attached.");

      is(firstLine0.querySelectorAll(".dbg-results-line-contents").length, 1,
        "The first result for the first source doesn't have the correct number of nodes for a line.");
      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string").length, 3,
        "The first result for the first source doesn't have the correct number of strings in a line.");

      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string[match=true]").length, 1,
        "The first result for the first source doesn't have the correct number of matches in a line.");
      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string[match=true]")[0].getAttribute("value"), "de",
        "The first result for the first source doesn't have the correct match in a line.");

      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string[match=false]").length, 2,
        "The first result for the first source doesn't have the correct number of non-matches in a line.");
      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string[match=false]")[0].getAttribute("value"), "/* Any copyright is ",
        "The first result for the first source doesn't have the correct non-matches in a line.");
      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string[match=false]")[1].getAttribute("value"), "dicated to the Public Domain.",
        "The first result for the first source doesn't have the correct non-matches in a line.");

      let firstLine1 = searchResult1[0];
      is(firstLine1.querySelector(".dbg-results-line-number").getAttribute("value"), "1",
        "The first result for the second source doesn't have the correct line attached.");

      is(firstLine1.querySelectorAll(".dbg-results-line-contents").length, 1,
        "The first result for the second source doesn't have the correct number of nodes for a line.");
      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string").length, 3,
        "The first result for the second source doesn't have the correct number of strings in a line.");

      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string[match=true]").length, 1,
        "The first result for the second source doesn't have the correct number of matches in a line.");
      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string[match=true]")[0].getAttribute("value"), "de",
        "The first result for the second source doesn't have the correct match in a line.");

      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string[match=false]").length, 2,
        "The first result for the second source doesn't have the correct number of non-matches in a line.");
      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string[match=false]")[0].getAttribute("value"), "/* Any copyright is ",
        "The first result for the second source doesn't have the correct non-matches in a line.");
      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string[match=false]")[1].getAttribute("value"), "dicated to the Public Domain.",
        "The first result for the second source doesn't have the correct non-matches in a line.");

      let secondLine1 = searchResult1[1];
      is(secondLine1.querySelector(".dbg-results-line-number").getAttribute("value"), "6",
        "The second result for the second source doesn't have the correct line attached.");

      is(secondLine1.querySelectorAll(".dbg-results-line-contents").length, 1,
        "The second result for the second source doesn't have the correct number of nodes for a line.");
      is(secondLine1.querySelectorAll(".dbg-results-line-contents-string").length, 3,
        "The second result for the second source doesn't have the correct number of strings in a line.");

      is(secondLine1.querySelectorAll(".dbg-results-line-contents-string[match=true]").length, 1,
        "The second result for the second source doesn't have the correct number of matches in a line.");
      is(secondLine1.querySelectorAll(".dbg-results-line-contents-string[match=true]")[0].getAttribute("value"), "de",
        "The second result for the second source doesn't have the correct match in a line.");

      is(secondLine1.querySelectorAll(".dbg-results-line-contents-string[match=false]").length, 2,
        "The second result for the second source doesn't have the correct number of non-matches in a line.");
      is(secondLine1.querySelectorAll(".dbg-results-line-contents-string[match=false]")[0].getAttribute("value"), '  ',
        "The second result for the second source doesn't have the correct non-matches in a line.");
      is(secondLine1.querySelectorAll(".dbg-results-line-contents-string[match=false]")[1].getAttribute("value"), 'bugger;',
        "The second result for the second source doesn't have the correct non-matches in a line.");

      deferred.resolve();
    });
  });

  setText(gSearchBox, "!de");

  return deferred.promise;
}

function secondSearch() {
  let deferred = promise.defer();

  is(gSearchView.itemCount, 2,
    "The global search pane should have some child nodes from the previous search.");
  is(gSearchView.widget._parent.hidden, false,
    "The global search pane should be visible from the previous search.");
  is(gSearchView._splitter.hidden, false,
    "The global search pane splitter should be visible from the previous search.");

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

      let sourceResults = gDebugger.document.querySelectorAll(".dbg-source-results");
      is(sourceResults.length, 2,
        "There should be matches found in two sources.");

      let item0 = gDebugger.SourceResults.getItemForElement(sourceResults[0]);
      let item1 = gDebugger.SourceResults.getItemForElement(sourceResults[1]);
      is(item0.instance.expanded, true,
        "The first source results should automatically be expanded.")
      is(item1.instance.expanded, true,
        "The second source results should automatically be expanded.")

      let searchResult0 = sourceResults[0].querySelectorAll(".dbg-search-result");
      let searchResult1 = sourceResults[1].querySelectorAll(".dbg-search-result");
      is(searchResult0.length, 1,
        "There should be one line result for the first url.");
      is(searchResult1.length, 1,
        "There should be one line result for the second url.");

      let firstLine0 = searchResult0[0];
      is(firstLine0.querySelector(".dbg-results-line-number").getAttribute("value"), "1",
        "The first result for the first source doesn't have the correct line attached.");

      is(firstLine0.querySelectorAll(".dbg-results-line-contents").length, 1,
        "The first result for the first source doesn't have the correct number of nodes for a line.");
      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string").length, 5,
        "The first result for the first source doesn't have the correct number of strings in a line.");

      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string[match=true]").length, 2,
        "The first result for the first source doesn't have the correct number of matches in a line.");
      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string[match=true]")[0].getAttribute("value"), "ed",
        "The first result for the first source doesn't have the correct matches in a line.");
      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string[match=true]")[1].getAttribute("value"), "ed",
        "The first result for the first source doesn't have the correct matches in a line.");

      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string[match=false]").length, 3,
        "The first result for the first source doesn't have the correct number of non-matches in a line.");
      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string[match=false]")[0].getAttribute("value"), "/* Any copyright is d",
        "The first result for the first source doesn't have the correct non-matches in a line.");
      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string[match=false]")[1].getAttribute("value"), "icat",
        "The first result for the first source doesn't have the correct non-matches in a line.");
      is(firstLine0.querySelectorAll(".dbg-results-line-contents-string[match=false]")[2].getAttribute("value"), " to the Public Domain.",
        "The first result for the first source doesn't have the correct non-matches in a line.");

      let firstLine1 = searchResult1[0];
      is(firstLine1.querySelector(".dbg-results-line-number").getAttribute("value"), "1",
        "The first result for the second source doesn't have the correct line attached.");

      is(firstLine1.querySelectorAll(".dbg-results-line-contents").length, 1,
        "The first result for the second source doesn't have the correct number of nodes for a line.");
      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string").length, 5,
        "The first result for the second source doesn't have the correct number of strings in a line.");

      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string[match=true]").length, 2,
        "The first result for the second source doesn't have the correct number of matches in a line.");
      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string[match=true]")[0].getAttribute("value"), "ed",
        "The first result for the second source doesn't have the correct matches in a line.");
      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string[match=true]")[1].getAttribute("value"), "ed",
        "The first result for the second source doesn't have the correct matches in a line.");

      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string[match=false]").length, 3,
        "The first result for the second source doesn't have the correct number of non-matches in a line.");
      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string[match=false]")[0].getAttribute("value"), "/* Any copyright is d",
        "The first result for the second source doesn't have the correct non-matches in a line.");
      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string[match=false]")[1].getAttribute("value"), "icat",
        "The first result for the second source doesn't have the correct non-matches in a line.");
      is(firstLine1.querySelectorAll(".dbg-results-line-contents-string[match=false]")[2].getAttribute("value"), " to the Public Domain.",
        "The first result for the second source doesn't have the correct non-matches in a line.");

      deferred.resolve();
    });
  });

  backspaceText(gSearchBox, 2);
  typeText(gSearchBox, "ED");

  return deferred.promise;
}

function clearSearch() {
  gSearchView.clearView();

  is(gSearchView.itemCount, 0,
    "The global search pane shouldn't have any child nodes after clearing.");
  is(gSearchView.widget._parent.hidden, true,
    "The global search pane shouldn't be visible after clearing.");
  is(gSearchView._splitter.hidden, true,
    "The global search pane splitter shouldn't be visible after clearing.");
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
