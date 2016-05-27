/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests opening the variable inspection popup while stopped at a debugger statement,
 * clicking "step in" and verifying that the popup is gone.
 */

const TAB_URL = EXAMPLE_URL + "doc_with-frame.html";

let gTab, gPanel, gDebugger;
let actions, gSources, gVariables;

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    actions = bindActionCreators(gPanel);
    gSources = gDebugger.DebuggerView.Sources;
    gVariables = gDebugger.DebuggerView.Variables;
    let bubble = gDebugger.DebuggerView.VariableBubble;
    let tooltip = bubble._tooltip.panel;
    let testPopupHiding = Task.async(function* () {
      yield addBreakpoint();
      yield ensureThreadClientState(gPanel, "resumed");
      yield pauseDebuggee();
      yield openVarPopup(gPanel, { line: 20, ch: 17 });
      is(tooltip.querySelectorAll(".devtools-tooltip-simple-text").length, 1,
          "The popup should be open with a simple text entry");
      // Now we're stopped at a breakpoint with an open popup
      // we'll send a keypress and check if the popup closes
      executeSoon(() => EventUtils.synthesizeKey("VK_F11", {}));
      // The keypress should cause one resumed event and one paused event
      yield waitForThreadEvents(gPanel, "resumed");
      yield waitForThreadEvents(gPanel, "paused");
      // Here's the state we're actually interested in checking..
      checkVariablePopupClosed(bubble);
      yield resumeDebuggerThenCloseAndFinish(gPanel);
    });
    testPopupHiding();
  });
}

function addBreakpoint() {
  return actions.addBreakpoint({ actor: gSources.selectedValue, line: 21 });
}

function pauseDebuggee() {
  generateMouseClickInTab(gTab, "content.document.querySelector('button')");

  // The first 'with' scope should be expanded by default, but the
  // variables haven't been fetched yet. This is how 'with' scopes work.
  return promise.all([
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_SCOPES),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.FETCHED_VARIABLES)
  ]);
}

function checkVariablePopupClosed(bubble) {
  ok(!bubble.contentsShown(),
    "When stepping, popup should close and be hidden.");
  ok(bubble._tooltip.isEmpty(),
    "The variable inspection popup should now be empty.");
  ok(!bubble._markedText,
    "The marked text in the editor was removed.");
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  actions = null;
  gSources = null;
  gVariables = null;
});
