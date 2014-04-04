/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 740825: Test adding conditional breakpoints (with server-side support)
 */

const TAB_URL = EXAMPLE_URL + "doc_conditional-breakpoints.html";

function test() {
  // Linux debug test slaves are a bit slow at this test sometimes.
  requestLongerTimeout(2);

  let gTab, gDebuggee, gPanel, gDebugger;
  let gEditor, gSources, gBreakpoints, gBreakpointsAdded, gBreakpointsRemoving;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
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
      .then(() => resumeAndTestBreakpoint(20))
      .then(() => resumeAndTestBreakpoint(21))
      .then(() => resumeAndTestBreakpoint(22))
      .then(() => resumeAndTestBreakpoint(23))
      .then(() => resumeAndTestBreakpoint(24))
      .then(() => resumeAndTestBreakpoint(25))
      .then(() => resumeAndTestBreakpoint(27))
      .then(() => resumeAndTestBreakpoint(28))
      .then(() => resumeAndTestBreakpoint(29))
      .then(() => resumeAndTestNoBreakpoint())
      .then(() => reloadActiveTab(gPanel, gDebugger.EVENTS.BREAKPOINT_SHOWN, 13))
      .then(() => testAfterReload())
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    gDebuggee.ermahgerd();
  });

  function addBreakpoints() {
    return promise.resolve(null)
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 18,
                                         condition: "undefined"
                                       }))
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 19,
                                         condition: "null"
                                       }))
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 20,
                                         condition: "42"
                                       }))
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 21,
                                         condition: "true"
                                       }))
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 22,
                                         condition: "'nasu'"
                                       }))
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 23,
                                         condition: "/regexp/"
                                       }))
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 24,
                                         condition: "({})"
                                       }))
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 25,
                                         condition: "(function() {})"
                                       }))
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 26,
                                         condition: "(function() { return false; })()"
                                       }))
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 27,
                                         condition: "a"
                                       }))
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 28,
                                         condition: "a !== undefined"
                                       }))
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 29,
                                         condition: "a !== null"
                                       }))
      .then(() => gPanel.addBreakpoint({ url: gSources.selectedValue,
                                         line: 30,
                                         condition: "b"
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

    is(gBreakpointsAdded.size, 13,
      "13 breakpoints currently added.");
    is(gBreakpointsRemoving.size, 0,
      "No breakpoints currently being removed.");
    is(gEditor.getBreakpoints().length, 13,
      "13 breakpoints currently shown in the editor.");

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
      is(gDebugger.document.querySelectorAll(".dbg-breakpoint").length, 13,
        "There should be thirteen visible breakpoints.");
    });

    gDebugger.gThreadClient.resume();

    return finished;
  }

  function testBreakpoint(aLine, aHighlightBreakpoint) {
    // Highlight the breakpoint only if required.
    if (aHighlightBreakpoint) {
      let finished = waitForCaretUpdated(gPanel, aLine).then(() => testBreakpoint(aLine));
      gSources.highlightBreakpoint({ url: gSources.selectedValue, line: aLine });
      return finished;
    }

    let selectedUrl = gSources.selectedValue;
    let selectedBreakpoint = gSources._selectedBreakpointItem;

    ok(selectedUrl,
      "There should be a selected item in the sources pane.");
    ok(selectedBreakpoint,
      "There should be a selected brekapoint in the sources pane.");

    is(selectedBreakpoint.attachment.url, selectedUrl,
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
      is(aBreakpointClient.location.url, selectedUrl,
        "The breakpoint's client url is correct");
      is(aBreakpointClient.location.line, aLine,
        "The breakpoint's client line is correct");
      isnot(aBreakpointClient.condition, undefined,
        "The breakpoint on line " + aLine + " should have a conditional expression.");

      ok(isCaretPos(gPanel, aLine),
        "The editor caret position is not properly set.");
    });
  }

  function testAfterReload() {
    let selectedUrl = gSources.selectedValue;
    let selectedBreakpoint = gSources._selectedBreakpointItem;

    ok(selectedUrl,
      "There should be a selected item in the sources pane after reload.");
    ok(!selectedBreakpoint,
      "There should be no selected brekapoint in the sources pane after reload.");

    return promise.resolve(null)
      .then(() => testBreakpoint(18, true))
      .then(() => testBreakpoint(19, true))
      .then(() => testBreakpoint(20, true))
      .then(() => testBreakpoint(21, true))
      .then(() => testBreakpoint(22, true))
      .then(() => testBreakpoint(23, true))
      .then(() => testBreakpoint(24, true))
      .then(() => testBreakpoint(25, true))
      .then(() => testBreakpoint(26, true))
      .then(() => testBreakpoint(27, true))
      .then(() => testBreakpoint(28, true))
      .then(() => testBreakpoint(29, true))
      .then(() => testBreakpoint(30, true))
      .then(() => {
        is(gSources.itemCount, 1,
          "Found the expected number of sources.");
        is(gEditor.getText().indexOf("ermahgerd"), 253,
          "The correct source was loaded again.");
        is(gSources.selectedValue, gSources.values[0],
          "The correct source is selected.");

        ok(gSources.selectedItem,
          "There should be a selected source in the sources pane.")
        ok(gSources._selectedBreakpointItem,
          "There should be a selected breakpoint in the sources pane.")
        is(gSources._conditionalPopupVisible, false,
          "The breakpoint conditional expression popup should not be shown.");
      });
  }
}
