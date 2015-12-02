/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that right clicking and selecting the pretty print context menu
 * item prettifies the source.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print.html";

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gContextMenu = gDebugger.document.getElementById("sourceEditorContextMenu");

    Task.spawn(function*() {
      yield waitForSourceShown(gPanel, "code_ugly.js");

      const finished = waitForSourceShown(gPanel, "code_ugly.js");
      once(gContextMenu, "popupshown").then(() => {
        const menuItem = gDebugger.document.getElementById("se-dbg-cMenu-prettyPrint");
        menuItem.click();
      });
      gContextMenu.openPopup(gEditor.container, "overlap", 0, 0, true, false);
      yield finished;

      ok(gEditor.getText().includes("\n  "),
         "The source should be pretty printed.")

      closeDebuggerAndFinish(gPanel);
    });
  });
}
