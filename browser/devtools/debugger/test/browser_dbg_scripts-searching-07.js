/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

/**
 * Tests if the global search results are expanded on scroll or click, and
 * clicking matches makes the source editor shows the correct script and
 * makes a selection based on the match.
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
    gDebugger = gPane.contentWindow;

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      framesAdded = true;
      runTest();
    });

    gDebuggee.firstCall();
  });

  window.addEventListener("Debugger:ScriptShown", function _onEvent(aEvent) {
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
    gScripts = gDebugger.DebuggerView.Scripts;
    gSearchView = gDebugger.DebuggerView.GlobalSearch;
    gSearchBox = gScripts._searchbox;

    doSearch();
  });
}

function doSearch() {
  window.addEventListener("Debugger:GlobalSearch:MatchFound", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gScripts.selected + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gScripts.selected;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        continueTest();
      });
    } else {
      ok(false, "The current script shouldn't have changed after a global search.");
    }
  });
  executeSoon(function() {
    write("!a");
  });
}

function continueTest() {
  let scriptResults = gDebugger.document.querySelectorAll(".dbg-script-results");
  is(scriptResults.length, 2,
    "There should be matches found in two scripts.");

  testScrollToExpand(scriptResults);
  testExpandCollapse(scriptResults);
  testAdditionalScrollToExpand(scriptResults);
  testClickLineToJump(scriptResults, [testClickMatchToJump, closeDebuggerAndFinish]);
}

function testScrollToExpand(scriptResults) {
  is(scriptResults[0].expanded, true,
    "The first script results should automatically be expanded.");
  is(scriptResults[1].expanded, false,
    "The first script results should not be automatically expanded.");

  gSearchView._forceExpandResults = true;
  gSearchView._onResultsScroll();

  is(scriptResults[0].expanded, true,
    "The first script results should be expanded after scrolling.");
  is(scriptResults[1].expanded, true,
    "The second script results should be expanded after scrolling.");
}

function testExpandCollapse(scriptResults) {
  let firstHeader = scriptResults[0].querySelector(".dbg-results-header");
  let secondHeader = scriptResults[1].querySelector(".dbg-results-header");

  EventUtils.sendMouseEvent({ type: "click" }, firstHeader);
  EventUtils.sendMouseEvent({ type: "click" }, secondHeader);

  is(scriptResults[0].expanded, false,
    "The first script results should be collapsed on click.")
  is(scriptResults[1].expanded, false,
    "The second script results should be collapsed on click.")

  EventUtils.sendMouseEvent({ type: "click" }, firstHeader);
  EventUtils.sendMouseEvent({ type: "click" }, secondHeader);

  is(scriptResults[0].expanded, true,
    "The first script results should be expanded on an additional click.");
  is(scriptResults[1].expanded, true,
    "The second script results should be expanded on an additional click.");
}

function testAdditionalScrollToExpand(scriptResults) {
  let firstHeader = scriptResults[0].querySelector(".dbg-results-header");
  let secondHeader = scriptResults[1].querySelector(".dbg-results-header");

  EventUtils.sendMouseEvent({ type: "click" }, firstHeader);
  EventUtils.sendMouseEvent({ type: "click" }, secondHeader);

  is(scriptResults[0].expanded, false,
    "The first script results should be recollapsed on click.")
  is(scriptResults[1].expanded, false,
    "The second script results should be recollapsed on click.")

  gSearchView._onResultsScroll();

  is(scriptResults[0].expanded, false,
    "The first script results should not be automatically re-expanded on scroll after a user collapsed them.")
  is(scriptResults[1].expanded, false,
    "The second script results should not be automatically re-expanded on scroll after a user collapsed them.")
}

function testClickLineToJump(scriptResults, callbacks) {
  let targetResults = scriptResults[0];
  let firstHeader = targetResults.querySelector(".dbg-results-header");
  firstHeader.parentNode.expand()

  is(firstHeader.parentNode.expanded, true,
    "The first script results should be expanded after direct function call.");

  window.addEventListener("Debugger:ScriptShown", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + aEvent.detail.url + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = aEvent.detail.url;
    if (url.indexOf("-01.js") != -1) {
      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 0 &&
           gEditor.getCaretPosition().col == 4,
          "The editor didn't jump to the correct line. (1)");
        is(gScripts.visibleItemsCount, 2,
          "Not all the correct scripts are shown after the search. (1)");

        callbacks[0](scriptResults, callbacks.slice(1));
      });
    } else {
      ok(false, "We jumped in a bowl of hot lava (aka WRONG MATCH). That was bad for us.");
    }
  });

  let firstLine = targetResults.querySelector(".line-contents");
  EventUtils.sendMouseEvent({ type: "click" }, firstLine);
}

function testClickMatchToJump(scriptResults, callbacks) {
  let targetResults = scriptResults[1];
  let secondHeader = targetResults.querySelector(".dbg-results-header");
  secondHeader.parentNode.expand()

  is(secondHeader.parentNode.expanded, true,
    "The second script results should be expanded after direct function call.");

  window.addEventListener("Debugger:ScriptShown", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + aEvent.detail.url + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = aEvent.detail.url;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 5 &&
           gEditor.getCaretPosition().col == 5,
          "The editor didn't jump to the correct line. (1)");
        is(gScripts.visibleItemsCount, 2,
          "Not all the correct scripts are shown after the search. (1)");

        callbacks[0]();
      });
    } else {
      ok(false, "We jumped in a bowl of hot lava (aka WRONG MATCH). That was bad for us.");
    }
  });

  let matches = targetResults.querySelectorAll(".string[match=true]");
  let lastMatch = matches[matches.length - 1];
  EventUtils.sendMouseEvent({ type: "click" }, lastMatch);
}

function clear() {
  gSearchBox.focus();
  gSearchBox.value = "";
}

function write(text) {
  clear();
  append(text);
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
  gSearchBox = null;
});
