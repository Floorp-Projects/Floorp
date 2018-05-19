/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test reloading:
 * 1. reload the source
 * 2. re-sync breakpoints
 */

async function waitForBreakpoint(dbg, location) {
  return waitForState(
    dbg,
    state => {
      return dbg.selectors.getBreakpoint(dbg.getState(), location);
    },
    "Waiting for breakpoint"
  );
}

function getBreakpoints(dbg) {
  const breakpoints = dbg.selectors.getBreakpoints(dbg.getState());
  return breakpoints.valueSeq().toJS();
}

add_task(async function() {
  const dbg = await initDebugger("doc-minified.html");

  dump(`>> meh`);

  await navigate(dbg, "sourcemaps-reload/doc-sourcemaps-reload.html", "v1");

  dump(`>> select v1`);
  await selectSource(dbg, "v1");
  await addBreakpoint(dbg, "v1", 6);
  let breakpoint = getBreakpoints(dbg)[0];
  is(breakpoint.location.line, 6);

  let syncBp = waitForDispatch(dbg, "SYNC_BREAKPOINT");
  await reload(dbg);

  await waitForPaused(dbg);
  await syncBp;
  assertDebugLine(dbg, 72);
  breakpoint = getBreakpoints(dbg)[0];

  is(breakpoint.location.line, 9);
  is(breakpoint.generatedLocation.line, 73);

  await resume(dbg);
  syncBp = waitForDispatch(dbg, "SYNC_BREAKPOINT", 2);
  await selectSource(dbg, "v1");
  await addBreakpoint(dbg, "v1", 13);

  await reload(dbg);
  await waitForSource(dbg, "v1");
  await syncBp;

  is(getBreakpoints(dbg).length, 0, "No breakpoints");
});
