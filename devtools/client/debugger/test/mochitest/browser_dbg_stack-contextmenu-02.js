/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the copy contextmenu copys the stack frames to the clipboard.
 */

const TAB_URL = EXAMPLE_URL + "doc_recursion-stack.html";
const STACK_STRING = "simpleCall@" + EXAMPLE_URL + "doc_recursion-stack.html:14:8";

function test() {
  let gTab, gPanel, gDebugger, gFrames;
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gFrames = gDebugger.DebuggerView.StackFrames;

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.AFTER_FRAMES_REFILLED)
     .then(openContextMenu)
     .then(testCopyStackMenuItem)
     .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
     .catch(aError => {
       ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
     });
    callInTab(gTab, "simpleCall");
  });

  function clickCopyStack() {
    return new Promise((resolve, reject) => {
      let copyStackMenuItem = gDebugger.document.getElementById("copyStackMenuItem");
      if (!copyStackMenuItem) {
        reject(new Error("The Copy stack context menu item is not available."));
      }

      ok(copyStackMenuItem, "The Copy stack context menu item is available.");
      EventUtils.synthesizeMouseAtCenter(copyStackMenuItem, {}, gDebugger);
      resolve();
    });
  }

  function testCopyStackMenuItem() {
    return waitForClipboardPromise(clickCopyStack, STACK_STRING);
  }

  function openContextMenu() {
    let contextMenu = gDebugger.document.getElementById("stackFramesContextMenu");
    let contextMenuShown = once(contextMenu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(gFrames.getItemAtIndex(0).prebuiltNode, {type: "contextmenu", button: 2}, gDebugger);
    return contextMenuShown;
  }
}
