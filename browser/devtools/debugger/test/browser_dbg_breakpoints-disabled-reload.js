/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that disabled breakpoints survive target navigation.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    let gDebugger = aPanel.panelWin;
    let gEvents = gDebugger.EVENTS;
    let gEditor = gDebugger.DebuggerView.editor;
    let gSources = gDebugger.DebuggerView.Sources;
    let gBreakpoints = gDebugger.DebuggerController.Breakpoints;
    let gBreakpointLocation = { url: EXAMPLE_URL + "code_script-switching-01.js", line: 5 };

    Task.spawn(function() {
      yield waitForSourceShown(aPanel, "-01.js");
      yield aPanel.addBreakpoint(gBreakpointLocation);

      yield ensureThreadClientState(aPanel, "resumed");
      yield testWhenBreakpointEnabledAndFirstSourceShown();

      yield reloadActiveTab(aPanel, gEvents.SOURCE_SHOWN);
      yield testWhenBreakpointEnabledAndSecondSourceShown();

      yield gSources.disableBreakpoint(gBreakpointLocation);
      yield reloadActiveTab(aPanel, gEvents.SOURCE_SHOWN);
      yield testWhenBreakpointDisabledAndSecondSourceShown();

      yield gSources.enableBreakpoint(gBreakpointLocation);
      yield reloadActiveTab(aPanel, gEvents.SOURCE_SHOWN);
      yield testWhenBreakpointEnabledAndSecondSourceShown();

      yield resumeDebuggerThenCloseAndFinish(aPanel);
    });

    function verifyView({ disabled, visible }) {
      return Task.spawn(function() {
        // It takes a tick for the checkbox in the SideMenuWidget and the
        // gutter in the SourceEditor to get updated.
        yield waitForTick();

        let breakpointItem = gSources.getBreakpoint(gBreakpointLocation);
        let visibleBreakpoints = gEditor.getBreakpoints();
        is(!!breakpointItem.attachment.disabled, disabled,
          "The selected brekapoint state was correct.");
        is(breakpointItem.attachment.view.checkbox.hasAttribute("checked"), !disabled,
          "The selected brekapoint's checkbox state was correct.");

        // Source editor starts counting line and column numbers from 0.
        let breakpointLine = breakpointItem.attachment.line - 1;
        let matchedBreakpoints = visibleBreakpoints.filter(e => e.line == breakpointLine);
        is(!!matchedBreakpoints.length, visible,
          "The selected breakpoint's visibility in the editor was correct.");
      });
    }

    // All the following executeSoon()'s are required to spin the event loop
    // before causing the debuggee to pause, to allow functions to yield first.

    function testWhenBreakpointEnabledAndFirstSourceShown() {
      return Task.spawn(function() {
        yield ensureSourceIs(aPanel, "-01.js");
        yield verifyView({ disabled: false, visible: true });

        executeSoon(() => aDebuggee.firstCall());
        yield waitForDebuggerEvents(aPanel, gEvents.FETCHED_SCOPES);
        yield ensureSourceIs(aPanel, "-01.js");
        yield ensureCaretAt(aPanel, 5);
        yield verifyView({ disabled: false, visible: true });

        executeSoon(() => gDebugger.gThreadClient.resume());
        yield waitForSourceAndCaretAndScopes(aPanel, "-02.js", 6);
        yield verifyView({ disabled: false, visible: false });
      });
    }

    function testWhenBreakpointEnabledAndSecondSourceShown() {
      return Task.spawn(function() {
        yield ensureSourceIs(aPanel, "-02.js", true);
        yield verifyView({ disabled: false, visible: false });

        executeSoon(() => aDebuggee.firstCall());
        yield waitForSourceAndCaretAndScopes(aPanel, "-01.js", 5);
        yield verifyView({ disabled: false, visible: true });

        executeSoon(() => gDebugger.gThreadClient.resume());
        yield waitForSourceAndCaretAndScopes(aPanel, "-02.js", 6);
        yield verifyView({ disabled: false, visible: false });
      });
    }

    function testWhenBreakpointDisabledAndSecondSourceShown() {
      return Task.spawn(function() {
        yield ensureSourceIs(aPanel, "-02.js", true);
        yield verifyView({ disabled: true, visible: false });

        executeSoon(() => aDebuggee.firstCall());
        yield waitForDebuggerEvents(aPanel, gEvents.FETCHED_SCOPES);
        yield ensureSourceIs(aPanel, "-02.js");
        yield ensureCaretAt(aPanel, 6);
        yield verifyView({ disabled: true, visible: false });

        executeSoon(() => gDebugger.gThreadClient.resume());
        yield waitForDebuggerEvents(aPanel, gEvents.AFTER_FRAMES_CLEARED);
        yield ensureSourceIs(aPanel, "-02.js");
        yield ensureCaretAt(aPanel, 6);
        yield verifyView({ disabled: true, visible: false });
      });
    }
  });
}
