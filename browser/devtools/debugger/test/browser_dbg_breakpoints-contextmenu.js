/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if the context menu associated with each breakpoint does what it should.
 */

const TAB_URL = EXAMPLE_URL + "doc_script-switching-01.html";

function test() {
  // Debug test slaves are a bit slow at this test.
  requestLongerTimeout(2);

  let gTab, gDebuggee, gPanel, gDebugger;
  let gEditor, gSources, gBreakpoints;

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gBreakpoints = gDebugger.DebuggerController.Breakpoints;

    waitForSourceShown(gPanel, "-01.js")
      .then(performTestWhileNotPaused)
      .then(performTestWhilePaused)
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });

  function addBreakpoints() {
    return promise.resolve(null)
      .then(() => gPanel.addBreakpoint({ url: gSources.values[0], line: 5 }))
      .then(() => gPanel.addBreakpoint({ url: gSources.values[1], line: 6 }))
      .then(() => gPanel.addBreakpoint({ url: gSources.values[1], line: 7 }))
      .then(() => gPanel.addBreakpoint({ url: gSources.values[1], line: 8 }))
      .then(() => gPanel.addBreakpoint({ url: gSources.values[1], line: 9 }))
      .then(() => ensureThreadClientState(gPanel, "resumed"));
  }

  function performTestWhileNotPaused() {
    info("Performing test while not paused...");

    return addBreakpoints()
      .then(initialChecks)
      .then(() => checkBreakpointToggleSelf(0))
      .then(() => checkBreakpointToggleOthers(0))
      .then(() => checkBreakpointToggleSelf(1))
      .then(() => checkBreakpointToggleOthers(1))
      .then(() => checkBreakpointToggleSelf(2))
      .then(() => checkBreakpointToggleOthers(2))
      .then(() => checkBreakpointToggleSelf(3))
      .then(() => checkBreakpointToggleOthers(3))
      .then(() => checkBreakpointToggleSelf(4))
      .then(() => checkBreakpointToggleOthers(4))
      .then(testDeleteAll);
  }

  function performTestWhilePaused() {
    info("Performing test while paused...");

    return addBreakpoints()
      .then(initialChecks)
      .then(pauseAndCheck)
      .then(() => checkBreakpointToggleSelf(0))
      .then(() => checkBreakpointToggleOthers(0))
      .then(() => checkBreakpointToggleSelf(1))
      .then(() => checkBreakpointToggleOthers(1))
      .then(() => checkBreakpointToggleSelf(2))
      .then(() => checkBreakpointToggleOthers(2))
      .then(() => checkBreakpointToggleSelf(3))
      .then(() => checkBreakpointToggleOthers(3))
      .then(() => checkBreakpointToggleSelf(4))
      .then(() => checkBreakpointToggleOthers(4))
      .then(testDeleteAll);
  }

  function pauseAndCheck() {
    let finished = waitForSourceAndCaretAndScopes(gPanel, "-01.js", 5).then(() => {
      is(gSources.selectedLabel, "code_script-switching-01.js",
        "The currently selected source is incorrect (3).");
      is(gSources.selectedIndex, 0,
        "The currently selected source is incorrect (4).");
      ok(isCaretPos(gPanel, 5),
        "The editor location is correct after pausing.");
    });

    is(gSources.selectedLabel, "code_script-switching-02.js",
      "The currently selected source is incorrect (1).");
    is(gSources.selectedIndex, 1,
      "The currently selected source is incorrect (2).");
    ok(isCaretPos(gPanel, 9),
      "The editor location is correct before pausing.");

    // Spin the event loop before causing the debuggee to pause, to allow
    // this function to return first.
    executeSoon(() => {
      EventUtils.sendMouseEvent({ type: "click" },
        gDebuggee.document.querySelector("button"),
        gDebuggee);
    });

    return finished;
  }

  function initialChecks() {
    for (let source in gSources) {
      for (let breakpoint in source) {
        ok(gBreakpoints._getAdded(breakpoint.attachment),
          "All breakpoint items should have corresponding promises (1).");
        ok(!gBreakpoints._getRemoving(breakpoint.attachment),
          "All breakpoint items should have corresponding promises (2).");
        ok(breakpoint.attachment.actor,
          "All breakpoint items should have corresponding promises (3).");
        is(!!breakpoint.attachment.disabled, false,
          "All breakpoints should initially be enabled.");

        let prefix = "bp-cMenu-"; // "breakpoints context menu"
        let identifier = gBreakpoints.getIdentifier(breakpoint.attachment);
        let enableSelfId = prefix + "enableSelf-" + identifier + "-menuitem";
        let disableSelfId = prefix + "disableSelf-" + identifier + "-menuitem";

        is(gDebugger.document.getElementById(enableSelfId).getAttribute("hidden"), "true",
          "The 'Enable breakpoint' context menu item should initially be hidden'.");
        ok(!gDebugger.document.getElementById(disableSelfId).hasAttribute("hidden"),
          "The 'Disable breakpoint' context menu item should initially not be hidden'.");
        is(breakpoint.attachment.view.checkbox.getAttribute("checked"), "true",
          "All breakpoints should initially have a checked checkbox.");
      }
    }
  }

  function checkBreakpointToggleSelf(aIndex) {
    let deferred = promise.defer();

    EventUtils.sendMouseEvent({ type: "click" },
      gDebugger.document.querySelectorAll(".dbg-breakpoint")[aIndex],
      gDebugger);

    let selectedBreakpoint = gSources._selectedBreakpointItem;

    ok(gBreakpoints._getAdded(selectedBreakpoint.attachment),
      "There should be a breakpoint client available (1).");
    ok(!gBreakpoints._getRemoving(selectedBreakpoint.attachment),
      "There should be a breakpoint client available (2).");
    ok(selectedBreakpoint.attachment.actor,
      "There should be a breakpoint client available (3).");
    is(!!selectedBreakpoint.attachment.disabled, false,
      "The breakpoint should not be disabled yet (" + aIndex + ").");

    gBreakpoints._getAdded(selectedBreakpoint.attachment).then(aBreakpointClient => {
      ok(aBreakpointClient,
        "There should be a breakpoint client available as a promise.");
    });

    let prefix = "bp-cMenu-"; // "breakpoints context menu"
    let identifier = gBreakpoints.getIdentifier(selectedBreakpoint.attachment);
    let enableSelfId = prefix + "enableSelf-" + identifier + "-menuitem";
    let disableSelfId = prefix + "disableSelf-" + identifier + "-menuitem";

    is(gDebugger.document.getElementById(enableSelfId).getAttribute("hidden"), "true",
      "The 'Enable breakpoint' context menu item should be hidden'.");
    ok(!gDebugger.document.getElementById(disableSelfId).hasAttribute("hidden"),
      "The 'Disable breakpoint' context menu item should not be hidden'.");

    ok(isCaretPos(gPanel, selectedBreakpoint.attachment.line),
      "The source editor caret position was incorrect (" + aIndex + ").");

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_REMOVED).then(() => {
      ok(!gBreakpoints._getAdded(selectedBreakpoint.attachment),
        "There should be no breakpoint client available (4).");

      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_ADDED).then(() => {
        ok(gBreakpoints._getAdded(selectedBreakpoint.attachment),
          "There should be a breakpoint client available (5).");

        deferred.resolve();
      });

      // Test re-disabling this breakpoint.
      gSources._onEnableSelf(selectedBreakpoint.attachment);
      is(selectedBreakpoint.attachment.disabled, false,
        "The current breakpoint should now be enabled.")

      is(gDebugger.document.getElementById(enableSelfId).getAttribute("hidden"), "true",
        "The 'Enable breakpoint' context menu item should be hidden'.");
      ok(!gDebugger.document.getElementById(disableSelfId).hasAttribute("hidden"),
        "The 'Disable breakpoint' context menu item should not be hidden'.");
      ok(selectedBreakpoint.attachment.view.checkbox.hasAttribute("checked"),
        "The breakpoint should now be checked.");
    });

    // Test disabling this breakpoint.
    gSources._onDisableSelf(selectedBreakpoint.attachment);
    is(selectedBreakpoint.attachment.disabled, true,
      "The current breakpoint should now be disabled.")

    ok(!gDebugger.document.getElementById(enableSelfId).hasAttribute("hidden"),
      "The 'Enable breakpoint' context menu item should not be hidden'.");
    is(gDebugger.document.getElementById(disableSelfId).getAttribute("hidden"), "true",
      "The 'Disable breakpoint' context menu item should be hidden'.");
    ok(!selectedBreakpoint.attachment.view.checkbox.hasAttribute("checked"),
      "The breakpoint should now be unchecked.");

    return deferred.promise;
  }

  function checkBreakpointToggleOthers(aIndex) {
    let deferred = promise.defer();

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_REMOVED, 4).then(() => {
      let selectedBreakpoint = gSources._selectedBreakpointItem;

      ok(gBreakpoints._getAdded(selectedBreakpoint.attachment),
        "There should be a breakpoint client available (6).");
      ok(!gBreakpoints._getRemoving(selectedBreakpoint.attachment),
        "There should be a breakpoint client available (7).");
      ok(selectedBreakpoint.attachment.actor,
        "There should be a breakpoint client available (8).");
      is(!!selectedBreakpoint.attachment.disabled, false,
        "The targetted breakpoint should not have been disabled (" + aIndex + ").");

      for (let source in gSources) {
        for (let otherBreakpoint in source) {
          if (otherBreakpoint != selectedBreakpoint) {
            ok(!gBreakpoints._getAdded(otherBreakpoint.attachment),
              "There should be no breakpoint client for a disabled breakpoint (9).");
            is(otherBreakpoint.attachment.disabled, true,
              "Non-targetted breakpoints should have been disabled (10).");
          }
        }
      }

      waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_ADDED, 4).then(() => {
        for (let source in gSources) {
          for (let someBreakpoint in source) {
            ok(gBreakpoints._getAdded(someBreakpoint.attachment),
              "There should be a breakpoint client for all enabled breakpoints (11).");
            is(someBreakpoint.attachment.disabled, false,
              "All breakpoints should now have been enabled (12).");
          }
        }

        waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_REMOVED, 5).then(() => {
          for (let source in gSources) {
            for (let someBreakpoint in source) {
              ok(!gBreakpoints._getAdded(someBreakpoint.attachment),
                "There should be no breakpoint client for a disabled breakpoint (13).");
              is(someBreakpoint.attachment.disabled, true,
                "All breakpoints should now have been disabled (14).");
            }
          }

          waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_ADDED, 5).then(() => {
            for (let source in gSources) {
              for (let someBreakpoint in source) {
                ok(gBreakpoints._getAdded(someBreakpoint.attachment),
                  "There should be a breakpoint client for all enabled breakpoints (15).");
                is(someBreakpoint.attachment.disabled, false,
                  "All breakpoints should now have been enabled (16).");
              }
            }

            // Done.
            deferred.resolve();
          });

          // Test re-enabling all breakpoints.
          enableAll();
        });

        // Test disabling all breakpoints.
        disableAll();
      });

      // Test re-enabling other breakpoints.
      enableOthers();
    });

    // Test disabling other breakpoints.
    disableOthers();

    return deferred.promise;
  }

  function testDeleteAll() {
    let deferred = promise.defer();

    waitForDebuggerEvents(gPanel, gDebugger.EVENTS.BREAKPOINT_REMOVED, 5).then(() => {
      ok(!gSources._selectedBreakpointItem,
        "There should be no breakpoint available after removing all breakpoints.");

      for (let source in gSources) {
        for (let otherBreakpoint in source) {
          ok(false, "It's a trap!");
        }
      }

      // Done.
      deferred.resolve()
    });

    // Test deleting all breakpoints.
    deleteAll();

    return deferred.promise;
  }

  function disableOthers() {
    gSources._onDisableOthers(gSources._selectedBreakpointItem.attachment);
  }
  function enableOthers() {
    gSources._onEnableOthers(gSources._selectedBreakpointItem.attachment);
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
}
