/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function countSources(dbg) {
  return dbg.selectors.getSourceCount(dbg.getState());
}

const sources = [
  "simple1.js",
  "simple2.js",
  "simple3.js",
  "long.js",
  "scripts.html"
];

/**
 * Test navigating
 * navigating while paused will reset the pause state and sources
 */
add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html");
  const {
    selectors: { getSelectedSource, isPaused },
    getState
  } = dbg;

  invokeInTab("firstCall");
  await waitForPaused(dbg);

  await navigate(dbg, "doc-scripts.html", "simple1.js");
  await addBreakpoint(dbg, "simple1.js", 4);
  invokeInTab("main");
  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "simple1");
  toggleScopes(dbg);

  assertPausedLocation(dbg);
  is(countSources(dbg), 5, "5 sources are loaded.");

  await navigate(dbg, "doc-scripts.html", ...sources);
  is(countSources(dbg), 5, "5 sources are loaded.");
  ok(!isPaused(getState()), "Is not paused");

  await navigate(dbg, "doc-scripts.html", ...sources);
  is(countSources(dbg), 5, "5 sources are loaded.");

  // Test that the current select source persists across reloads
  await selectSource(dbg, "long.js");
  await reload(dbg, "long.js");
  await waitForSelectedSource(dbg, "long.js");

  ok(
    getSelectedSource(getState()).url.includes("long.js"),
    "Selected source is long.js"
  );
});
