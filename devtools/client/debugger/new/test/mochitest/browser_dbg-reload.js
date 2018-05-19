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

add_task(async function() {
  const dbg = await initDebugger("reload/doc-reload.html");

  await waitForSource(dbg, "sjs_code_reload");
  await selectSource(dbg, "sjs_code_reload");
  await addBreakpoint(dbg, "sjs_code_reload", 2);

  await reload(dbg, "sjs_code_reload.sjs");
  await waitForSelectedSource(dbg, "sjs_code_reload.sjs");

  const source = findSource(dbg, "sjs_code_reload");
  const location = { sourceId: source.id, line: 6 };

  await waitForBreakpoint(dbg, location);

  const breakpoints = dbg.selectors.getBreakpoints(dbg.getState());
  const breakpointList = breakpoints.valueSeq().toJS();
  const breakpoint = breakpointList[0];

  is(breakpointList.length, 1);
  is(breakpoint.location.line, 6);
});
