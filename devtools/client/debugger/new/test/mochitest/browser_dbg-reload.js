/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test reloading:
 * 1. reload the source
 * 2. re-sync breakpoints
 */

add_task(async function() {
  const dbg = await initDebugger("reload/doc_reload.html", "sjs_code_reload");

  const sym = waitForDispatch(dbg, "SET_SYMBOLS");
  await selectSource(dbg, "sjs_code_reload");
  await sym;

  await addBreakpoint(dbg, "sjs_code_reload", 2);

  const sync = waitForDispatch(dbg, "SYNC_BREAKPOINT");
  await reload(dbg,  "sjs_code_reload");
  await sync;

  const breakpoints = dbg.selectors.getBreakpoints(dbg.getState());
  const breakpointList = breakpoints.valueSeq().toJS();
  const breakpoint = breakpointList[0];

  is(breakpointList.length, 1);
  is(breakpoint.location.line, 6);
});
