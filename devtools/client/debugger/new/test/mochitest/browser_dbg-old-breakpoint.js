/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we show a breakpoint in the UI when there is an old pending
// breakpoint with an invalid original location.
add_task(async function() {
  clearDebuggerPreferences();

  const pending = {
    bp1: {
      location: {
        sourceId: "",
        sourceUrl: EXAMPLE_URL + "nowhere2.js",
        line: 5
      },
      generatedLocation: {
        sourceUrl: EXAMPLE_URL + "simple1.js",
        line: 4
      },
      options: {},
      disabled: false
    },
    bp2: {
      location: {
        sourceId: "",
        sourceUrl: EXAMPLE_URL + "nowhere.js",
        line: 5
      },
      generatedLocation: {
        sourceUrl: EXAMPLE_URL + "simple3.js",
        line: 2
      },
      options: {},
      disabled: false
    },
  };
  asyncStorage.setItem("debugger.pending-breakpoints", pending);

  const toolbox = await openNewTabAndToolbox(EXAMPLE_URL + "doc-scripts.html", "jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  // Pending breakpoints are installed asynchronously, keep invoking the entry
  // function until the debugger pauses.
  await waitUntil(() => {
    invokeInTab("main");
    return isPaused(dbg);
  });

  ok(true, "paused at unmapped breakpoint");
  await waitForState(dbg, state => dbg.selectors.getBreakpointCount(state) == 2);
  ok(true, "unmapped breakpoints shown in UI");
});

// Test that if we show a breakpoint with an old generated location, it is
// removed after we load the original source and find the new generated
// location.
add_task(async function() {
  clearDebuggerPreferences();

  const pending = {
    bp1: {
      location: {
        sourceId: "",
        sourceUrl: "webpack:///entry.js",
        line: 15,
        column: 0
      },
      generatedLocation: {
        sourceUrl: EXAMPLE_URL + "sourcemaps/bundle.js",
        line: 47,
        column: 16
      },
      astLocation: {},
      options: {},
      disabled: false
    },
  };
  asyncStorage.setItem("debugger.pending-breakpoints", pending);

  const toolbox = await openNewTabAndToolbox(EXAMPLE_URL + "doc-sourcemaps.html", "jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  await waitForState(dbg, state => {
    const bps = dbg.selectors.getBreakpointsList(state);
    return bps.length == 1
        && bps[0].location.sourceUrl.includes("entry.js")
        && bps[0].location.line == 15;
  });
  ok(true, "removed old breakpoint during sync");
});
