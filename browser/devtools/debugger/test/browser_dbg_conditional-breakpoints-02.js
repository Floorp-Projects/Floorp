/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 740825: Test the debugger conditional breakpoints.
 */

const TAB_URL = EXAMPLE_URL + "doc_conditional-breakpoints.html";

function test() {
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

    // This test forces conditional breakpoints to be evaluated on the
    // client-side
    var client = gPanel.target.client;
    client.mainRoot.traits.conditionalBreakpoints = false;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 17)
      .then(() => initialChecks())
      .then(() => addBreakpoint1())
      .then(() => testBreakpoint(18, false, false, undefined))
      .then(() => addBreakpoint2())
      .then(() => testBreakpoint(19, false, false, undefined))
      .then(() => modBreakpoint2())
      .then(() => testBreakpoint(19, false, true, undefined))
      .then(() => addBreakpoint3())
      .then(() => testBreakpoint(20, true, false, undefined))
      .then(() => modBreakpoint3())
      .then(() => testBreakpoint(20, true, false, "bamboocha"))
      .then(() => addBreakpoint4())
      .then(() => testBreakpoint(21, false, false, undefined))
      .then(() => delBreakpoint4())
      .then(() => setCaretPosition(18))
      .then(() => testBreakpoint(18, false, false, undefined))
      .then(() => setCaretPosition(19))
      .then(() => testBreakpoint(19, false, false, undefined))
      .then(() => setCaretPosition(20))
      .then(() => testBreakpoint(20, true, false, "bamboocha"))
      .then(() => setCaretPosition(17))
      .then(() => testNoBreakpoint(17))
      .then(() => setCaretPosition(21))
      .then(() => testNoBreakpoint(21))
      .then(() => clickOnBreakpoint(0))
      .then(() => testBreakpoint(18, false, false, undefined))
      .then(() => clickOnBreakpoint(1))
      .then(() => testBreakpoint(19, false, false, undefined))
      .then(() => clickOnBreakpoint(2))
      .then(() => testBreakpoint(20, true, true, "bamboocha"))
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    gDebuggee.ermahgerd();
  });

  function initialChecks() {
    is(gDebugger.gThreadClient.state, "paused",
      "Should only be getting stack frames while paused.");
    is(gSources.itemCount, 1,
      "Found the expected number of sources.");
    is(gEditor.getText().indexOf("ermahgerd"), 253,
      "The correct source was loaded initially.");
    is(gSources.selectedValue, gSources.values[0],
      "The correct source is selected.");

    is(gBreakpointsAdded.size, 0,
      "No breakpoints currently added.");
    is(gBreakpointsRemoving.size, 0,
      "No breakpoints currently being removed.");
    is(gEditor.getBreakpoints().length, 0,
      "No breakpoints currently shown in the editor.");

    ok(!gBreakpoints._getAdded({ url: "foo", line: 3 }),
      "_getAdded('foo', 3) returns falsey.");
    ok(!gBreakpoints._getRemoving({ url: "bar", line: 3 }),
      "_getRemoving('bar', 3) returns falsey.");
  }

  function addBreakpoint1() {
    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_ADDED);
    gPanel.addBreakpoint({ url: gSources.selectedValue, line: 18 });
    return finished;
  }

  function addBreakpoint2() {
    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_ADDED);
    setCaretPosition(19);
    gSources._onCmdAddBreakpoint();
    return finished;
  }

  function modBreakpoint2() {
    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.CONDITIONAL_BREAKPOINT_POPUP_SHOWING);
    setCaretPosition(19);
    gSources._onCmdAddConditionalBreakpoint();
    return finished;
  }

  function addBreakpoint3() {
    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_ADDED);
    setCaretPosition(20);
    gSources._onCmdAddConditionalBreakpoint();
    return finished;
  }

  function modBreakpoint3() {
    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.CONDITIONAL_BREAKPOINT_POPUP_HIDING);
    typeText(gSources._cbTextbox, "bamboocha");
    EventUtils.sendKey("RETURN", gDebugger);
    return finished;
  }

  function addBreakpoint4() {
    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_ADDED);
    setCaretPosition(21);
    gSources._onCmdAddBreakpoint();
    return finished;
  }

  function delBreakpoint4() {
    let finished = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_REMOVED);
    setCaretPosition(21);
    gSources._onCmdAddBreakpoint();
    return finished;
  }

  function testBreakpoint(aLine, aOpenPopupFlag, aPopupVisible, aConditionalExpression) {
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
    is(!!selectedBreakpoint.attachment.openPopup, aOpenPopupFlag,
      "The breakpoint on line " + aLine + " should have a correct popup state (1).");
    is(gSources._conditionalPopupVisible, aPopupVisible,
      "The breakpoint on line " + aLine + " should have a correct popup state (2).");

    return gBreakpoints._getAdded(selectedBreakpoint.attachment).then(aBreakpointClient => {
      is(aBreakpointClient.location.url, selectedUrl,
        "The breakpoint's client url is correct");
      is(aBreakpointClient.location.line, aLine,
        "The breakpoint's client line is correct");
      is(aBreakpointClient.conditionalExpression, aConditionalExpression,
        "The breakpoint on line " + aLine + " should have a correct conditional expression.");
      is("conditionalExpression" in aBreakpointClient, !!aConditionalExpression,
        "The breakpoint on line " + aLine + " should have a correct conditional state.");

      ok(isCaretPos(gPanel, aLine),
        "The editor caret position is not properly set.");
    });
  }

  function testNoBreakpoint(aLine) {
    let selectedUrl = gSources.selectedValue;
    let selectedBreakpoint = gSources._selectedBreakpointItem;

    ok(selectedUrl,
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
}
