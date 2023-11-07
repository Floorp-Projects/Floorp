/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

requestLongerTimeout(2);

add_task(async function () {
  // NOTE: the CORS call makes the test run times inconsistent
  const dbg = await initDebugger("doc-sourcemaps.html");
  const {
    selectors: { getBreakpoint, getBreakpointCount },
  } = dbg;

  await waitForSources(dbg, "entry.js", "output.js", "times2.js", "opts.js");
  ok(true, "Original sources exist");
  const entrySrc = findSource(dbg, "entry.js");

  await selectSource(dbg, entrySrc);
  ok(
    getCM(dbg).getValue().includes("window.keepMeAlive"),
    "Original source text loaded correctly"
  );

  await addBreakpoint(dbg, entrySrc, 5);
  await addBreakpoint(dbg, entrySrc, 15, 1);
  await disableBreakpoint(dbg, entrySrc, 15, 1);

  // Test reloading the debugger
  const onReloaded = reload(dbg, "opts.js");
  await waitForDispatch(dbg.store, "LOAD_ORIGINAL_SOURCE_TEXT");

  await waitForPausedInOriginalFileAndToggleMapScopes(dbg, "entry.js");
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "entry.js").id, 5);

  await waitForBreakpointCount(dbg, 2);
  is(getBreakpointCount(), 2, "Two breakpoints exist");

  const bp = getBreakpoint(
    createLocation({
      source: entrySrc,
      line: 15,
      column: 0,
    })
  );
  ok(bp, "Breakpoint is on the correct line");
  ok(bp.disabled, "Breakpoint is disabled");
  await assertBreakpoint(dbg, 15);

  await resume(dbg);
  info("Wait for reload to complete after resume");
  await onReloaded;
});

async function waitForBreakpointCount(dbg, count) {
  return waitForState(
    dbg,
    state => dbg.selectors.getBreakpointCount() === count
  );
}
