/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

/**
 * Tests if the global search results are hidden when they're supposed to
 * (after a focus lost, or when ESCAPE is pressed).
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
  is(gSearchView._pane.hidden, true,
    "The global search pane shouldn't be visible yet.");

  window.addEventListener("Debugger:GlobalSearch:MatchFound", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gScripts.selected + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gScripts.selected;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        testFocusLost();
      });
    } else {
      ok(false, "The current script shouldn't have changed after a global search.");
    }
  });
  executeSoon(function() {
    write("!a");
  });
}

function testFocusLost()
{
  is(gSearchView._pane.hidden, false,
    "The global search pane should be visible after a search.");

  window.addEventListener("Debugger:GlobalSearch:ViewCleared", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gScripts.selected + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gScripts.selected;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        reshowSearch();
      });
    } else {
      ok(false, "The current script shouldn't have changed after the global search stopped.");
    }
  });
  executeSoon(function() {
    gDebugger.DebuggerView.editor.focus();
  });
}

function reshowSearch() {
  is(gSearchView._pane.hidden, true,
    "The global search pane shouldn't be visible after the search was stopped.");

  window.addEventListener("Debugger:GlobalSearch:MatchFound", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gScripts.selected + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gScripts.selected;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        testEscape();
      });
    } else {
      ok(false, "The current script shouldn't have changed after a global re-search.");
    }
  });
  executeSoon(function() {
    sendEnter();
  });
}

function testEscape()
{
  is(gSearchView._pane.hidden, false,
    "The global search pane should be visible after a re-search.");

  window.addEventListener("Debugger:GlobalSearch:ViewCleared", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gScripts.selected + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gScripts.selected;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        finalCheck();
      });
    } else {
      ok(false, "The current script shouldn't have changed after the global search escaped.");
    }
  });
  executeSoon(function() {
    sendEscape();
  });
}

function finalCheck()
{
  is(gSearchView._pane.hidden, true,
    "The global search pane shouldn't be visible after the search was escaped.");

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

function sendEnter() {
  gSearchBox.focus();
  EventUtils.sendKey("ENTER");
}

function sendEscape() {
  gSearchBox.focus();
  EventUtils.sendKey("ESCAPE");
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
