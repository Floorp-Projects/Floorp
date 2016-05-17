/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * When the split console is focused and the debugger is open,
 * debugger shortcut keys like F11 should work
 */
const TAB_URL = EXAMPLE_URL + "doc_step-many-statements.html";

function test() {
  // This does the same assertions over a series of sub-tests, and it
  // can timeout in linux e10s.  No sense in breaking it up into multiple
  // tests, so request extra time.
  requestLongerTimeout(2);

  let gDebugger, gToolbox, gThreadClient, gTab, gPanel;
  initDebugger(TAB_URL).then(([aTab, debuggeeWin, aPanel]) => {
    gPanel = aPanel;
    gDebugger = aPanel.panelWin;
    gToolbox = gDevTools.getToolbox(aPanel.target);
    gTab = aTab;
    gThreadClient = gDebugger.DebuggerController.activeThread;
    waitForSourceShown(aPanel, TAB_URL).then(testConsole);
  });
  let testConsole = Task.async(function* () {
    // We need to open the split console (with an ESC keypress),
    // then get the script into a paused state by pressing a button in the page,
    // ensure focus is in the split console,
    // synthesize a few keys - important ones we share listener for are
    // "resumeKey", "stepOverKey", "stepInKey", "stepOutKey"
    // then check that
    //   * The input cursor remains in the console's input box
    //   * The paused state is as expected
    //   * the debugger cursor is where we want it
    let jsterm = yield getSplitConsole(gToolbox, gDebugger);
    // The console is now open (if not make the test fail already)
    ok(gToolbox.splitConsole, "Split console is shown.");

    // Information for sub-tests. When 'key' is synthesized 'keyRepeat' times,
    // cursor should be at 'caretLine' of this test..
    let stepTests = [
      {key: "VK_F11", keyRepeat: 1, caretLine: 16},
      {key: "VK_F11", keyRepeat: 2, caretLine: 18},
      {key: "VK_F11", keyRepeat: 2, caretLine: 27},
      {key: "VK_F10", keyRepeat: 1, caretLine: 27},
      {key: "VK_F11", keyRepeat: 1, caretLine: 18},
      {key: "VK_F11", keyRepeat: 5, caretLine: 32},
      {key: "VK_F11", modifier:"Shift", keyRepeat: 1, caretLine: 29},
      {key: "VK_F11", modifier:"Shift", keyRepeat: 2, caretLine: 34},
      {key: "VK_F11", modifier:"Shift", keyRepeat: 2, caretLine: 34}
    ];
    // Trigger script that stops at debugger statement
    executeSoon(() => generateMouseClickInTab(gTab,
      "content.document.getElementById('start')"));
    yield waitForPause(gThreadClient);

    // Focus the console and add event listener to track whether it loses focus
    // (Must happen after generateMouseClickInTab() call)
    let consoleLostFocus = false;
    jsterm.focus();
    jsterm.inputNode.addEventListener("blur", () => {consoleLostFocus = true;});

    is(gThreadClient.paused, true,
      "Should be paused at debugger statement.");
    // As long as we have test work to do..
    for (let i = 0, thisTest; thisTest = stepTests[i]; i++) {
      // First we send another key event if required by the test
      while (thisTest.keyRepeat > 0) {
        thisTest.keyRepeat --;
        let keyMods = thisTest.modifier === "Shift" ? {shiftKey:true} : {};
        executeSoon(() => {EventUtils.synthesizeKey(thisTest.key, keyMods);});
        yield waitForPause(gThreadClient);
      }

      // We've sent the required number of keys
      // Here are the conditions we're interested in: paused state,
      // cursor still in console (tested later), caret correct in editor
      is(gThreadClient.paused, true,
        "Should still be paused");
      // ok(isCaretPos(gPanel, thisTest.caretLine),
      //  "Test " + i + ": CaretPos at line " + thisTest.caretLine);
      ok(isDebugPos(gPanel, thisTest.caretLine),
        "Test " + i + ": DebugPos at line " + thisTest.caretLine);
    }
    // Did focus go missing while we were stepping?
    is(consoleLostFocus, false, "Console input should not lose focus");
    // We're done with the tests in the stepTests array
    // Last key we test is "resume"
    executeSoon(() => EventUtils.synthesizeKey("VK_F8", {}));

    // We reset the variable tracking loss of focus to test the resume case
    consoleLostFocus = false;

    gPanel.target.on("thread-resumed", () => {
      is(gThreadClient.paused, false,
        "Should not be paused after resume");
      // Final test: did we preserve console inputNode focus during resume?
      is(consoleLostFocus, false, "Resume - console should keep focus");
      closeDebuggerAndFinish(gPanel);
    });
  });
}
