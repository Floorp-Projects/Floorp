/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that conditional breakpoints with undefined expressions
 * are stored as plain breakpoints when re-enabling them.
 */

const TAB_URL = EXAMPLE_URL + "doc_conditional-breakpoints.html";

function test() {
  let gTab, gDebuggee, gPanel, gDebugger;
  let gSources, gBreakpoints, gLocation;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;

    // This test forces conditional breakpoints to be evaluated on the
    // client-side
    var client = gPanel.target.client;
    client.mainRoot.traits.conditionalBreakpoints = false;

    gLocation = { url: gSources.selectedValue, line: 18 };

    waitForSourceAndCaretAndScopes(gPanel, ".html", 17)
      .then(addBreakpoint)
      .then(setDummyConditional)
      .then(() => {
        let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_REMOVED);
        toggleBreakpoint();
        return finished;
      })
      .then(() => {
        let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_ADDED);
        toggleBreakpoint();
        return finished;
      })
      .then(testConditionalExpressionOnClient)
      .then(() => {
        let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.CONDITIONAL_BREAKPOINT_POPUP_SHOWING);
        openConditionalPopup();
        finished.then(() => ok(false, "The popup shouldn't have opened."));
        return waitForTime(1000);
      })
      .then(() => {
        // Reset traits back to default value
        client.mainRoot.traits.conditionalBreakpoints = true;
      })
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    gDebuggee.ermahgerd();
  });

  function addBreakpoint() {
    return gPanel.addBreakpoint(gLocation);
  }

  function setDummyConditional(aClient) {
    // This happens when a conditional expression input popup is shown
    // but the user doesn't type anything into it.
    aClient.conditionalExpression = "";
  }

  function toggleBreakpoint() {
    EventUtils.sendMouseEvent({ type: "click" },
      gDebugger.document.querySelector(".dbg-breakpoint-checkbox"),
      gDebugger);
  }

  function openConditionalPopup() {
    EventUtils.sendMouseEvent({ type: "click" },
      gDebugger.document.querySelector(".dbg-breakpoint"),
      gDebugger);
  }

  function testConditionalExpressionOnClient() {
    return gBreakpoints._getAdded(gLocation).then(aClient => {
      if ("conditionalExpression" in aClient) {
        ok(false, "A conditional expression shouldn't have been set.");
      } else {
        ok(true, "The conditional expression wasn't set, as expected.");
      }
    });
  }
}
