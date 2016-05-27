/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test auto pretty printing.

const TAB_URL = EXAMPLE_URL + "doc_auto-pretty-print-01.html";

var gTab, gPanel, gDebugger;
var gEditor, gSources, gPrefs, gOptions, gView;

var gFirstSourceLabel = "code_ugly-5.js";
var gSecondSourceLabel = "code_ugly-6.js";

var gOriginalPref = Services.prefs.getBoolPref("devtools.debugger.auto-pretty-print");

function test() {
  let options = {
    source: gFirstSourceLabel,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gPrefs = gDebugger.Prefs;
    gOptions = gDebugger.DebuggerView.Options;
    gView = gDebugger.DebuggerView;

    Task.spawn(function* () {
      testSourceIsUgly();

      enableAutoPrettyPrint();
      testAutoPrettyPrintOn();

      reload(gPanel);
      yield waitForSourceShown(gPanel, gFirstSourceLabel);
      testSourceIsUgly();
      yield waitForSourceShown(gPanel, gFirstSourceLabel);
      testSourceIsPretty();
      disableAutoPrettyPrint();
      testAutoPrettyPrintOff();

      let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
      gSources.selectedIndex = 1;
      yield finished;

      testSecondSourceLabel();
      testSourceIsUgly();

      enableAutoPrettyPrint();
      yield closeDebuggerAndFinish(gPanel);
    });
  });
}

function testSourceIsUgly() {
  ok(!gEditor.getText().includes("\n  "),
    "The source shouldn't be pretty printed yet.");
}

function testSecondSourceLabel() {
  let source = gSources.selectedItem.attachment.source;
  ok(source.url === EXAMPLE_URL + gSecondSourceLabel,
    "Second source url is correct.");
}

function testProgressBarShown() {
  const deck = gDebugger.document.getElementById("editor-deck");
  is(deck.selectedIndex, 2, "The progress bar should be shown");
}

function testAutoPrettyPrintOn() {
  is(gPrefs.autoPrettyPrint, true,
    "The auto-pretty-print pref should be on.");
  is(gOptions._autoPrettyPrint.getAttribute("checked"), "true",
    "The Auto pretty print menu item should be checked.");
}

function disableAutoPrettyPrint() {
  gOptions._autoPrettyPrint.setAttribute("checked", "false");
  gOptions._toggleAutoPrettyPrint();
  gOptions._onPopupHidden();
}

function enableAutoPrettyPrint() {
  gOptions._autoPrettyPrint.setAttribute("checked", "true");
  gOptions._toggleAutoPrettyPrint();
  gOptions._onPopupHidden();
}

function testAutoPrettyPrintOff() {
  is(gPrefs.autoPrettyPrint, false,
    "The auto-pretty-print pref should be off.");
  isnot(gOptions._autoPrettyPrint.getAttribute("checked"), "true",
       "The Auto pretty print menu item should not be checked.");
}

function testSourceIsPretty() {
  ok(gEditor.getText().includes("\n  "),
    "The source should be pretty printed.");
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gOptions = null;
  gPrefs = null;
  gView = null;
  Services.prefs.setBoolPref("devtools.debugger.auto-pretty-print", gOriginalPref);
});
