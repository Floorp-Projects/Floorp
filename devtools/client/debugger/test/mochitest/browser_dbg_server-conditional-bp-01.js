/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test adding conditional breakpoints (with server-side support)
 */

const TAB_URL = EXAMPLE_URL + "doc_conditional-breakpoints.html";

function test() {
  // Linux debug test slaves are a bit slow at this test sometimes.
  requestLongerTimeout(2);

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

    const addBreakpoints = Task.async(function* () {
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 18 },
                                  "undefined");
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 19 },
                                  "null");
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 20 },
                                  "42");
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 21 },
                                  "true");
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 22 },
                                  "'nasu'");
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 23 },
                                  "/regexp/");
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 24 },
                                  "({})");
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 25 },
                                  "(function() {})");
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 26 },
                                  "(function() { return false; })()");
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 27 },
                                  "a");
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 28 },
                                  "a !== undefined");
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 29 },
                                  "b");
      yield actions.addBreakpoint({ actor: gSources.selectedValue, line: 30 },
                                  "a !== null");
    });

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
        is(gDebugger.document.querySelectorAll(".dbg-breakpoint").length, 13,
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

      ok(selectedActor,
         "There should be a selected item in the sources pane.");
      ok(selectedBreakpoint,
         "There should be a selected breakpoint in the sources pane.");

      let source = gSources.selectedItem.attachment.source;
      let bp = queries.getBreakpoint(getState(), selectedBreakpoint.location);

      ok(bp, "The selected breakpoint exists");
      is(bp.location.actor, source.actor,
         "The breakpoint on line " + line + " wasn't added on the correct source.");
      is(bp.location.line, line,
         "The breakpoint on line " + line + " wasn't found.");
      is(!!bp.disabled, false,
         "The breakpoint on line " + line + " should be enabled.");
      is(!!selectedBreakpointItem.attachment.openPopup, false,
         "The breakpoint on line " + line + " should not have opened a popup.");
      is(gSources._conditionalPopupVisible, false,
         "The breakpoint conditional expression popup should not have been shown.");
      isnot(bp.condition, undefined,
            "The breakpoint on line " + line + " should have a conditional expression.");
      ok(isCaretPos(gPanel, line),
         "The editor caret position is not properly set.");
    }

    const testAfterReload = Task.async(function* () {
      let selectedActor = gSources.selectedValue;
      let selectedBreakpoint = gSources._selectedBreakpoint;

      ok(selectedActor,
         "There should be a selected item in the sources pane after reload.");
      ok(!selectedBreakpoint,
         "There should be no selected breakpoint in the sources pane after reload.");

      yield testBreakpoint(18, true);
      yield testBreakpoint(19, true);
      yield testBreakpoint(20, true);
      yield testBreakpoint(21, true);
      yield testBreakpoint(22, true);
      yield testBreakpoint(23, true);
      yield testBreakpoint(24, true);
      yield testBreakpoint(25, true);
      yield testBreakpoint(26, true);
      yield testBreakpoint(27, true);
      yield testBreakpoint(28, true);
      yield testBreakpoint(29, true);
      yield testBreakpoint(30, true);

      is(gSources.itemCount, 1,
         "Found the expected number of sources.");
      is(gEditor.getText().indexOf("ermahgerd"), 253,
         "The correct source was loaded again.");
      is(gSources.selectedValue, gSources.values[0],
         "The correct source is selected.");

      ok(gSources.selectedItem,
         "There should be a selected source in the sources pane.");
      ok(gSources._selectedBreakpoint,
         "There should be a selected breakpoint in the sources pane.");
      is(gSources._conditionalPopupVisible, false,
         "The breakpoint conditional expression popup should not be shown.");
    });

    Task.spawn(function* () {
      let onCaretUpdated = waitForCaretAndScopes(gPanel, 17);
      callInTab(gTab, "ermahgerd");
      yield onCaretUpdated;

      yield addBreakpoints();

      is(gDebugger.gThreadClient.state, "paused",
         "Should only be getting stack frames while paused.");
      is(queries.getSourceCount(getState()), 1,
         "Found the expected number of sources.");
      is(gEditor.getText().indexOf("ermahgerd"), 253,
         "The correct source was loaded initially.");
      is(gSources.selectedValue, gSources.values[0],
         "The correct source is selected.");
      is(queries.getBreakpoints(getState()).length, 13,
         "13 breakpoints currently added.");

      yield resumeAndTestBreakpoint(20);
      yield resumeAndTestBreakpoint(21);
      yield resumeAndTestBreakpoint(22);
      yield resumeAndTestBreakpoint(23);
      yield resumeAndTestBreakpoint(24);
      yield resumeAndTestBreakpoint(25);
      yield resumeAndTestBreakpoint(27);
      yield resumeAndTestBreakpoint(28);
      yield resumeAndTestBreakpoint(29);
      yield resumeAndTestBreakpoint(30);
      yield resumeAndTestNoBreakpoint();

      let sourceShown = waitForSourceShown(gPanel, ".html");
      reload(gPanel),
      yield sourceShown;
      testAfterReload();

      // When a breakpoint is highlighted, the conditional expression
      // popup opens, and then closes, and when it closes it sends the
      // expression to the server which pauses the server. Make sure
      // we wait if there is a pending request.
      if (gDebugger.gThreadClient.state === "paused") {
        yield waitForThreadEvents(gPanel, "resumed");
      }

      closeDebuggerAndFinish(gPanel);
    });
  });
}
