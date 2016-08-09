/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test adding and modifying conditional breakpoints (with server-side support)
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
    const CONDITIONAL_POPUP_SHOWN = gDebugger.EVENTS.CONDITIONAL_BREAKPOINT_POPUP_SHOWN;

    function addBreakpoint1() {
      return actions.addBreakpoint({ actor: gSources.selectedValue, line: 18 });
    }

    function addBreakpoint2() {
      let finished = waitForDispatch(gPanel, constants.ADD_BREAKPOINT);
      setCaretPosition(19);
      gSources._onCmdAddBreakpoint();
      return finished;
    }

    function modBreakpoint2() {
      setCaretPosition(19);

      let popupShown = waitForDebuggerEvents(gPanel, CONDITIONAL_POPUP_SHOWN);
      gSources._onCmdAddConditionalBreakpoint();
      return popupShown;
    }

    function* addBreakpoint3() {
      let finished = waitForDispatch(gPanel, constants.ADD_BREAKPOINT);
      let popupShown = waitForDebuggerEvents(gPanel, CONDITIONAL_POPUP_SHOWN);

      setCaretPosition(20);
      gSources._onCmdAddConditionalBreakpoint();
      yield finished;
      yield popupShown;
    }

    function* modBreakpoint3() {
      setCaretPosition(20);

      let popupShown = waitForDebuggerEvents(gPanel, CONDITIONAL_POPUP_SHOWN);
      gSources._onCmdAddConditionalBreakpoint();
      yield popupShown;

      typeText(gSources._cbTextbox, "bamboocha");

      let finished = waitForDispatch(gPanel, constants.SET_BREAKPOINT_CONDITION);
      EventUtils.sendKey("RETURN", gDebugger);
      yield finished;
    }

    function addBreakpoint4() {
      let finished = waitForDispatch(gPanel, constants.ADD_BREAKPOINT);
      setCaretPosition(21);
      gSources._onCmdAddBreakpoint();
      return finished;
    }

    function delBreakpoint4() {
      let finished = waitForDispatch(gPanel, constants.REMOVE_BREAKPOINT);
      setCaretPosition(21);
      gSources._onCmdAddBreakpoint();
      return finished;
    }

    function testBreakpoint(aLine, aPopupVisible, aConditionalExpression) {
      const source = queries.getSelectedSource(getState());
      ok(source,
         "There should be a selected item in the sources pane.");

      const bp = queries.getBreakpoint(getState(), {
        actor: source.actor,
        line: aLine
      });
      const bpItem = gSources._getBreakpoint(bp);
      ok(bp, "There should be a breakpoint.");
      ok(bpItem, "There should be a breakpoint in the sources pane.");

      is(bp.location.actor, source.actor,
         "The breakpoint on line " + aLine + " wasn't added on the correct source.");
      is(bp.location.line, aLine,
         "The breakpoint on line " + aLine + " wasn't found.");
      is(!!bp.disabled, false,
         "The breakpoint on line " + aLine + " should be enabled.");
      is(gSources._conditionalPopupVisible, aPopupVisible,
         "The breakpoint on line " + aLine + " should have a correct popup state (2).");
      is(bp.condition, aConditionalExpression,
         "The breakpoint on line " + aLine + " should have a correct conditional expression.");
    }

    function testNoBreakpoint(aLine) {
      let selectedActor = gSources.selectedValue;
      let selectedBreakpoint = gSources._selectedBreakpoint;

      ok(selectedActor,
         "There should be a selected item in the sources pane for line " + aLine + ".");
      ok(!selectedBreakpoint,
         "There should be no selected brekapoint in the sources pane for line " + aLine + ".");

      ok(isCaretPos(gPanel, aLine),
         "The editor caret position is not properly set.");
    }

    function setCaretPosition(aLine) {
      gEditor.setCursor({ line: aLine - 1, ch: 0 });
    }

    function clickOnBreakpoint(aIndex) {
      EventUtils.sendMouseEvent({ type: "click" },
                                gDebugger.document.querySelectorAll(".dbg-breakpoint")[aIndex],
                                gDebugger);
    }

    function waitForConditionUpdate() {
      // This will close the popup and send another request to update
      // the condition
      gSources._hideConditionalPopup();
      return waitForDispatch(gPanel, constants.SET_BREAKPOINT_CONDITION);
    }

    Task.spawn(function* () {
      let onCaretUpdated = waitForCaretAndScopes(gPanel, 17);
      callInTab(gTab, "ermahgerd");
      yield onCaretUpdated;

      is(gDebugger.gThreadClient.state, "paused",
         "Should only be getting stack frames while paused.");
      is(queries.getSourceCount(getState()), 1,
         "Found the expected number of sources.");
      is(gEditor.getText().indexOf("ermahgerd"), 253,
         "The correct source was loaded initially.");
      is(gSources.selectedValue, gSources.values[0],
         "The correct source is selected.");

      is(queries.getBreakpoints(getState()).length, 0,
         "No breakpoints currently added.");

      yield addBreakpoint1();
      testBreakpoint(18, false, undefined);

      yield addBreakpoint2();
      testBreakpoint(19, false, undefined);
      yield modBreakpoint2();
      testBreakpoint(19, true, undefined);
      yield waitForConditionUpdate();
      yield addBreakpoint3();
      testBreakpoint(20, true, "");
      yield waitForConditionUpdate();
      yield modBreakpoint3();
      testBreakpoint(20, false, "bamboocha");
      yield addBreakpoint4();
      testBreakpoint(21, false, undefined);
      yield delBreakpoint4();

      setCaretPosition(18);
      is(gSources._selectedBreakpoint.location.line, 18,
         "The selected breakpoint is line 18");
      yield testBreakpoint(18, false, undefined);

      setCaretPosition(19);
      is(gSources._selectedBreakpoint.location.line, 19,
         "The selected breakpoint is line 19");
      yield testBreakpoint(19, false, "");

      setCaretPosition(20);
      is(gSources._selectedBreakpoint.location.line, 20,
         "The selected breakpoint is line 20");
      yield testBreakpoint(20, false, "bamboocha");

      setCaretPosition(17);
      yield testNoBreakpoint(17);

      setCaretPosition(21);
      yield testNoBreakpoint(21);

      clickOnBreakpoint(0);
      is(gSources._selectedBreakpoint.location.line, 18,
         "The selected breakpoint is line 18");
      yield testBreakpoint(18, false, undefined);

      clickOnBreakpoint(1);
      is(gSources._selectedBreakpoint.location.line, 19,
         "The selected breakpoint is line 19");
      yield testBreakpoint(19, false, "");

      clickOnBreakpoint(2);
      is(gSources._selectedBreakpoint.location.line, 20,
         "The selected breakpoint is line 20");
      testBreakpoint(20, true, "bamboocha");

      resumeDebuggerThenCloseAndFinish(gPanel);
    });
  });
}
