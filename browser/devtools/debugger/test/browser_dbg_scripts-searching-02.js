/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gEditor = null;
var gScripts = null;
var gSearchBox = null;
var gMenulist = null;

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
    gSearchBox = gScripts._searchbox;
    gMenulist = gScripts._scripts;

    firstSearch();
  });
}

function firstSearch() {
  window.addEventListener("Debugger:ScriptShown", function _onEvent(aEvent) {
    dump("Current script url:\n" + aEvent.detail.url + "\n");
    dump("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = aEvent.detail.url;
    if (url.indexOf("-01.js") != -1) {
      window.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        dump("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 4 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line. (1)");
        is(gScripts.visibleItemsCount, 1,
          "Not all the correct scripts are shown after the search. (1)");

        secondSearch();
      });
    }
  });
  write(".*-01\.js:5");
}

function secondSearch() {
  window.addEventListener("Debugger:ScriptShown", function _onEvent(aEvent) {
    dump("Current script url:\n" + aEvent.detail.url + "\n");
    dump("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = aEvent.detail.url;
    if (url.indexOf("-02.js") != -1) {
      window.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        dump("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 5 &&
           gEditor.getCaretPosition().col == 8,
          "The editor didn't jump to the correct line. (2)");
        is(gScripts.visibleItemsCount, 1,
          "Not all the correct scripts are shown after the search. (2)");

        finalCheck();
      });
    }
  });
  write(".*-02\.js#debugger;");
}

function finalCheck() {
  clear();
  ok(gEditor.getCaretPosition().line == 5 &&
     gEditor.getCaretPosition().col == 8,
    "The editor didn't remain at the correct token. (3)");
  is(gScripts.visibleItemsCount, 2,
    "Not all the scripts are shown after the search. (3)");

  closeDebuggerAndFinish();
}

function clear() {
  gSearchBox.focus();
  gSearchBox.value = "";
}

function write(text) {
  clear();

  for (let i = 0; i < text.length; i++) {
    EventUtils.sendChar(text[i]);
  }
  dump("editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
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
  gMenulist = null;
});
