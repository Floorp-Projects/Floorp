/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that large files are treated differently in the debugger:
 *   1) No parsing to determine current symbol is attempted when
 *      starting a search
 */

const TAB_URL = EXAMPLE_URL + "doc_function-search.html";

function test() {
  let options = {
    source: "-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab, aDebuggee, aPanel]) => {
    const gTab = aTab;
    const gDebuggee = aDebuggee;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;

    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const Filtering = gDebugger.DebuggerView.Filtering;

    // Setting max size so that code_function-search-01.js will be
    // considered a large file on first load
    gDebugger.DebuggerView.LARGE_FILE_SIZE = 1;

    function testLargeFile() {
      ok(gEditor.getText().length > gDebugger.DebuggerView.LARGE_FILE_SIZE,
         "First source is considered a large file.");
      is(gEditor.getMode().name, "javascript",
         "Editor is syntax highlighting.");
      ok(gEditor.getText().includes("First source!"),
         "Editor text contents appears to be correct.");

      // Press ctrl+f with the cursor in a token
      gEditor.focus();
      gEditor.setCursor({ line: 3, ch: 10});
      synthesizeKeyFromKeyTag(gDebugger.document.getElementById("tokenSearchKey"));
      is(Filtering._searchbox.value, "#",
        "Search box is NOT prefilled with current token");
    }

    function testSmallFile() {
      ok(gEditor.getText().length < gDebugger.DebuggerView.LARGE_FILE_SIZE,
         "Second source is considered a small file.");
      is(gEditor.getMode().name, "javascript",
         "Editor is syntax highlighting.");
      ok(gEditor.getText().includes("First source!"),
         "Editor text contents appears to be correct.");

      // Press ctrl+f with the cursor in a token
      gEditor.focus();
      gEditor.setCursor({ line: 3, ch: 10});
      synthesizeKeyFromKeyTag(gDebugger.document.getElementById("tokenSearchKey"));
      is(Filtering._searchbox.value, "#test",
        "Search box is prefilled with current token");
    }

    Task.spawn(function* () {
      yield testLargeFile();

      info("Making it appear as a small file and then reselecting 01.js");
      gDebugger.DebuggerView.LARGE_FILE_SIZE = 1000;
      gSources.selectedIndex = 1;
      yield waitForSourceShown(gPanel, "-02.js");
      gSources.selectedIndex = 0;
      yield waitForSourceShown(gPanel, "-01.js");

      yield testSmallFile();

      closeDebuggerAndFinish(gPanel);
    });
  });
}
