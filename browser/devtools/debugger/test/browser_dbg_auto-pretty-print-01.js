/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

// Test auto pretty printing.

const TAB_URL = EXAMPLE_URL + "doc_auto-pretty-print-01.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources, gPrefs, gOptions, gView;

let gFirstSourceLabel = "code_ugly-5.js";
let gSecondSourceLabel = "code_ugly-6.js";

let gOriginalPref = Services.prefs.getBoolPref("devtools.debugger.auto-pretty-print");
Services.prefs.setBoolPref("devtools.debugger.auto-pretty-print", true);

function test(){
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gPrefs = gDebugger.Prefs;
    gOptions = gDebugger.DebuggerView.Options;
    gView = gDebugger.DebuggerView;

    // Should be on by default.
    testAutoPrettyPrintOn();

    waitForSourceShown(gPanel, gFirstSourceLabel)
      .then(testSourceIsUgly)
      .then(() => waitForSourceShown(gPanel, gFirstSourceLabel))
      .then(testSourceIsPretty)
      .then(disableAutoPrettyPrint)
      .then(testAutoPrettyPrintOff)
      .then(() => {
        let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
          gSources.selectedIndex = 1;
          return finished;
      })
      .then(testSecondSourceLabel)
      .then(testSourceIsUgly)
      // Re-enable auto pretty printing for browser_dbg_auto-pretty-print-02.js
      .then(enableAutoPrettyPrint)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + DevToolsUtils.safeErrorString(aError));
      })
  });
}

function testSourceIsUgly() {
  ok(!gEditor.getText().contains("\n  "),
    "The source shouldn't be pretty printed yet.");
}

function testSecondSourceLabel(){
  ok(gSources.containsValue(EXAMPLE_URL + gSecondSourceLabel),
    "Second source url is correct.");
}

function testProgressBarShown() {
  const deck = gDebugger.document.getElementById("editor-deck");
  is(deck.selectedIndex, 2, "The progress bar should be shown");
}

function testAutoPrettyPrintOn(){
  is(gPrefs.autoPrettyPrint, true,
    "The auto-pretty-print pref should be on.");
  is(gOptions._autoPrettyPrint.getAttribute("checked"), "true",
    "The Auto pretty print menu item should be checked.");
}

function disableAutoPrettyPrint(){
  gOptions._autoPrettyPrint.setAttribute("checked", "false");
  gOptions._toggleAutoPrettyPrint();
  gOptions._onPopupHidden();
}

function enableAutoPrettyPrint(){
  gOptions._autoPrettyPrint.setAttribute("checked", "true");
  gOptions._toggleAutoPrettyPrint();
  gOptions._onPopupHidden();
}

function testAutoPrettyPrintOff(){
  is(gPrefs.autoPrettyPrint, false,
    "The auto-pretty-print pref should be off.");
  isnot(gOptions._autoPrettyPrint.getAttribute("checked"), "true",
       "The Auto pretty print menu item should not be checked.");
}

function testSourceIsPretty() {
  ok(gEditor.getText().contains("\n  "),
    "The source should be pretty printed.")
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gOptions = null;
  gPrefs = null;
  gView = null;
  Services.prefs.setBoolPref("devtools.debugger.auto-pretty-print", gOriginalPref);
});
