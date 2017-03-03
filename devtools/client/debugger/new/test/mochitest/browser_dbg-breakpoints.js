/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function toggleBreakpoint(dbg, index) {
  const bp = findElement(dbg, "breakpointItem", index);
  const input = bp.querySelector("input");
  input.click();
}

function removeBreakpoint(dbg, index) {
  return Task.spawn(function* () {
    const bp = findElement(dbg, "breakpointItem", index);
    bp.querySelector(".close-btn").click();
    yield waitForDispatch(dbg, "REMOVE_BREAKPOINT");
  });
}

function disableBreakpoint(dbg, index) {
  return Task.spawn(function* () {
    toggleBreakpoint(dbg, index);
    yield waitForDispatch(dbg, "REMOVE_BREAKPOINT");
  });
}

function enableBreakpoint(dbg, index) {
  return Task.spawn(function* () {
    toggleBreakpoint(dbg, index);
    yield waitForDispatch(dbg, "ADD_BREAKPOINT");
  });
}

function toggleBreakpoints(dbg) {
  return Task.spawn(function* () {
    clickElement(dbg, "toggleBreakpoints");
    yield waitForDispatch(dbg, "TOGGLE_BREAKPOINTS");
  });
}

function findBreakpoint(dbg, url, line) {
  const { selectors: { getBreakpoint }, getState } = dbg;
  const source = findSource(dbg, url);
  return getBreakpoint(getState(), { sourceId: source.id, line });
}

function findBreakpoints(dbg) {
  const { selectors: { getBreakpoints }, getState } = dbg;
  return getBreakpoints(getState());
}

add_task(function* () {
  const dbg = yield initDebugger("doc-scripts.html");

  // Create two breakpoints
  yield selectSource(dbg, "simple2");
  yield addBreakpoint(dbg, "simple2", 3);
  yield addBreakpoint(dbg, "simple2", 5);

  // Disable the first one
  yield disableBreakpoint(dbg, 1);
  let bp1 = findBreakpoint(dbg, "simple2", 3);
  let bp2 = findBreakpoint(dbg, "simple2", 5);
  is(bp1.disabled, true, "first breakpoint is disabled");
  is(bp2.disabled, false, "second breakpoint is enabled");

  // Disable and Re-Enable the second one
  yield disableBreakpoint(dbg, 2);
  yield enableBreakpoint(dbg, 2);
  bp2 = findBreakpoint(dbg, "simple2", 5);
  is(bp2.disabled, false, "second breakpoint is enabled");
});

// toggle all
add_task(function* () {
  const dbg = yield initDebugger("doc-scripts.html");

  // Create two breakpoints
  yield selectSource(dbg, "simple2");
  yield addBreakpoint(dbg, "simple2", 3);
  yield addBreakpoint(dbg, "simple2", 5);

  // Disable all of the breakpoints
  yield toggleBreakpoints(dbg);
  let bp1 = findBreakpoint(dbg, "simple2", 3);
  let bp2 = findBreakpoint(dbg, "simple2", 5);
  is(bp1.disabled, true, "first breakpoint is disabled");
  is(bp2.disabled, true, "second breakpoint is disabled");

  // Enable all of the breakpoints
  yield toggleBreakpoints(dbg);
  bp1 = findBreakpoint(dbg, "simple2", 3);
  bp2 = findBreakpoint(dbg, "simple2", 5);
  is(bp1.disabled, false, "first breakpoint is enabled");
  is(bp2.disabled, false, "second breakpoint is enabled");

  // Remove the breakpoints
  yield removeBreakpoint(dbg, 1);
  yield removeBreakpoint(dbg, 1);
  const bps = findBreakpoints(dbg);
  is(bps.size, 0, "breakpoints are removed");
});
