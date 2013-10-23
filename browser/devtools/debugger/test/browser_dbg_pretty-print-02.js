/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that right clicking and selecting the pretty print context menu
 * item prettifies the source.
 */

const TAB_URL = EXAMPLE_URL + "doc_pretty-print.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gContextMenu;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gContextMenu = gDebugger.document.getElementById("sourceEditorContextMenu");

    waitForSourceShown(gPanel, "code_ugly.js")
      .then(() => {
        const finished = waitForSourceShown(gPanel, "code_ugly.js");
        selectContextMenuItem();
        return finished;
      })
      .then(testSourceIsPretty)
      .then(closeDebuggerAndFinish.bind(null, gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + DevToolsUtils.safeErrorString(aError));
      });
  });
}

function selectContextMenuItem() {
  once(gContextMenu, "popupshown").then(() => {
    const menuItem = gDebugger.document.getElementById("se-dbg-cMenu-prettyPrint");
    menuItem.click();
  });
  gContextMenu.openPopup(gEditor.container, "overlap", 0, 0, true, false);
}

function testSourceIsPretty() {
  ok(gEditor.getText().contains("\n    "),
     "The source should be pretty printed.")
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gContextMenu = null;
});
