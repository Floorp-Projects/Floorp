/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that conditional breakpoints with an exception-throwing expression
 * could pause on hit
 */

const TAB_URL = EXAMPLE_URL + "doc_conditional-breakpoints.html";

function test() {
  let gTab, gPanel, gDebugger, gEditor;
  let gSources, gBreakpoints, gLocation;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;

    // This test forces conditional breakpoints to be evaluated on the
    // client-side
    var client = gPanel.target.client;
    client.mainRoot.traits.conditionalBreakpoints = false;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 17)
      .then(addBreakpoints)
      .then(() => resumeAndTestBreakpoint(18))
      .then(() => resumeAndTestBreakpoint(19))
      .then(() => resumeAndTestBreakpoint(20))
      .then(() => resumeAndTestBreakpoint(23))
      .then(() => resumeAndTestNoBreakpoint())
      .then(() => {
        // Reset traits back to default value
        client.mainRoot.traits.conditionalBreakpoints = true;
      })
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    callInTab(gTab, "ermahgerd");
  });

  function resumeAndTestBreakpoint(aLine) {
    let finished = waitForCaretUpdated(gPanel, aLine).then(() => testBreakpoint(aLine));

    EventUtils.sendMouseEvent({ type: "mousedown" },
      gDebugger.document.getElementById("resume"),
      gDebugger);

    return finished;
  }

  function resumeAndTestNoBreakpoint() {
    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.AFTER_FRAMES_CLEARED).then(() => {
      is(gSources.itemCount, 1,
        "Found the expected number of sources.");
      is(gEditor.getText().indexOf("ermahgerd"), 253,
        "The correct source was loaded initially.");
      is(gSources.selectedValue, gSources.values[0],
        "The correct source is selected.");

      ok(gSources.selectedItem,
        "There should be a selected source in the sources pane.");
      ok(!gSources._selectedBreakpointItem,
        "There should be no selected breakpoint in the sources pane.");
      is(gSources._conditionalPopupVisible, false,
        "The breakpoint conditional expression popup should not be shown.");

      is(gDebugger.document.querySelectorAll(".dbg-stackframe").length, 0,
        "There should be no visible stackframes.");
      is(gDebugger.document.querySelectorAll(".dbg-breakpoint").length, 6,
        "There should be thirteen visible breakpoints.");
    });

    gDebugger.gThreadClient.resume();

    return finished;
  }

  function addBreakpoints() {
    return promise.resolve(null)
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue, line: 18 }))
      .then(aClient => aClient.conditionalExpression = " 1a")
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue, line: 19 }))
      .then(aClient => aClient.conditionalExpression = "new Error()")
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue, line: 20 }))
      .then(aClient => aClient.conditionalExpression = "true")
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue, line: 21 }))
      .then(aClient => aClient.conditionalExpression = "false")
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue, line: 22 }))
      .then(aClient => aClient.conditionalExpression = "0")
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue, line: 23 }))
      .then(aClient => aClient.conditionalExpression = "randomVar");
  }

  function testBreakpoint(aLine, aHighlightBreakpoint) {
    // Highlight the breakpoint only if required.
    if (aHighlightBreakpoint) {
      let finished = waitForCaretUpdated(gPanel, aLine).then(() => testBreakpoint(aLine));
      gSources.highlightBreakpoint({ actor: gSources.selectedValue, line: aLine });
      return finished;
    }

    let selectedActor = gSources.selectedValue;
    let selectedBreakpoint = gSources._selectedBreakpointItem;

    ok(selectedActor,
      "There should be a selected item in the sources pane.");
    ok(selectedBreakpoint,
      "There should be a selected breakpoint in the sources pane.");

    let source = gSources.selectedItem.attachment.source;

    is(selectedBreakpoint.attachment.actor, source.actor,
      "The breakpoint on line " + aLine + " wasn't added on the correct source.");
    is(selectedBreakpoint.attachment.line, aLine,
      "The breakpoint on line " + aLine + " wasn't found.");
    is(!!selectedBreakpoint.attachment.disabled, false,
      "The breakpoint on line " + aLine + " should be enabled.");
    is(!!selectedBreakpoint.attachment.openPopup, false,
      "The breakpoint on line " + aLine + " should not have opened a popup.");
    is(gSources._conditionalPopupVisible, false,
      "The breakpoint conditional expression popup should not have been shown.");

    return gBreakpoints._getAdded(selectedBreakpoint.attachment).then(aBreakpointClient => {
      is(aBreakpointClient.location.url, source.url,
        "The breakpoint's client url is correct");
      is(aBreakpointClient.location.line, aLine,
        "The breakpoint's client line is correct");
      isnot(aBreakpointClient.conditionalExpression, undefined,
        "The breakpoint on line " + aLine + " should have a conditional expression.");

      ok(isCaretPos(gPanel, aLine),
        "The editor caret position is not properly set.");
    });
  }
}
