/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests related to source panel keyboard shortcut bindings
 */

const TAB_URL = EXAMPLE_URL + "doc_function-search.html";
const SCRIPT_URI = EXAMPLE_URL + "code_function-search-01.js";

function test() {
  let gTab, gPanel, gDebugger, gSources;

  let options = {
    source: SCRIPT_URI,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    testCopyURLShortcut()
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function testCopyURLShortcut() {
    return waitForClipboardPromise(sendCopyShortcut, SCRIPT_URI);
  }

  function sendCopyShortcut() {
    EventUtils.synthesizeKey("C", { accelKey: true }, gDebugger);
  }
}
