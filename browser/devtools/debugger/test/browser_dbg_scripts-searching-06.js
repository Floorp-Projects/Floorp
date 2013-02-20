/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

/**
 * Tests if the global search results trigger MatchFound and NoMatchFound events
 * properly, and triggers the expected UI behavior.
 */

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gEditor = null;
var gSources = null;
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

    gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
      let url = aEvent.detail.url;
      if (url.indexOf("-02.js") != -1) {
        scriptShown = true;
        gDebugger.removeEventListener(aEvent.type, _onEvent);
        runTest();
      }
    });

    gDebugger.DebuggerController.activeThread.addOneTimeListener("framesadded", function() {
      framesAdded = true;
      runTest();
    });

    gDebuggee.firstCall();
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
    gSources = gDebugger.DebuggerView.Sources;
    gSearchView = gDebugger.DebuggerView.GlobalSearch;
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    doSearch();
  });
}

function doSearch() {
  gDebugger.addEventListener("Debugger:GlobalSearch:MatchFound", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gSources.selectedValue;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 5 &&
           gEditor.getCaretPosition().col == 0,
          "The editor shouldn't have jumped to a matching line yet.");
        is(gSources.visibleItems.length, 2,
          "Not all the scripts are shown after the global search.");

        testSearchMatchNotFound();
      });
    } else {
      ok(false, "The current script shouldn't have changed after a global search.");
    }
  });
  executeSoon(function() {
    write("!eval");
  });
}

function testSearchMatchNotFound() {
  gDebugger.addEventListener("Debugger:GlobalSearch:MatchNotFound", function _onEvent(aEvent) {
    gDebugger.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gSources.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gSources.selectedValue;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 5 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't remain at the correct line.");
        is(gSources.visibleItems.length, 2,
          "Not all the correct scripts are shown after the search.");

        closeDebuggerAndFinish();
      });
    } else {
      ok(false, "How did you get here? Go away!");
    }
  });
  append("/punch him/");
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
    EventUtils.sendChar(text[i], gDebugger);
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
  gSources = null;
  gSearchView = null;
  gSearchBox = null;
});
