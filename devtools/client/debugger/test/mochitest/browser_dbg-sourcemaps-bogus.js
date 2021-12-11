/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that an error while loading a sourcemap does not break
// debugging.
requestLongerTimeout(2);

add_task(async function() {
  // NOTE: the CORS call makes the test run times inconsistent
  const dbg = await initDebugger("doc-sourcemap-bogus.html", "bogus-map.js");

  await selectSource(dbg, "bogus-map.js");

  // We should still be able to set breakpoints and pause in the
  // generated source.
  await addBreakpoint(dbg, "bogus-map.js", 4);
  invokeInTab("runCode");
  await waitForPaused(dbg);
  assertPausedLocation(dbg);

  // Make sure that only the single generated source exists. The
  // sourcemap failed to download.
  is(dbg.selectors.getSourceCount(), 1, "Only 1 source exists");
});
