/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

/**
 * Tests if the global search results are cleared on location changes, and
 * the expected UI behaviors are triggered.
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
  requestLongerTimeout(3);

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
  is(gSearchView._container._list.childNodes.length, 0,
    "The global search pane shouldn't have any child nodes yet.");
  is(gSearchView._container._parent.hidden, true,
    "The global search pane shouldn't be visible yet.");
  is(gSearchView._splitter.hidden, true,
    "The global search pane splitter shouldn't be visible yet.");

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

        isnot(gSearchView._container._list.childNodes.length, 0,
          "The global search pane should be visible now.");
        isnot(gSearchView._container._parent.hidden, true,
          "The global search pane should be visible now.");
        isnot(gSearchView._splitter.hidden, true,
          "The global search pane splitter should be visible now.");

        testLocationChange();
      });
    } else {
      ok(false, "The current script shouldn't have changed after a global search.");
    }
  });
  executeSoon(function() {
    write("!eval");
  });
}

function testLocationChange()
{
  gDebugger.DebuggerController._target.once("navigate", function onTabNavigated(aEvent, aPacket) {
    ok(true, "tabNavigated event was fired after location change.");
    info("Still attached to the tab.");

    executeSoon(function() {
      is(gSearchView._container._list.childNodes.length, 0,
        "The global search pane shouldn't have any child nodes after a page navigation.");
      is(gSearchView._container._parent.hidden, true,
        "The global search pane shouldn't be visible after a page navigation.");
      is(gSearchView._splitter.hidden, true,
        "The global search pane splitter shouldn't be visible after a page navigation.");

      is(gDebugger.DebuggerController.SourceScripts.getCache().length, 0,
        "The scripts sources cache for global searching should be cleared after a page navigation.")

      closeDebuggerAndFinish();
    });
  });

  content.location = TAB1_URL;
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
