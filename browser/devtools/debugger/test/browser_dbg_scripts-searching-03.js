/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

/**
 * Tests basic functionality of global search (lowercase + upper case, expected
 * UI behavior, number of results found etc.)
 */

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gEditor = null;
var gScripts = null;
var gSearchView = null;
var gSearchBox = null;

function test()
{
  let scriptShown = false;
  let framesAdded = false;

  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gDebugger.SourceResults.prototype.alwaysExpand = false;

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      framesAdded = true;
      runTest();
    });

    gDebuggee.firstCall();
  });

  window.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    let url = aEvent.detail.url;
    if (url.indexOf("-02.js") != -1) {
      scriptShown = true;
      window.removeEventListener(aEvent.type, _onEvent);
      runTest();
    }
  });

  function runTest()
  {
    if (scriptShown && framesAdded) {
      Services.tm.currentThread.dispatch({ run: testScriptSearching }, 0);
    }
  }
}

function testScriptSearching() {
  gDebugger.DebuggerController.activeThread.resume(function() {
    gEditor = gDebugger.DebuggerView.editor;
    gScripts = gDebugger.DebuggerView.Sources;
    gSearchView = gDebugger.DebuggerView.GlobalSearch;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    firstSearch();
  });
}

function firstSearch() {
  is(gSearchView._container._list.childNodes.length, 0,
    "The global search pane shouldn't have any child nodes yet.");
  is(gSearchView._container._parent.hidden, true,
    "The global search pane shouldn't be visible yet.");
  is(gSearchView._splitter.hidden, true,
    "The global search pane splitter shouldn't be visible yet.");

  window.addEventListener("Debugger:GlobalSearch:MatchFound", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gScripts.selectedValue;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 5 &&
           gEditor.getCaretPosition().col == 0,
          "The editor shouldn't have jumped to a matching line yet.");
        is(gScripts.visibleItems.length, 2,
          "Not all the scripts are shown after the global search.");

        let scriptResults = gDebugger.document.querySelectorAll(".dbg-source-results");
        is(scriptResults.length, 2,
          "There should be matches found in two scripts.");

        let item0 = gDebugger.SourceResults.getItemForElement(scriptResults[0]);
        let item1 = gDebugger.SourceResults.getItemForElement(scriptResults[1]);
        is(item0.instance.expanded, true,
          "The first script results should automatically be expanded.")
        is(item1.instance.expanded, false,
          "The first script results should not be automatically expanded.")

        let searchResult0 = scriptResults[0].querySelectorAll(".dbg-search-result");
        let searchResult1 = scriptResults[1].querySelectorAll(".dbg-search-result");
        is(searchResult0.length, 1,
          "There should be one line result for the first url.");
        is(searchResult1.length, 2,
          "There should be two line results for the second url.");

        let firstLine0 = searchResult0[0];
        is(firstLine0.querySelector(".line-number").getAttribute("value"), "1",
          "The first result for the first script doesn't have the correct line attached.");

        is(firstLine0.querySelectorAll(".line-contents").length, 1,
          "The first result for the first script doesn't have the correct number of nodes for a line.");
        is(firstLine0.querySelectorAll(".string").length, 3,
          "The first result for the first script doesn't have the correct number of strings in a line.");

        is(firstLine0.querySelectorAll(".string[match=true]").length, 1,
          "The first result for the first script doesn't have the correct number of matches in a line.");
        is(firstLine0.querySelectorAll(".string[match=true]")[0].getAttribute("value"), "de",
          "The first result for the first script doesn't have the correct match in a line.");

        is(firstLine0.querySelectorAll(".string[match=false]").length, 2,
          "The first result for the first script doesn't have the correct number of non-matches in a line.");
        is(firstLine0.querySelectorAll(".string[match=false]")[0].getAttribute("value"), "/* Any copyright is ",
          "The first result for the first script doesn't have the correct non-matches in a line.");
        is(firstLine0.querySelectorAll(".string[match=false]")[1].getAttribute("value"), "dicated to the Public Domain.",
          "The first result for the first script doesn't have the correct non-matches in a line.");

        let firstLine1 = searchResult1[0];
        is(firstLine1.querySelector(".line-number").getAttribute("value"), "1",
          "The first result for the second script doesn't have the correct line attached.");

        is(firstLine1.querySelectorAll(".line-contents").length, 1,
          "The first result for the second script doesn't have the correct number of nodes for a line.");
        is(firstLine1.querySelectorAll(".string").length, 3,
          "The first result for the second script doesn't have the correct number of strings in a line.");

        is(firstLine1.querySelectorAll(".string[match=true]").length, 1,
          "The first result for the second script doesn't have the correct number of matches in a line.");
        is(firstLine1.querySelectorAll(".string[match=true]")[0].getAttribute("value"), "de",
          "The first result for the second script doesn't have the correct match in a line.");

        is(firstLine1.querySelectorAll(".string[match=false]").length, 2,
          "The first result for the second script doesn't have the correct number of non-matches in a line.");
        is(firstLine1.querySelectorAll(".string[match=false]")[0].getAttribute("value"), "/* Any copyright is ",
          "The first result for the second script doesn't have the correct non-matches in a line.");
        is(firstLine1.querySelectorAll(".string[match=false]")[1].getAttribute("value"), "dicated to the Public Domain.",
          "The first result for the second script doesn't have the correct non-matches in a line.");

        let secondLine1 = searchResult1[1];
        is(secondLine1.querySelector(".line-number").getAttribute("value"), "6",
          "The second result for the second script doesn't have the correct line attached.");

        is(secondLine1.querySelectorAll(".line-contents").length, 1,
          "The second result for the second script doesn't have the correct number of nodes for a line.");
        is(secondLine1.querySelectorAll(".string").length, 3,
          "The second result for the second script doesn't have the correct number of strings in a line.");

        is(secondLine1.querySelectorAll(".string[match=true]").length, 1,
          "The second result for the second script doesn't have the correct number of matches in a line.");
        is(secondLine1.querySelectorAll(".string[match=true]")[0].getAttribute("value"), "de",
          "The second result for the second script doesn't have the correct match in a line.");

        is(secondLine1.querySelectorAll(".string[match=false]").length, 2,
          "The second result for the second script doesn't have the correct number of non-matches in a line.");
        is(secondLine1.querySelectorAll(".string[match=false]")[0].getAttribute("value"), '  eval("',
          "The second result for the second script doesn't have the correct non-matches in a line.");
        is(secondLine1.querySelectorAll(".string[match=false]")[1].getAttribute("value"), 'bugger;");',
          "The second result for the second script doesn't have the correct non-matches in a line.");


        secondSearch();
      });
    } else {
      ok(false, "The current script shouldn't have changed after a global search.");
    }
  });
  executeSoon(function() {
    write("!de");
  });
}

