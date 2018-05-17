/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function toggleBreakpoint(dbg, index) {
  const bp = findAllElements(dbg, "breakpointItems")[index];
  const input = bp.querySelector("input");
  input.click();
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
  const toggled = waitForDispatch(dbg, "DISABLE_BREAKPOINT", count);
  toggleBreakpoints(dbg);
  return toggled;
}

function enableBreakpoints(dbg, count) {
  const enabled = waitForDispatch(dbg, "ENABLE_BREAKPOINT", count);
  toggleBreakpoints(dbg);
  return enabled;
}

function findBreakpoint(dbg, url, line) {
  const {
    selectors: { getBreakpoint },
    getState
  } = dbg;
  const source = findSource(dbg, url);
  return getBreakpoint(getState(), { sourceId: source.id, line });
}

function findBreakpoints(dbg) {
  const {
    selectors: { getBreakpoints },
    getState
  } = dbg;
  return getBreakpoints(getState());
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");

  // Create two breakpoints
  await selectSource(dbg, "simple2");
  await addBreakpoint(dbg, "simple2", 3);
  await addBreakpoint(dbg, "simple2", 5);

  // Disable the first one
  await disableBreakpoint(dbg, 0);
  let bp1 = findBreakpoint(dbg, "simple2", 3);
  let bp2 = findBreakpoint(dbg, "simple2", 5);
  is(bp1.disabled, true, "first breakpoint is disabled");
  is(bp2.disabled, false, "second breakpoint is enabled");

  // Disable and Re-Enable the second one
  await disableBreakpoint(dbg, 1);
  await enableBreakpoint(dbg, 1);
  bp2 = findBreakpoint(dbg, "simple2", 5);
  is(bp2.disabled, false, "second breakpoint is enabled");
});
