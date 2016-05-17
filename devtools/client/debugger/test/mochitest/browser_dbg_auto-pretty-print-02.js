/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    const gTab = aTab;
    const gDebuggee = aDebuggee;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const gPrefs = gDebugger.Prefs;
    const gOptions = gDebugger.DebuggerView.Options;
    const gView = gDebugger.DebuggerView;

    // Should be on by default.
    testAutoPrettyPrintOn();

    Task.spawn(function* () {

      yield waitForSourceShown(gPanel, gFirstSourceLabel);
      testSourceIsUgly();

      yield waitForSourceShown(gPanel, gFirstSourceLabel);
      testSourceIsPretty();
      testPrettyPrintButtonOn();

      // select second source
      yield selectSecondSource();
      testSecondSourceLabel();

      // select first source
      yield selectFirstSource();
      testFirstSourceLabel();
      testPrettyPrintButtonOn();

      // Disable auto pretty printing so it does not affect the following tests.
      yield disableAutoPrettyPrint();

      closeDebuggerAndFinish(gPanel)
        .then(null, aError => {
          ok(false, "Got an error: " + DevToolsUtils.safeErrorString(aError));
        });
    });

    function selectSecondSource() {
      let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN, 2);
      gSources.selectedIndex = 1;
      return finished;
    }

    function selectFirstSource() {
      let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
      gSources.selectedIndex = 0;
      return finished;
    }

    function testSourceIsUgly() {
      ok(!gEditor.getText().includes("\n  "),
        "The source shouldn't be pretty printed yet.");
    }

    function testFirstSourceLabel() {
      let source = gSources.selectedItem.attachment.source;
      ok(source.url === EXAMPLE_URL + gFirstSourceLabel,
        "First source url is correct.");
    }

    function testSecondSourceLabel() {
      let source = gSources.selectedItem.attachment.source;
      ok(source.url === EXAMPLE_URL + gSecondSourceLabel,
        "Second source url is correct.");
    }

    function testAutoPrettyPrintOn() {
      is(gPrefs.autoPrettyPrint, true,
        "The auto-pretty-print pref should be on.");
      is(gOptions._autoPrettyPrint.getAttribute("checked"), "true",
        "The Auto pretty print menu item should be checked.");
    }

    function testPrettyPrintButtonOn() {
      is(gDebugger.document.getElementById("pretty-print").checked, true,
        "The button should be checked when the source is selected.");
    }

    function disableAutoPrettyPrint() {
      gOptions._autoPrettyPrint.setAttribute("checked", "false");
      gOptions._toggleAutoPrettyPrint();
      gOptions._onPopupHidden();
      info("Disabled auto pretty printing.");
    }

    function testSourceIsPretty() {
      ok(gEditor.getText().includes("\n  "),
        "The source should be pretty printed.");
    }

    registerCleanupFunction(function () {
      Services.prefs.setBoolPref("devtools.debugger.auto-pretty-print", gOriginalPref);
    });

  });
}
