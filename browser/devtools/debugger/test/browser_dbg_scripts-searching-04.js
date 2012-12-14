/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

/**
 * Tests if the global search results switch back and forth, and wrap around
 * when switching between them.
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

        isnot(gSearchView._container._list.childNodes.length, 0,
          "The global search pane should be visible now.");
        isnot(gSearchView._container._parent.hidden, true,
          "The global search pane should be visible now.");
        isnot(gSearchView._splitter.hidden, true,
          "The global search pane splitter should be visible now.");

        doFirstJump();
      });
    } else {
      ok(false, "The current script shouldn't have changed after a global search.");
    }
  });
  executeSoon(function() {
    write("!eval");
  });
}

function doFirstJump() {
  window.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + aEvent.detail.url + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = aEvent.detail.url;
    if (url.indexOf("-01.js") != -1) {
      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 4 &&
           gEditor.getCaretPosition().col == 6,
          "The editor didn't jump to the correct line. (1)");
        is(gScripts.visibleItems.length, 2,
          "Not all the correct scripts are shown after the search. (1)");

        doSecondJump();
      });
    } else {
      ok(false, "We jumped in a bowl of hot lava (aka WRONG MATCH). That was bad for us.");
    }
  });
  executeSoon(function() {
    EventUtils.sendKey("DOWN");
  });
}

function doSecondJump() {
  window.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + aEvent.detail.url + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = aEvent.detail.url;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 5 &&
           gEditor.getCaretPosition().col == 6,
          "The editor didn't jump to the correct line. (2)");
        is(gScripts.visibleItems.length, 2,
          "Not all the correct scripts are shown after the search. (2)");

        doWrapAroundJump();
      });
    } else {
      ok(false, "We jumped in a bowl of hot lava (aka WRONG MATCH). That was bad for us.");
    }
  });
  executeSoon(function() {
    EventUtils.sendKey("RETURN");
  });
}

function doWrapAroundJump() {
  window.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + aEvent.detail.url + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = aEvent.detail.url;
    if (url.indexOf("-01.js") != -1) {
      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 4 &&
           gEditor.getCaretPosition().col == 6,
          "The editor didn't jump to the correct line. (3)");
        is(gScripts.visibleItems.length, 2,
          "Not all the correct scripts are shown after the search. (3)");

        doBackwardsWrapAroundJump();
      });
    } else {
      ok(false, "We jumped in a bowl of hot lava (aka WRONG MATCH). That was bad for us.");
    }
  });
  executeSoon(function() {
    EventUtils.sendKey("ENTER");
  });
}

function doBackwardsWrapAroundJump() {
  window.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + aEvent.detail.url + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = aEvent.detail.url;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 5 &&
           gEditor.getCaretPosition().col == 6,
          "The editor didn't jump to the correct line. (4)");
        is(gScripts.visibleItems.length, 2,
          "Not all the correct scripts are shown after the search. (4)");

        testSearchTokenEmpty();
      });
    } else {
      ok(false, "We jumped in a bowl of hot lava (aka WRONG MATCH). That was bad for us.");
    }
  });
  executeSoon(function() {
    EventUtils.sendKey("UP");
  });
}

function testSearchTokenEmpty() {
  window.addEventListener("Debugger:GlobalSearch:TokenEmpty", function _onEvent(aEvent) {
    window.removeEventListener(aEvent.type, _onEvent);
    info("Current script url:\n" + gScripts.selectedValue + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = gScripts.selectedValue;
    if (url.indexOf("-02.js") != -1) {
      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 5 &&
           gEditor.getCaretPosition().col == 6,
          "The editor didn't remain at the correct line. (5)");
        is(gScripts.visibleItems.length, 2,
          "Not all the correct scripts are shown after the search. (5)");

        is(gSearchView._container._list.childNodes.length, 0,
          "The global search pane shouldn't have any child nodes after clear().");
        is(gSearchView._container._parent.hidden, true,
          "The global search pane shouldn't be visible after clear().");
        is(gSearchView._splitter.hidden, true,
          "The global search pane splitter shouldn't be visible after clear().");

        closeDebuggerAndFinish();
      });
    } else {
      ok(false, "How did you get here? Go away!");
    }
  });
  backspace(4);
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
