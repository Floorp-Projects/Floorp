function toggleBreakpoint(dbg, index) {
  const bp = findElement(dbg, "breakpointItem", index);
  const input = bp.querySelector("input");
  input.click();
}

async function removeBreakpoint(dbg, index) {
  const removed = waitForDispatch(dbg, "REMOVE_BREAKPOINT");
  const bp = findElement(dbg, "breakpointItem", index);
  bp.querySelector(".close-btn").click();
  await removed;
}

async function disableBreakpoint(dbg, index) {
  const disabled = waitForDispatch(dbg, "DISABLE_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await disabled;
}

async function enableBreakpoint(dbg, index) {
  const enabled = waitForDispatch(dbg, "ENABLE_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await enabled;
}

function toggleBreakpoints(dbg, count) {
  clickElement(dbg, "toggleBreakpoints");
}

function disableBreakpoints(dbg, count) {
  const toggled = waitForDispatch(dbg, "DISABLE_ALL_BREAKPOINTS", count);
  toggleBreakpoints(dbg);
  return toggled;
}

function enableBreakpoints(dbg, count) {
  const enabled = waitForDispatch(dbg, "ENABLE_ALL_BREAKPOINTS", count);
  toggleBreakpoints(dbg);
  return enabled;
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

// toggle all breakpoints
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple2");

  // Create two breakpoints
  await selectSource(dbg, "simple2");
  await addBreakpoint(dbg, "simple2", 3);
  await addBreakpoint(dbg, "simple2", 5);

  // Disable all of the breakpoints
  await disableBreakpoints(dbg, 1);
  let bp1 = findBreakpoint(dbg, "simple2", 3);
  let bp2 = findBreakpoint(dbg, "simple2", 5);

  if (!bp2) {
    debugger;
  }

  is(bp1.disabled, true, "first breakpoint is disabled");
  is(bp2.disabled, true, "second breakpoint is disabled");

  // Enable all of the breakpoints
  await enableBreakpoints(dbg, 1);
  bp1 = findBreakpoint(dbg, "simple2", 3);
  bp2 = findBreakpoint(dbg, "simple2", 5);

  is(bp1.disabled, false, "first breakpoint is enabled");
  is(bp2.disabled, false, "second breakpoint is enabled");

  // Remove the breakpoints
  await removeBreakpoint(dbg, 1);
  await removeBreakpoint(dbg, 1);

  const bps = findBreakpoints(dbg);

  is(bps.size, 0, "breakpoints are removed");
});
