/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

/**
 * Tests basic functionality of scripts filtering (file search).
 */

var gPane = null;
var gTab = null;
var gDebuggee = null;
var gDebugger = null;
var gEditor = null;
var gSources = null;
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
    gSearchBox = gDebugger.DebuggerView.Filtering._searchbox;

    firstSearch();
  });
}

function firstSearch() {
  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + aEvent.detail.url + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = aEvent.detail.url;
    if (url.indexOf("-01.js") != -1) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 4 &&
           gEditor.getCaretPosition().col == 0,
          "The editor didn't jump to the correct line. (1)");
        is(gSources.visibleItems.length, 1,
          "Not all the correct scripts are shown after the search. (1)");

        secondSearch();
      });
    } else {
      ok(false, "Get off my lawn.");
    }
  });
  write(".*-01\.js:5");
}

function secondSearch() {
  let token = "deb";

  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + aEvent.detail.url + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = aEvent.detail.url;
    if (url.indexOf("-02.js") != -1) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        append("#" + token);

        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 5 &&
           gEditor.getCaretPosition().col == 8 + token.length,
          "The editor didn't jump to the correct line. (2)");
        is(gSources.visibleItems.length, 1,
          "Not all the correct scripts are shown after the search. (2)");

        waitForFirstScript();
      });
    } else {
      ok(false, "Get off my lawn.");
    }
  });
  gSources.selectedIndex = 1;
}

function waitForFirstScript() {
  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + aEvent.detail.url + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = aEvent.detail.url;
    if (url.indexOf("-01.js") != -1) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        thirdSearch();
      });
    }
  });
  gSources.selectedIndex = 0;
}

function thirdSearch() {
  let token = "deb";

  gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
    info("Current script url:\n" + aEvent.detail.url + "\n");
    info("Debugger editor text:\n" + gEditor.getText() + "\n");

    let url = aEvent.detail.url;
    if (url.indexOf("-02.js") != -1) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);

      executeSoon(function() {
        info("Editor caret position: " + gEditor.getCaretPosition().toSource() + "\n");
        ok(gEditor.getCaretPosition().line == 5 &&
           gEditor.getCaretPosition().col == 8 + token.length,
          "The editor didn't jump to the correct line. (3)");
        is(gSources.visibleItems.length, 1,
          "Not all the correct scripts are shown after the search. (3)");

        fourthSearch(0, "ugger;", token);
      });
    } else {
      ok(false, "Get off my lawn.");
    }
  });
  write(".*-02\.js#" + token);
}

function fourthSearch(i, string, token) {
  info("Searchbox value: " + gSearchBox.value);

  ok(gEditor.getCaretPosition().line == 5 &&
     gEditor.getCaretPosition().col == 8 + token.length + i,
    "The editor didn't remain at the correct token. (4)");

  if (string[i]) {
    EventUtils.sendChar(string[i], gDebugger);
    fourthSearch(i + 1, string, token);
    return;
  }

  clear();
  ok(gEditor.getCaretPosition().line == 5 &&
     gEditor.getCaretPosition().col == 8 + token.length + i,
    "The editor didn't remain at the correct token. (5)");

  executeSoon(function() {
    let noMatchingSources = gDebugger.L10N.getStr("noMatchingSourcesText");

    is(gSources.visibleItems.length, 2,
      "Not all the scripts are shown after the searchbox was emptied.");
    is(gSources.selectedIndex, 1,
      "The scripts container should have retained its selected index after the searchbox was emptied.");

    write("BOGUS");
    ok(gEditor.getCaretPosition().line == 5 &&
       gEditor.getCaretPosition().col == 8 + token.length + i,
      "The editor didn't remain at the correct token. (6)");

    is(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should display a notice that no scripts match the searched token.");
    is(gSources.visibleItems.length, 0,
      "No scripts should be displayed in the scripts container after a bogus search.");
    is(gSources.selectedIndex, 1,
      "The scripts container should retain its selected index after a bogus search.");

    clear();
    ok(gEditor.getCaretPosition().line == 5 &&
       gEditor.getCaretPosition().col == 8 + token.length + i,
      "The editor didn't remain at the correct token. (7)");

    isnot(gSources.widget.getAttribute("label"), noMatchingSources,
      "The scripts container should not display a notice after the searchbox was emptied.");
    is(gSources.visibleItems.length, 2,
      "Not all the scripts are shown after the searchbox was emptied.");
    is(gSources.selectedIndex, 1,
      "The scripts container should have retained its selected index after the searchbox was emptied of a bogus search.");

    noMatchingSourcesSingleCharCheck(token, i);
  });
}

function noMatchingSourcesSingleCharCheck(token, i) {
  let noMatchingSources = gDebugger.L10N.getStr("noMatchingSourcesText");

  write("x");
  ok(gEditor.getCaretPosition().line == 5 &&
     gEditor.getCaretPosition().col == 8 + token.length + i,
    "The editor didn't remain at the correct token. (8)");

  is(gSources.widget.getAttribute("label"), noMatchingSources,
    "The scripts container should display a notice after no matches are found.");
  is(gSources.visibleItems.length, 0,
    "No scripts should be shown after no matches are found.");
  is(gSources.selectedIndex, 1,
    "The scripts container should have retained its selected index after no matches are found.");

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
  gSearchBox = null;
});
