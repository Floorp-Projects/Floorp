/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that disabled breakpoints survive target navigation.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  let options = {
    source: "-01.js",
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    const gPanel = aPanel;
    const gTab = aTab;
    const gDebugger = gPanel.panelWin;
    const gEvents = gDebugger.EVENTS;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require("./content/queries");
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;
    let gBreakpointLocation;

    Task.spawn(function* () {
      gBreakpointLocation = { actor: getSourceActor(gSources, EXAMPLE_URL + "code_script-switching-01.js"),
                              line: 5 };

      yield actions.addBreakpoint(gBreakpointLocation);

      yield ensureThreadClientState(gPanel, "resumed");
      yield testWhenBreakpointEnabledAndFirstSourceShown();

      gSources._preferredSourceURL = EXAMPLE_URL + "code_script-switching-02.js";
      yield reloadActiveTab(gPanel, gEvents.SOURCE_SHOWN);
      yield testWhenBreakpointEnabledAndSecondSourceShown();

      yield actions.disableBreakpoint(gBreakpointLocation);
      yield reloadActiveTab(gPanel, gEvents.SOURCE_SHOWN);

      yield testWhenBreakpointDisabledAndSecondSourceShown();

      yield actions.enableBreakpoint(gBreakpointLocation);
      yield reloadActiveTab(gPanel, gEvents.SOURCE_SHOWN);
      yield testWhenBreakpointEnabledAndSecondSourceShown();

      yield resumeDebuggerThenCloseAndFinish(gPanel);
    });

    function verifyView({ disabled }) {
      return Task.spawn(function* () {
        // It takes a tick for the checkbox in the SideMenuWidget and the
        // gutter in the editor to get updated.
        yield waitForTick();

        let breakpoint = queries.getBreakpoint(getState(), gBreakpointLocation);
        let breakpointItem = gSources._getBreakpoint(breakpoint);
        is(!!breakpoint.disabled, disabled,
           "The selected breakpoint state was correct.");

        is(breakpointItem.attachment.view.checkbox.hasAttribute("checked"), !disabled,
          "The selected breakpoint's checkbox state was correct.");
      });
    }

    // All the following executeSoon()'s are required to spin the event loop
    // before causing the debuggee to pause, to allow functions to yield first.

    function testWhenBreakpointEnabledAndFirstSourceShown() {
      return Task.spawn(function* () {
        yield ensureSourceIs(gPanel, "-01.js");
        yield verifyView({ disabled: false });

        callInTab(gTab, "firstCall");
        yield waitForDebuggerEvents(gPanel, gEvents.FETCHED_SCOPES);
        yield ensureSourceIs(gPanel, "-01.js");
        yield ensureCaretAt(gPanel, 5);
        yield verifyView({ disabled: false });

        executeSoon(() => gDebugger.gThreadClient.resume());
        yield waitForSourceAndCaretAndScopes(gPanel, "-02.js", 6);
        yield verifyView({ disabled: false });
      });
    }

    function testWhenBreakpointEnabledAndSecondSourceShown() {
      return Task.spawn(function* () {
        yield ensureSourceIs(gPanel, "-02.js", true);
        yield verifyView({ disabled: false });

        callInTab(gTab, "firstCall");
        yield waitForSourceAndCaretAndScopes(gPanel, "-01.js", 1);
        yield verifyView({ disabled: false });

        executeSoon(() => gDebugger.gThreadClient.resume());
        yield waitForSourceAndCaretAndScopes(gPanel, "-02.js", 6);
        yield verifyView({ disabled: false });
      });
    }

    function testWhenBreakpointDisabledAndSecondSourceShown() {
      return Task.spawn(function* () {
        yield ensureSourceIs(gPanel, "-02.js", true);
        yield verifyView({ disabled: true });

        callInTab(gTab, "firstCall");
        yield waitForDebuggerEvents(gPanel, gEvents.FETCHED_SCOPES);
        yield ensureSourceIs(gPanel, "-02.js");
        yield ensureCaretAt(gPanel, 6);
        yield verifyView({ disabled: true });

        executeSoon(() => gDebugger.gThreadClient.resume());
        yield waitForDebuggerEvents(gPanel, gEvents.AFTER_FRAMES_CLEARED);
        yield ensureSourceIs(gPanel, "-02.js");
        yield ensureCaretAt(gPanel, 6);
        yield verifyView({ disabled: true });
      });
    }
  });
}
