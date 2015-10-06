/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests opening the variable inspection popup while stopped at a breakpoint,
 * faking a [F11] "step in" keypress and verifying that the popup is gone.
 */

const TAB_URL = EXAMPLE_URL + "doc_with-frame.html";

function test() {
  let gTab, gPanel, gDebugger;
  let gBreakpoints, gSources;
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    gSources = gDebugger.DebuggerView.Sources;
    let testPopupHiding = Task.async(function *(){
      let bubble = gDebugger.DebuggerView.VariableBubble;
      let tooltip = bubble._tooltip.panel;
      // Add a breakpoint
      yield gBreakpoints.addBreakpoint({ actor: gSources.selectedValue,
        line: 21 });
      yield ensureThreadClientState(gPanel, "resumed");
      // Click button in page to start script execution, will stop at breakpoint
      yield pauseDebuggee();
      yield openVarPopup(gPanel, { line: 20, ch: 17 });
      is(tooltip.querySelectorAll(".devtools-tooltip-simple-text").length, 1,
          "The popup should be open with a simple text entry");
      ok(bubble.contentsShown(),
          "The popup API should confirm the popup is open.");
      // Now we're stopped at a breakpoint with an open popup
      // we'll send a keypress and check if the popup closes
      EventUtils.synthesizeKey('VK_F11', {});
      // The keypress should cause one resumed event and one paused event
      yield waitForThreadEvents(gPanel, "resumed");
      yield waitForThreadEvents(gPanel, "paused");
      // Here's the state we're actually interested in checking..
      console.log('bubble ' + bubble.contentsShown);
      console.log('bubble ' + bubble.contentsShown());
      ok(!bubble.contentsShown(),
        "When stepping, popup should close and be hidden.");
      ok(bubble._tooltip.isEmpty(),
        "The variable inspection popup should now be empty.");
      ok(!bubble._markedText,
        "The marked text in the editor was removed.");

      yield resumeDebuggerThenCloseAndFinish(gPanel);
    });
    waitForSourceShown(gPanel, TAB_URL).then(testPopupHiding)
  });

  let pauseDebuggee = function() {
    generateMouseClickInTab(gTab, "content.document.querySelector('button')");
    return promise.all([
      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES),
      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_VARIABLES)
    ]);
  }

}
