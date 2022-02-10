/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Assert hitting breakpoints when reloading a page using source maps,
 * and whose all file content changes (source map, original and generated).
 */

const testServer = createVersionizedHttpTestServer("sourcemaps-reload");
const TEST_URL = testServer.urlFor("index.html");

add_task(async function() {
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URL, "original.js");

  info("Add initial breakpoint");
  await selectSource(dbg, "original.js");
  await addBreakpoint(dbg, "original.js", 6);

  let breakpoint = getBreakpoints(dbg)[0];
  is(breakpoint.location.line, 6);
  is(breakpoint.generatedLocation.line, 82);
  is(getBreakpointCount(dbg), 1, "Only one breakpoint exists");

  info("Reload with a new version of the file");
  const syncBp = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  testServer.switchToNextVersion();
  await reload(dbg);
  await syncBp;
  breakpoint = getBreakpoints(dbg)[0];

  is(breakpoint.location.line, 9);
  is(breakpoint.generatedLocation.line, 82);

  info("Add a second breakpoint");
  await addBreakpoint(dbg, "original.js", 13);
  is(getBreakpointCount(dbg), 2, "Two breakpoints exist");

  // NOTE: When we reload, the `foo` function and the
  // module is no longer 13 lines long
  info("Reload and observe no breakpoints");
  testServer.switchToNextVersion();
  await reload(dbg);
  await waitForSource(dbg, "original.js");

  // There will initially be zero breakpoints, but wait to make sure none are
  // installed while syncing.
  await waitForTime(1000);

  is(getBreakpointCount(dbg), 0, "No breakpoints");
});

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
