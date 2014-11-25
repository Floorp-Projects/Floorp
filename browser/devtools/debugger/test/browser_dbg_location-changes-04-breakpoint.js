/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that reloading a page with a breakpoint set does not cause it to
 * fire more than once.
 */

const TAB_URL = EXAMPLE_URL + "doc_included-script.html";
const SOURCE_URL = EXAMPLE_URL + "code_location-changes.js";

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 17).then(addBreakpoint);
    gDebuggee.runDebuggerStatement();
  });
}

function addBreakpoint() {
  waitForSourceAndCaret(gPanel, ".js", 5).then(() => {
    ok(true,
      "Switched to the desired function when adding a breakpoint " +
      "but not passing { noEditorUpdate: true } as an option.");

    testResume();
  });

  gPanel.addBreakpoint({ actor: getSourceActor(gSources, SOURCE_URL), line: 5 });
}

function testResume() {
  is(gDebugger.gThreadClient.state, "paused",
    "The breakpoint wasn't hit yet (1).");
  is(getSelectedSourceURL(gSources), SOURCE_URL,
    "The currently shown source is incorrect (1).");
  ok(isCaretPos(gPanel, 5),
    "The source editor caret position is incorrect (1).");

  gDebugger.gThreadClient.resume(testClick);
}

function testClick() {
  isnot(gDebugger.gThreadClient.state, "paused",
    "The breakpoint wasn't hit yet (2).");
  is(getSelectedSourceURL(gSources), SOURCE_URL,
    "The currently shown source is incorrect (2).");
  ok(isCaretPos(gPanel, 5),
    "The source editor caret position is incorrect (2).");

  gDebugger.gThreadClient.addOneTimeListener("paused", (aEvent, aPacket) => {
    is(aPacket.why.type, "breakpoint",
      "Execution has advanced to the breakpoint.");
    isnot(aPacket.why.type, "debuggerStatement",
      "The breakpoint was hit before the debugger statement.");

    ensureCaretAt(gPanel, 5, 1, true).then(afterBreakpointHit);
  });

  EventUtils.sendMouseEvent({ type: "click" },
    gDebuggee.document.querySelector("button"),
    gDebuggee);
}

function afterBreakpointHit() {
  is(gDebugger.gThreadClient.state, "paused",
     "The breakpoint was hit (3).");
  is(getSelectedSourceURL(gSources), SOURCE_URL,
    "The currently shown source is incorrect (3).");
  ok(isCaretPos(gPanel, 5),
    "The source editor caret position is incorrect (3).");

  gDebugger.gThreadClient.addOneTimeListener("paused", (aEvent, aPacket) => {
    is(aPacket.why.type, "debuggerStatement",
      "Execution has advanced to the next line.");
    isnot(aPacket.why.type, "breakpoint",
      "No ghost breakpoint was hit.");

    ensureCaretAt(gPanel, 6, 1, true).then(afterDebuggerStatementHit);
  });

  gDebugger.gThreadClient.resume();
}

function afterDebuggerStatementHit() {
  is(gDebugger.gThreadClient.state, "paused",
    "The debugger statement was hit (4).");
  is(getSelectedSourceURL(gSources), SOURCE_URL,
    "The currently shown source is incorrect (4).");
  ok(isCaretPos(gPanel, 6),
    "The source editor caret position is incorrect (4).");

  promise.all([
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.NEW_SOURCE),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCES_ADDED),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN),
    reloadActiveTab(gPanel, gDebugger.EVENTS.BREAKPOINT_SHOWN_IN_EDITOR),
    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_SHOWN_IN_PANE)
  ]).then(testClickAgain);
}

function testClickAgain() {
  isnot(gDebugger.gThreadClient.state, "paused",
    "The breakpoint wasn't hit yet (5).");
  is(getSelectedSourceURL(gSources), SOURCE_URL,
    "The currently shown source is incorrect (5).");
  ok(isCaretPos(gPanel, 1),
    "The source editor caret position is incorrect (5).");

  gDebugger.gThreadClient.addOneTimeListener("paused", (aEvent, aPacket) => {
    is(aPacket.why.type, "breakpoint",
      "Execution has advanced to the breakpoint.");
    isnot(aPacket.why.type, "debuggerStatement",
      "The breakpoint was hit before the debugger statement.");

    ensureCaretAt(gPanel, 5, 1, true).then(afterBreakpointHitAgain);
  });

  EventUtils.sendMouseEvent({ type: "click" },
    gDebuggee.document.querySelector("button"),
    gDebuggee);
}

function afterBreakpointHitAgain() {
  is(gDebugger.gThreadClient.state, "paused",
    "The breakpoint was hit (6).");
  is(getSelectedSourceURL(gSources), SOURCE_URL,
    "The currently shown source is incorrect (6).");
  ok(isCaretPos(gPanel, 5),
    "The source editor caret position is incorrect (6).");

  gDebugger.gThreadClient.addOneTimeListener("paused", (aEvent, aPacket) => {
    is(aPacket.why.type, "debuggerStatement",
      "Execution has advanced to the next line.");
    isnot(aPacket.why.type, "breakpoint",
      "No ghost breakpoint was hit.");

    ensureCaretAt(gPanel, 6, 1, true).then(afterDebuggerStatementHitAgain);
  });

  gDebugger.gThreadClient.resume();
}

function afterDebuggerStatementHitAgain() {
  is(gDebugger.gThreadClient.state, "paused",
    "The debugger statement was hit (7).");
  is(getSelectedSourceURL(gSources), SOURCE_URL,
    "The currently shown source is incorrect (7).");
  ok(isCaretPos(gPanel, 6),
    "The source editor caret position is incorrect (7).");

  showSecondSource();
}

function showSecondSource() {
  gDebugger.once(gDebugger.EVENTS.SOURCE_SHOWN, () => {
    is(gEditor.getText().indexOf("debugger"), 447,
      "The correct source is shown in the source editor.")
    is(gEditor.getBreakpoints().length, 0,
      "No breakpoints should be shown for the second source.");

    ensureCaretAt(gPanel, 1, 1, true).then(showFirstSourceAgain);
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.querySelectorAll(".side-menu-widget-item-contents")[1],
    gDebugger);
}

function showFirstSourceAgain() {
  gDebugger.once(gDebugger.EVENTS.SOURCE_SHOWN, () => {
    is(gEditor.getText().indexOf("debugger"), 148,
      "The correct source is shown in the source editor.")
    is(gEditor.getBreakpoints().length, 1,
      "One breakpoint should be shown for the first source.");

    ensureCaretAt(gPanel, 6, 1, true).then(() => resumeDebuggerThenCloseAndFinish(gPanel));
  });

  EventUtils.sendMouseEvent({ type: "mousedown" },
    gDebugger.document.querySelectorAll(".side-menu-widget-item-contents")[0],
    gDebugger);
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
});
