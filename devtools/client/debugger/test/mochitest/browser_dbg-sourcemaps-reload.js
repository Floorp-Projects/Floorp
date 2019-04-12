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

function getBreakpoints(dbg) {
  return dbg.selectors.getBreakpointsList();
}

function getBreakpointCount(dbg) {
  return dbg.selectors.getBreakpointCount();
}

add_task(async function() {
  const dbg = await initDebugger("doc-minified.html");

  await navigate(dbg, "sourcemaps-reload/doc-sourcemaps-reload.html", "v1.js");

  info("Add initial breakpoint");
  await selectSource(dbg, "v1.js");
  await addBreakpoint(dbg, "v1.js", 6);

  let breakpoint = getBreakpoints(dbg)[0];
  is(breakpoint.location.line, 6);

  info("Reload with a new version of the file");
  const syncBp = waitForDispatch(dbg, "SET_BREAKPOINT");
  await navigate(dbg, "doc-sourcemaps-reload2.html", "v1.js");

  await syncBp;
  breakpoint = getBreakpoints(dbg)[0];

  is(breakpoint.location.line, 9);
  is(breakpoint.generatedLocation.line, 79);

  info("Add a second breakpoint");
  await addBreakpoint(dbg, "v1.js", 13);
  is(getBreakpointCount(dbg), 2, "No breakpoints");

  // NOTE: When we reload, the `foo` function and the
  // module is no longer 13 lines long
  info("Reload and observe no breakpoints");
  await navigate(dbg, "doc-sourcemaps-reload3.html", "v1.js");
  await waitForSource(dbg, "v1");

  // There will initially be zero breakpoints, but wait to make sure none are
  // installed while syncing.
  await waitForTime(1000);

  is(getBreakpointCount(dbg), 0, "No breakpoints");
});
