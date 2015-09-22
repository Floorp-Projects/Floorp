/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the "Copy URL" functionality of the sources panel context menu
 */

const TAB_URL = EXAMPLE_URL + "doc_function-search.html";
const SCRIPT_URI = EXAMPLE_URL + "code_function-search-01.js";

function test() {
  let gTab, gPanel, gDebugger, gSources;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceShown(gPanel, "-01.js")
      .then(openContextMenu)
      .then(testCopyMenuItem)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function clickCopyURL() {
    return new Promise((resolve, reject) => {
      let copyURLMenuItem = gDebugger.document.getElementById("debugger-sources-context-copyurl");
      if (!copyURLMenuItem) {
        reject(new Error("The Copy URL context menu item is not available."));
      }

      ok(copyURLMenuItem, "The Copy URL context menu item is available.");
      EventUtils.synthesizeMouseAtCenter(copyURLMenuItem, {}, gDebugger);
      resolve();
    });
  }

  function testCopyMenuItem() {
    return waitForClipboardPromise(clickCopyURL, SCRIPT_URI);
  }

  function openContextMenu() {
    let contextMenu = gDebugger.document.getElementById("debuggerSourcesContextMenu");
    let contextMenuShown = once(contextMenu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(gSources.selectedItem.prebuiltNode, {type: 'contextmenu'}, gDebugger);
    return contextMenuShown;
  }
}
