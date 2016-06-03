/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if the context menu associated with each breakpoint does what it should.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  Task.spawn(function* () {
    const options = {
      source: EXAMPLE_URL + "code_script-switching-01.js",
      line: 1
    };
    const [gTab,, gPanel ] = yield initDebugger(TAB_URL, options);
    const gDebugger = gPanel.panelWin;
    const gSources = gDebugger.DebuggerView.Sources;
    const queries = gDebugger.require("./content/queries");
    const actions = bindActionCreators(gPanel);
    const getState = gDebugger.DebuggerController.getState;

    const addBreakpoints = Task.async(function* () {
      yield actions.addBreakpoint({ actor: gSources.values[0], line: 5 });
      yield actions.addBreakpoint({ actor: gSources.values[1], line: 6 });
      yield actions.addBreakpoint({ actor: gSources.values[1], line: 7 });
      yield actions.addBreakpoint({ actor: gSources.values[1], line: 8 });
      yield actions.addBreakpoint({ actor: gSources.values[1], line: 9 });
      yield ensureThreadClientState(gPanel, "resumed");
      gSources.highlightBreakpoint({ actor: gSources.values[1], line: 9 });
    });

    const pauseAndCheck = Task.async(function* () {
      let source = queries.getSelectedSource(getState());
      is(source.url, EXAMPLE_URL + "code_script-switching-02.js",
         "The currently selected source is incorrect (1).");
      is(gSources.selectedIndex, 1,
         "The currently selected source is incorrect (2).");
      ok(isCaretPos(gPanel, 9),
         "The editor location is correct before pausing.");

      generateMouseClickInTab(gTab, "content.document.querySelector('button')");

      return waitForSourceAndCaretAndScopes(gPanel, "-01.js", 5).then(() => {
        let source = queries.getSelectedSource(getState());
        is(source.url, EXAMPLE_URL + "code_script-switching-01.js",
           "The currently selected source is incorrect (3).");
        is(gSources.selectedIndex, 0,
           "The currently selected source is incorrect (4).");
        ok(isCaretPos(gPanel, 5),
           "The editor location is correct after pausing.");
      });
    });

    let initialChecks = Task.async(function* () {
      for (let bp of queries.getBreakpoints(getState())) {
        ok(bp.actor, "All breakpoint items should have an actor");
        ok(!bp.disabled, "All breakpoints should initially be enabled.");

        let prefix = "bp-cMenu-"; // "breakpoints context menu"
        let identifier = queries.makeLocationId(bp.location);
        let enableSelfId = prefix + "enableSelf-" + identifier + "-menuitem";
        let disableSelfId = prefix + "disableSelf-" + identifier + "-menuitem";

        // Check to make sure that only the bp context menu is shown when right clicking
        // this node (Bug 1159276).
        let breakpointItem = gSources._getBreakpoint(bp);
        let menu = gDebugger.document.getElementById("bp-mPop-" + identifier);
        let contextMenuShown = once(gDebugger.document, "popupshown");
        EventUtils.synthesizeMouseAtCenter(breakpointItem.prebuiltNode, {type: "contextmenu", button: 2}, gDebugger);
        let event = yield contextMenuShown;
        is(event.originalTarget.id, menu.id, "The correct context menu was shown");
        let contextMenuHidden = once(gDebugger.document, "popuphidden");
        menu.hidePopup();
        yield contextMenuHidden;

        is(gDebugger.document.getElementById(enableSelfId).getAttribute("hidden"), "true",
           "The 'Enable breakpoint' context menu item should initially be hidden'.");
        ok(!gDebugger.document.getElementById(disableSelfId).hasAttribute("hidden"),
           "The 'Disable breakpoint' context menu item should initially not be hidden'.");

        is(breakpointItem.attachment.view.checkbox.getAttribute("checked"), "true",
           "All breakpoints should initially have a checked checkbox.");
      }
    });

    const checkBreakpointToggleSelf = Task.async(function* (index) {
      EventUtils.sendMouseEvent({ type: "click" },
                                gDebugger.document.querySelectorAll(".dbg-breakpoint")[index],
                                gDebugger);

      let selectedBreakpoint = gSources._selectedBreakpoint;
      let selectedBreakpointItem = gSources._getBreakpoint(selectedBreakpoint);

      ok(selectedBreakpoint.actor,
         "Selected breakpoint should have an actor.");
      ok(!selectedBreakpoint.disabled,
         "The breakpoint should not be disabled yet (" + index + ").");

      let prefix = "bp-cMenu-"; // "breakpoints context menu"
      let identifier = queries.makeLocationId(selectedBreakpoint.location);
      let enableSelfId = prefix + "enableSelf-" + identifier + "-menuitem";
      let disableSelfId = prefix + "disableSelf-" + identifier + "-menuitem";

      is(gDebugger.document.getElementById(enableSelfId).getAttribute("hidden"), "true",
         "The 'Enable breakpoint' context menu item should be hidden'.");
      ok(!gDebugger.document.getElementById(disableSelfId).hasAttribute("hidden"),
         "The 'Disable breakpoint' context menu item should not be hidden'.");

      ok(isCaretPos(gPanel, selectedBreakpoint.location.line),
         "The source editor caret position was incorrect (" + index + ").");

      // Test disabling this breakpoint.
      gSources._onDisableSelf(selectedBreakpoint.location);
      yield waitForDispatch(gPanel, gDebugger.constants.REMOVE_BREAKPOINT);

      ok(!!queries.getBreakpoint(getState(), selectedBreakpoint.location).disabled,
         "The breakpoint should be disabled.");

      ok(!gDebugger.document.getElementById(enableSelfId).hasAttribute("hidden"),
         "The 'Enable breakpoint' context menu item should not be hidden'.");
      is(gDebugger.document.getElementById(disableSelfId).getAttribute("hidden"), "true",
         "The 'Disable breakpoint' context menu item should be hidden'.");
      ok(!selectedBreakpointItem.attachment.view.checkbox.hasAttribute("checked"),
         "The breakpoint should now be unchecked.");

      gSources._onEnableSelf(selectedBreakpoint.location);
      yield waitForDispatch(gPanel, gDebugger.constants.ADD_BREAKPOINT);

      ok(!queries.getBreakpoint(getState(), selectedBreakpoint.location).disabled,
         "The breakpoint should be enabled.");
      is(gDebugger.document.getElementById(enableSelfId).getAttribute("hidden"), "true",
         "The 'Enable breakpoint' context menu item should be hidden'.");
      ok(!gDebugger.document.getElementById(disableSelfId).hasAttribute("hidden"),
         "The 'Disable breakpoint' context menu item should not be hidden'.");
      ok(selectedBreakpointItem.attachment.view.checkbox.hasAttribute("checked"),
         "The breakpoint should now be checked.");
    });

    const checkBreakpointToggleOthers = Task.async(function* (index) {
      EventUtils.sendMouseEvent(
        { type: "click" },
        gDebugger.document.querySelectorAll(".dbg-breakpoint")[index],
        gDebugger
      );

      // Test disabling other breakpoints.
      disableOthers();
      yield waitForDispatch(gPanel, gDebugger.constants.REMOVE_BREAKPOINT, 4);

      let selectedBreakpoint = queries.getBreakpoint(getState(), gSources._selectedBreakpoint.location);

      ok(selectedBreakpoint.actor,
         "There should be a breakpoint actor.");
      ok(!selectedBreakpoint.disabled,
         "The targetted breakpoint should not have been disabled (" + index + ").");

      for (let bp of queries.getBreakpoints(getState())) {
        if (bp !== selectedBreakpoint) {
          ok(bp.disabled,
             "Non-targetted breakpoints should have been disabled.");
        }
      }

      // Test re-enabling other breakpoints.
      enableOthers();
      yield waitForDispatch(gPanel, gDebugger.constants.ADD_BREAKPOINT, 4);
      for (let bp of queries.getBreakpoints(getState())) {
        ok(!bp.disabled, "All breakpoints should be enabled.");
      }

      // Test disabling all breakpoints.
      disableAll();
      yield waitForDispatch(gPanel, gDebugger.constants.REMOVE_BREAKPOINT, 5);
      for (let bp of queries.getBreakpoints(getState())) {
        ok(!!bp.disabled, "All breakpoints should be disabled.");
      }

      // // Test re-enabling all breakpoints.
      enableAll();
      yield waitForDispatch(gPanel, gDebugger.constants.ADD_BREAKPOINT, 5);
      for (let bp of queries.getBreakpoints(getState())) {
        ok(!bp.disabled, "All breakpoints should be enabled.");
      }
    });

    const testDeleteAll = Task.async(function* () {
      // Test deleting all breakpoints.
      deleteAll();
      yield waitForDispatch(gPanel, gDebugger.constants.REMOVE_BREAKPOINT, 5);

      ok(!gSources._selectedBreakpoint,
         "There should be no breakpoint available after removing all breakpoints.");

      for (let bp of queries.getBreakpoints(getState())) {
        ok(false, "It's a trap!");
      }
    });

    function disableOthers() {
      gSources._onDisableOthers(gSources._selectedBreakpoint.location);
    }
    function enableOthers() {
      gSources._onEnableOthers(gSources._selectedBreakpoint.location);
    }
    function disableAll() {
      gSources._onDisableAll();
    }
    function enableAll() {
      gSources._onEnableAll();
    }
    function deleteAll() {
      gSources._onDeleteAll();
    }

    yield addBreakpoints();
    yield initialChecks();
    yield checkBreakpointToggleSelf(0);
    yield checkBreakpointToggleOthers(0);
    yield checkBreakpointToggleSelf(1);
    yield checkBreakpointToggleOthers(1);
    yield checkBreakpointToggleSelf(2);
    yield checkBreakpointToggleOthers(2);
    yield checkBreakpointToggleSelf(3);
    yield checkBreakpointToggleOthers(3);
    yield checkBreakpointToggleSelf(4);
    yield checkBreakpointToggleOthers(4);
    yield testDeleteAll();

    yield addBreakpoints();
    yield initialChecks();
    yield pauseAndCheck();
    yield checkBreakpointToggleSelf(0);
    yield checkBreakpointToggleOthers(0);
    yield checkBreakpointToggleSelf(1);
    yield checkBreakpointToggleOthers(1);
    yield checkBreakpointToggleSelf(2);
    yield checkBreakpointToggleOthers(2);
    yield checkBreakpointToggleSelf(3);
    yield checkBreakpointToggleOthers(3);
    yield checkBreakpointToggleSelf(4);
    yield checkBreakpointToggleOthers(4);
    yield testDeleteAll();

    resumeDebuggerThenCloseAndFinish(gPanel);
  });
}
