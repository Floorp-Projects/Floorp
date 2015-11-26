/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that reloading a page with a breakpoint set does not cause it to
 * fire more than once.
 */

const TAB_URL = EXAMPLE_URL + "doc_included-script.html";
const SOURCE_URL = EXAMPLE_URL + "code_location-changes.js";

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    const gTab = aTab;
    const gDebuggee = aDebuggee;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require('./content/queries');
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    function clickButtonAndPause() {
      const paused = waitForPause(gDebugger.gThreadClient);
      BrowserTestUtils.synthesizeMouse("button", 2, 2, {}, gBrowser.selectedBrowser);
      return paused;
    }

    Task.spawn(function*() {
      yield waitForSourceAndCaretAndScopes(gPanel, ".html", 17);

      const location = { actor: getSourceActor(gSources, SOURCE_URL), line: 5 };
      yield actions.addBreakpoint(location);

      const caretUpdated = waitForSourceAndCaret(gPanel, ".js", 5);
      gSources.highlightBreakpoint(location);
      yield caretUpdated;
      ok(true, "Switched to the desired function when adding a breakpoint");

      is(gDebugger.gThreadClient.state, "paused",
         "The breakpoint was hit (1).");
      is(getSelectedSourceURL(gSources), SOURCE_URL,
         "The currently shown source is correct (1).");
      ok(isCaretPos(gPanel, 5),
         "The source editor caret position is correct (1).");

      yield doResume(gPanel);

      isnot(gDebugger.gThreadClient.state, "paused",
            "The breakpoint was not hit yet (2).");
      is(getSelectedSourceURL(gSources), SOURCE_URL,
         "The currently shown source is correct (2).");
      ok(isCaretPos(gPanel, 5),
         "The source editor caret position is correct (2).");

      let packet = yield clickButtonAndPause();
      is(packet.why.type, "breakpoint",
         "Execution has advanced to the breakpoint.");
      isnot(packet.why.type, "debuggerStatement",
            "The breakpoint was hit before the debugger statement.");
      yield ensureCaretAt(gPanel, 5, 1, true);

      is(gDebugger.gThreadClient.state, "paused",
         "The breakpoint was hit (3).");
      is(getSelectedSourceURL(gSources), SOURCE_URL,
         "The currently shown source is incorrect (3).");
      ok(isCaretPos(gPanel, 5),
         "The source editor caret position is incorrect (3).");

      let paused = waitForPause(gDebugger.gThreadClient);
      gDebugger.gThreadClient.resume();
      packet = yield paused;

      is(packet.why.type, "debuggerStatement",
         "Execution has advanced to the next line.");
      isnot(packet.why.type, "breakpoint",
            "No ghost breakpoint was hit.");

      yield ensureCaretAt(gPanel, 6, 1, true);

      is(gDebugger.gThreadClient.state, "paused",
         "The debugger statement was hit (4).");
      is(getSelectedSourceURL(gSources), SOURCE_URL,
         "The currently shown source is incorrect (4).");
      ok(isCaretPos(gPanel, 6),
         "The source editor caret position is incorrect (4).");

      yield promise.all([
        reload(gPanel),
        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN)
      ]);

      isnot(gDebugger.gThreadClient.state, "paused",
            "The breakpoint wasn't hit yet (5).");
      is(getSelectedSourceURL(gSources), SOURCE_URL,
         "The currently shown source is incorrect (5).");
      ok(isCaretPos(gPanel, 1),
         "The source editor caret position is incorrect (5).");

      paused = waitForPause(gDebugger.gThreadClient);
      clickButtonAndPause();
      packet = yield paused;
      is(packet.why.type, "breakpoint",
         "Execution has advanced to the breakpoint.");
      isnot(packet.why.type, "debuggerStatement",
            "The breakpoint was hit before the debugger statement.");
      yield ensureCaretAt(gPanel, 5, 1, true);

      is(gDebugger.gThreadClient.state, "paused",
         "The breakpoint was hit (6).");
      is(getSelectedSourceURL(gSources), SOURCE_URL,
         "The currently shown source is incorrect (6).");
      ok(isCaretPos(gPanel, 5),
         "The source editor caret position is incorrect (6).");

      paused = waitForPause(gDebugger.gThreadClient);
      gDebugger.gThreadClient.resume();
      packet = yield paused;

      is(packet.why.type, "debuggerStatement",
         "Execution has advanced to the next line.");
      isnot(packet.why.type, "breakpoint",
            "No ghost breakpoint was hit.");

      yield ensureCaretAt(gPanel, 6, 1, true)

      is(gDebugger.gThreadClient.state, "paused",
         "The debugger statement was hit (7).");
      is(getSelectedSourceURL(gSources), SOURCE_URL,
         "The currently shown source is incorrect (7).");
      ok(isCaretPos(gPanel, 6),
         "The source editor caret position is incorrect (7).");

      let sourceShown = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN)
      // Click the second source in the list.
      yield actions.selectSource(getSourceForm(gSources, TAB_URL));
      yield sourceShown;
      is(gEditor.getText().indexOf("debugger"), 447,
         "The correct source is shown in the source editor.")
      is(gEditor.getBreakpoints().length, 0,
         "No breakpoints should be shown for the second source.");
      yield ensureCaretAt(gPanel, 1, 1, true);

      sourceShown = waitForDebuggerEvents(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
      yield actions.selectSource(getSourceForm(gSources, SOURCE_URL));
      yield sourceShown;
      is(gEditor.getText().indexOf("debugger"), 148,
         "The correct source is shown in the source editor.")
      is(gEditor.getBreakpoints().length, 1,
         "One breakpoint should be shown for the first source.");

      //yield waitForTime(2000);
      yield ensureCaretAt(gPanel, 1, 1, true);

      //yield waitForTime(50000);
      resumeDebuggerThenCloseAndFinish(gPanel);
    });

    callInTab(gTab, "runDebuggerStatement");
  });
}
