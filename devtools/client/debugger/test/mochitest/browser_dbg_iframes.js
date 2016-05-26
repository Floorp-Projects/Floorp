/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that iframes can be added as debuggees.
 */

const TAB_URL = EXAMPLE_URL + "doc_iframes.html";

function test() {
  let gTab, gDebuggee, gPanel, gDebugger;
  let gIframe, gEditor, gSources, gFrames;

  let options = {
    source: "inline-debugger-statement.html",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gIframe = gDebuggee.frames[0];
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gFrames = gDebugger.DebuggerView.StackFrames;

    checkIframeSource();
    checkIframePause()
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function checkIframeSource() {
    is(gDebugger.gThreadClient.paused, false,
      "Should be running after starting the test.");

    ok(isCaretPos(gPanel, 1),
      "The source editor caret position was incorrect.");
    is(gFrames.itemCount, 0,
      "Should have only no frames.");

    is(gSources.itemCount, 1,
      "Found the expected number of entries in the sources widget.");
    is(gEditor.getText().indexOf("debugger"), 348,
      "The correct source was loaded initially.");
    is(getSelectedSourceURL(gSources), EXAMPLE_URL + "doc_inline-debugger-statement.html",
      "The currently selected source value is incorrect (0).");
    is(gSources.selectedValue, gSources.values[0],
      "The currently selected source value is incorrect (1).");
  }

  function checkIframePause() {
    // Spin the event loop before causing the debuggee to pause, to allow
    // this function to return first.
    executeSoon(() => gIframe.runDebuggerStatement());

    return waitForCaretAndScopes(gPanel, 16).then(() => {
      is(gDebugger.gThreadClient.paused, true,
        "Should be paused after an interrupt request.");

      ok(isCaretPos(gPanel, 16),
        "The source editor caret position was incorrect.");
      is(gFrames.itemCount, 1,
        "Should have only one frame.");
    });
  }
}
