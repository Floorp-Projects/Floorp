/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Test reloading:
 * 1. reload the source
 * 2. re-sync breakpoints
 */

async function waitForBreakpoint(dbg, location) {
  return waitForState(
    dbg,
    state => {
      return dbg.selectors.getBreakpoint(location);
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
  const location = { sourceId: source.id, line: 6, column: 2 };

  await waitForBreakpoint(dbg, location);

  const breakpointList = dbg.selectors.getBreakpointsList();
  const breakpoint = breakpointList[0];

  is(breakpointList.length, 1);
  is(breakpoint.location.line, 6);
});
