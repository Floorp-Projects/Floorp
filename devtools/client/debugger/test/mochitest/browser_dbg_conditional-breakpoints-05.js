/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that conditional breakpoints with an exception-throwing expression
 * could pause on hit
 */

const TAB_URL = EXAMPLE_URL + "doc_conditional-breakpoints.html";

function test() {
  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gTab = aTab;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require("./content/queries");
    const constants = gDebugger.require("./content/constants");
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    // This test forces conditional breakpoints to be evaluated on the
    // client-side
    var client = gPanel.target.client;
    client.mainRoot.traits.conditionalBreakpoints = false;

    function resumeAndTestBreakpoint(line) {
      let finished = waitForCaretUpdated(gPanel, line).then(() => testBreakpoint(line));
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
        ok(!gSources._selectedBreakpoint,
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

    function testBreakpoint(line, highlightBreakpoint) {
      // Highlight the breakpoint only if required.
      if (highlightBreakpoint) {
        let finished = waitForCaretUpdated(gPanel, line).then(() => testBreakpoint(line));
        gSources.highlightBreakpoint({ actor: gSources.selectedValue, line: line });
        return finished;
      }

      let selectedActor = gSources.selectedValue;
      let selectedBreakpoint = gSources._selectedBreakpoint;
      let selectedBreakpointItem = gSources._getBreakpoint(selectedBreakpoint);
      let source = queries.getSource(getState(), selectedActor);

      ok(selectedActor,
         "There should be a selected item in the sources pane.");
      ok(selectedBreakpoint,
         "There should be a selected breakpoint.");
      ok(selectedBreakpointItem,
         "There should be a selected breakpoint item in the sources pane.");

      is(selectedBreakpoint.location.actor, source.actor,
         "The breakpoint on line " + line + " wasn't added on the correct source.");
      is(selectedBreakpoint.location.line, line,
         "The breakpoint on line " + line + " wasn't found.");
      is(!!selectedBreakpoint.location.disabled, false,
         "The breakpoint on line " + line + " should be enabled.");
      is(gSources._conditionalPopupVisible, false,
         "The breakpoint conditional expression popup should not have been shown.");

      isnot(selectedBreakpoint.condition, undefined,
            "The breakpoint on line " + line + " should have a conditional expression.");

      ok(isCaretPos(gPanel, line),
         "The editor caret position is not properly set.");
    }

    Task.spawn(function* () {
      let onCaretUpdated = waitForCaretAndScopes(gPanel, 17);
      callInTab(gTab, "ermahgerd");
      yield onCaretUpdated;

      yield actions.addBreakpoint(
        { actor: gSources.selectedValue, line: 18 }, " 1a"
      );
      yield actions.addBreakpoint(
        { actor: gSources.selectedValue, line: 19 }, "new Error()"
      );
      yield actions.addBreakpoint(
        { actor: gSources.selectedValue, line: 20 }, "true"
      );
      yield actions.addBreakpoint(
        { actor: gSources.selectedValue, line: 21 }, "false"
      );
      yield actions.addBreakpoint(
        { actor: gSources.selectedValue, line: 22 }, "0"
      );
      yield actions.addBreakpoint(
        { actor: gSources.selectedValue, line: 23 }, "randomVar"
      );

      yield resumeAndTestBreakpoint(18);
      yield resumeAndTestBreakpoint(19);
      yield resumeAndTestBreakpoint(20);
      yield resumeAndTestBreakpoint(23);
      yield resumeAndTestNoBreakpoint();

      // Reset traits back to default value
      client.mainRoot.traits.conditionalBreakpoints = true;
      closeDebuggerAndFinish(gPanel);
    });
  });
}
