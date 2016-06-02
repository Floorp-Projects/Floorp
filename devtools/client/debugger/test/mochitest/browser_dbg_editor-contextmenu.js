/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 731394: Test the debugger source editor default context menu.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gEditor, gSources, gContextMenu;

  let options = {
    source: EXAMPLE_URL + "code_script-switching-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gContextMenu = gDebugger.document.getElementById("sourceEditorContextMenu");

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 1).then(performTest).then(null, info);
    callInTab(gTab, "firstCall");
  });

  function performTest() {
    is(gDebugger.gThreadClient.state, "paused",
      "Should only be getting stack frames while paused.");
    is(gSources.itemCount, 2,
      "Found the expected number of sources.");
    is(gEditor.getText().indexOf("debugger"), 166,
      "The correct source was loaded initially.");
    is(gSources.selectedValue, gSources.values[1],
      "The correct source is selected.");

    is(gEditor.getText().indexOf("\u263a"), 162,
      "Unicode characters are converted correctly.");

    ok(gContextMenu,
      "The source editor's context menupopup is available.");
    ok(gEditor.getOption("readOnly"),
      "The source editor is read only.");

    gEditor.focus();
    gEditor.setSelection({ line: 1, ch: 0 }, { line: 1, ch: 10 });

    once(gContextMenu, "popupshown").then(testContextMenu).then(null, info);
    gContextMenu.openPopup(gEditor.container, "overlap", 0, 0, true, false);
  }

  function testContextMenu() {
    let document = gDebugger.document;

    ok(document.getElementById("editMenuCommands"),
      "#editMenuCommands found.");
    ok(!document.getElementById("editMenuKeys"),
      "#editMenuKeys not found.");

    gContextMenu.hidePopup();
    resumeDebuggerThenCloseAndFinish(gPanel);
  }
}