function secondSearch() {
  isnot(gSearchView._container._list.childNodes.length, 0,
    "The global search pane should have some child nodes from the previous search.");
  is(gSearchView._container._parent.hidden, false,
    "The global search pane should be visible from the previous search.");
  is(gSearchView._splitter.hidden, false,
    "The global search pane splitter should be visible from the previous search.");

  window.addEventListener("Debugger:GlobalSearch:MatchFound", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gScripts.selectedValue;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 5 &&
           gEditor.getCaretPosition().col == 0,
          "The editor shouldn't have jumped to a matching line yet.");
        is(gScripts.visibleItems.length, 2,
          "Not all the scripts are shown after the global search.");

        let scriptResults = gDebugger.document.querySelectorAll(".dbg-source-results");
        is(scriptResults.length, 2,
          "There should be matches found in two scripts.");

        let item0 = gDebugger.SourceResults.getItemForElement(scriptResults[0]);
        let item1 = gDebugger.SourceResults.getItemForElement(scriptResults[1]);
        is(item0.instance.expanded, true,
          "The first script results should automatically be expanded.")
        is(item1.instance.expanded, false,
          "The first script results should not be automatically expanded.")

        let searchResult0 = scriptResults[0].querySelectorAll(".dbg-search-result");
        let searchResult1 = scriptResults[1].querySelectorAll(".dbg-search-result");
        is(searchResult0.length, 1,
          "There should be one line result for the first url.");
        is(searchResult1.length, 1,
          "There should be one line result for the second url.");

        let firstLine0 = searchResult0[0];
        is(firstLine0.querySelector(".line-number").getAttribute("value"), "1",
          "The first result for the first script doesn't have the correct line attached.");

        is(firstLine0.querySelectorAll(".line-contents").length, 1,
          "The first result for the first script doesn't have the correct number of nodes for a line.");
        is(firstLine0.querySelectorAll(".string").length, 5,
          "The first result for the first script doesn't have the correct number of strings in a line.");

        is(firstLine0.querySelectorAll(".string[match=true]").length, 2,
          "The first result for the first script doesn't have the correct number of matches in a line.");
        is(firstLine0.querySelectorAll(".string[match=true]")[0].getAttribute("value"), "ed",
          "The first result for the first script doesn't have the correct matches in a line.");
        is(firstLine0.querySelectorAll(".string[match=true]")[1].getAttribute("value"), "ed",
          "The first result for the first script doesn't have the correct matches in a line.");

        is(firstLine0.querySelectorAll(".string[match=false]").length, 3,
          "The first result for the first script doesn't have the correct number of non-matches in a line.");
        is(firstLine0.querySelectorAll(".string[match=false]")[0].getAttribute("value"), "/* Any copyright is d",
          "The first result for the first script doesn't have the correct non-matches in a line.");
        is(firstLine0.querySelectorAll(".string[match=false]")[1].getAttribute("value"), "icat",
          "The first result for the first script doesn't have the correct non-matches in a line.");
        is(firstLine0.querySelectorAll(".string[match=false]")[2].getAttribute("value"), " to the Public Domain.",
          "The first result for the first script doesn't have the correct non-matches in a line.");

        let firstLine1 = searchResult1[0];
        is(firstLine1.querySelector(".line-number").getAttribute("value"), "1",
          "The first result for the second script doesn't have the correct line attached.");

        is(firstLine1.querySelectorAll(".line-contents").length, 1,
          "The first result for the second script doesn't have the correct number of nodes for a line.");
        is(firstLine1.querySelectorAll(".string").length, 5,
          "The first result for the second script doesn't have the correct number of strings in a line.");

        is(firstLine1.querySelectorAll(".string[match=true]").length, 2,
          "The first result for the second script doesn't have the correct number of matches in a line.");
        is(firstLine1.querySelectorAll(".string[match=true]")[0].getAttribute("value"), "ed",
          "The first result for the second script doesn't have the correct matches in a line.");
        is(firstLine1.querySelectorAll(".string[match=true]")[1].getAttribute("value"), "ed",
          "The first result for the second script doesn't have the correct matches in a line.");

        is(firstLine1.querySelectorAll(".string[match=false]").length, 3,
          "The first result for the second script doesn't have the correct number of non-matches in a line.");
        is(firstLine1.querySelectorAll(".string[match=false]")[0].getAttribute("value"), "/* Any copyright is d",
          "The first result for the second script doesn't have the correct non-matches in a line.");
        is(firstLine1.querySelectorAll(".string[match=false]")[1].getAttribute("value"), "icat",
          "The first result for the second script doesn't have the correct non-matches in a line.");
        is(firstLine1.querySelectorAll(".string[match=false]")[2].getAttribute("value"), " to the Public Domain.",
          "The first result for the second script doesn't have the correct non-matches in a line.");


        testClearView();
      });
    } else {
      ok(false, "The current script shouldn't have changed after a global search.");
    }
  });
  executeSoon(function() {
    backspace(2);
    append("ED");
  });
}

function testClearView() {
  gSearchView.clearView();

  is(gSearchView._container._list.childNodes.length, 0,
    "The global search pane shouldn't have any child nodes after clearView().");
  is(gSearchView._container._parent.hidden, true,
    "The global search pane shouldn't be visible after clearView().");
  is(gSearchView._splitter.hidden, true,
    "The global search pane splitter shouldn't be visible after clearView().");

  closeDebuggerAndFinish();
}

function clear() {
  gSearchBox.focus();
  gSearchBox.value = "";
}

function write(text) {
  clear();
  append(text);
}

function backspace(times) {
  for (let i = 0; i < times; i++) {
    EventUtils.sendKey("BACK_SPACE")
  }
}

function append(text) {
  gSearchBox.focus();

  for (let i = 0; i < text.length; i++) {
    EventUtils.sendChar(text[i]);
  }
  info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
  gEditor = null;
  gScripts = null;
  gSearchView = null;
  gSearchBox = null;
});
