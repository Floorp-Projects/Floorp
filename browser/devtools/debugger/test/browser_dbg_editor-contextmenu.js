/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 731394: Test the debugger source editor default context menu.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let gTab, gDebuggee, gPanel, gDebugger;
  let gEditor, gSources, gContextMenu;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gContextMenu = gDebugger.document.getElementById("sourceEditorContextMenu");

    waitForSourceAndCaretAndScopes(gPanel, "-02.js", 6).then(performTest).then(null, info);
    gDebuggee.firstCall();
  });

  function performTest() {
    is(gDebugger.gThreadClient.state, "paused",
      "Should only be getting stack frames while paused.");
    is(gSources.itemCount, 2,
      "Found the expected number of sources.");
    is(gEditor.getText().indexOf("debugger"), 172,
      "The correct source was loaded initially.");
    is(gSources.selectedValue, gSources.values[1],
      "The correct source is selected.");

    is(gEditor.getText().indexOf("\u263a"), 162,
      "Unicode characters are converted correctly.");

    ok(gContextMenu,
      "The source editor's context menupopup is available.");
    ok(gEditor.isReadOnly(),
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
