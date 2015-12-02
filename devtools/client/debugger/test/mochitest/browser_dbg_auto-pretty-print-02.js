/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that auto pretty printing doesn't accidentally toggle
 * pretty printing off when we switch to a minified source
 * that is already pretty printed.
 */

const TAB_URL = EXAMPLE_URL + "doc_auto-pretty-print-02.html";

var gTab, gDebuggee, gPanel, gDebugger;
var gEditor, gSources, gPrefs, gOptions, gView;

var gFirstSourceLabel = "code_ugly-6.js";
var gSecondSourceLabel = "code_ugly-7.js";

var gOriginalPref = Services.prefs.getBoolPref("devtools.debugger.auto-pretty-print");
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
      .then(testPrettyPrintButtonOn)
      .then(() => {
        // Switch to the second source.
        let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN, 2);
        gSources.selectedIndex = 1;
        return finished;
      })
      .then(testSecondSourceLabel)
      .then(() => {
        // Switch back to first source.
        let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
        gSources.selectedIndex = 0;
        return finished;
      })
      .then(testFirstSourceLabel)
      .then(testPrettyPrintButtonOn)
      // Disable auto pretty printing so it does not affect the following tests.
      .then(disableAutoPrettyPrint)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + DevToolsUtils.safeErrorString(aError));
      })
  });
}

function testSourceIsUgly() {
  ok(!gEditor.getText().includes("\n  "),
    "The source shouldn't be pretty printed yet.");
}

function testFirstSourceLabel(){
  let source = gSources.selectedItem.attachment.source;
  ok(source.url === EXAMPLE_URL + gFirstSourceLabel,
    "First source url is correct.");
}

function testSecondSourceLabel(){
  let source = gSources.selectedItem.attachment.source;
  ok(source.url === EXAMPLE_URL + gSecondSourceLabel,
    "Second source url is correct.");
}

function testAutoPrettyPrintOn(){
  is(gPrefs.autoPrettyPrint, true,
    "The auto-pretty-print pref should be on.");
  is(gOptions._autoPrettyPrint.getAttribute("checked"), "true",
    "The Auto pretty print menu item should be checked.");
}

function testPrettyPrintButtonOn(){
  is(gDebugger.document.getElementById("pretty-print").checked, true,
    "The button should be checked when the source is selected.");
}

function disableAutoPrettyPrint(){
  gOptions._autoPrettyPrint.setAttribute("checked", "false");
  gOptions._toggleAutoPrettyPrint();
  gOptions._onPopupHidden();
  info("Disabled auto pretty printing.");
}

function testSourceIsPretty() {
  ok(gEditor.getText().includes("\n  "),
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
