/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that right clicking and selecting the pretty print context menu
 * item prettifies the source.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print.html";

function test() {
  // Wait for debugger panel to be fully set and break on debugger statement
  let options = {
    source: "code_ugly.js",
    line: 2
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gContextMenu = gDebugger.document.getElementById("sourceEditorContextMenu");

    Task.spawn(function* () {
      const finished = waitForSourceShown(gPanel, "code_ugly.js");
      once(gContextMenu, "popupshown").then(() => {
        const menuItem = gDebugger.document.getElementById("se-dbg-cMenu-prettyPrint");
        menuItem.click();
      });
      gContextMenu.openPopup(gEditor.container, "overlap", 0, 0, true, false);
      yield finished;

      ok(gEditor.getText().includes("\n  "),
         "The source should be pretty printed.");

      resumeDebuggerThenCloseAndFinish(gPanel);
    });
  });
}
