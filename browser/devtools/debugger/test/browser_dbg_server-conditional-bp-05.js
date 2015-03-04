/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test conditional breakpoints throwing exceptions
 * with server support
 */

const TAB_URL = EXAMPLE_URL + "doc_conditional-breakpoints.html";

function test() {
  // Linux debug test slaves are a bit slow at this test sometimes.
  requestLongerTimeout(2);

  let gTab, gPanel, gDebugger;
  let gEditor, gSources, gBreakpoints, gBreakpointsAdded, gBreakpointsRemoving;

  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    gBreakpointsAdded = gBreakpoints._added;
    gBreakpointsRemoving = gBreakpoints._removing;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 17)
      .then(() => addBreakpoints())
      .then(() => initialChecks())
      .then(() => resumeAndTestBreakpoint(18))
      .then(() => resumeAndTestBreakpoint(19))
      .then(() => resumeAndTestBreakpoint(20))
      .then(() => resumeAndTestBreakpoint(23))
      .then(() => resumeAndTestNoBreakpoint())
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    callInTab(gTab, "ermahgerd");
  });

  function addBreakpoints() {
    return promise.resolve(null)
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue,
                                         line: 18,
                                         condition: "1a"
                                       }))
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue,
                                         line: 19,
                                         condition: "new Error()"
                                       }))
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue,
                                         line: 20,
                                         condition: "true"
                                       }))
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue,
                                         line: 21,
                                         condition: "false"
                                       }))
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue,
                                         line: 22,
                                         condition: "undefined"
                                       }))
      .then(() => gPanel.addBreakpoint({ actor: gSources.selectedValue,
                                         line: 23,
                                         condition: "randomVar"
                                       }));
  }

  function initialChecks() {
    is(gDebugger.gThreadClient.state, "paused",
      "Should only be getting stack frames while paused.");
    is(gSources.itemCount, 1,
      "Found the expected number of sources.");
    is(gEditor.getText().indexOf("ermahgerd"), 253,
      "The correct source was loaded initially.");
    is(gSources.selectedValue, gSources.values[0],
      "The correct source is selected.");

    is(gBreakpointsAdded.size, 6,
      "6 breakpoints currently added.");
    is(gBreakpointsRemoving.size, 0,
      "No breakpoints currently being removed.");
    is(gEditor.getBreakpoints().length, 6,
      "6 breakpoints currently shown in the editor.");

    ok(!gBreakpoints._getAdded({ url: "foo", line: 3 }),
      "_getAdded('foo', 3) returns falsey.");
    ok(!gBreakpoints._getRemoving({ url: "bar", line: 3 }),
      "_getRemoving('bar', 3) returns falsey.");
  }

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
        "There should be a selected source in the sources pane.")
      ok(!gSources._selectedBreakpointItem,
        "There should be no selected breakpoint in the sources pane.")
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
      "There should be a selected brekapoint in the sources pane.");

    is(selectedBreakpoint.attachment.actor, selectedActor,
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
      is(aBreakpointClient.location.actor, selectedActor,
        "The breakpoint's client url is correct");
      is(aBreakpointClient.location.line, aLine,
        "The breakpoint's client line is correct");
      isnot(aBreakpointClient.condition, undefined,
        "The breakpoint on line " + aLine + " should have a conditional expression.");

      ok(isCaretPos(gPanel, aLine),
        "The editor caret position is not properly set.");
    });
  }
}
