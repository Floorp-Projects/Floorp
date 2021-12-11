/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const SOURCES = [
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
    selectors: { getSelectedSource, getIsPaused, getCurrentThread },
    getState
  } = dbg;
 
  info("Pause in the first document");
  invokeInTab("firstCall");
  await waitForPaused(dbg);

  info("Navigate while being paused in the first document");
  await navigate(dbg, "doc-scripts.html", "simple1.js");

  info("Set a breakpoint on the second document and pause on it");
  await selectSource(dbg, "simple1");
  await addBreakpoint(dbg, "simple1.js", 4);
  invokeInTab("main");
  await waitForPaused(dbg);
  let source = findSource(dbg, "simple1.js");
  assertPausedAtSourceAndLine(dbg, source.id, 4);
  is(countSources(dbg), 5, "5 sources are loaded.");

  await waitForRequestsToSettle(dbg);
  let onBreakpoint = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  await navigate(dbg, "doc-scripts.html", ...SOURCES);
  await onBreakpoint
  is(countSources(dbg), 5, "5 sources are loaded.");
  ok(!getIsPaused(getCurrentThread()), "Is not paused");

  await waitForRequestsToSettle(dbg);
  onBreakpoint = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  await navigate(dbg, "doc-scripts.html", ...SOURCES);
  await onBreakpoint
  is(countSources(dbg), 5, "5 sources are loaded.");

  info("Test that the current selected source persists across reloads");
  await selectSource(dbg, "long.js");

  await waitForRequestsToSettle(dbg);
  onBreakpoint = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  await reload(dbg, "long.js");
  await onBreakpoint
  await waitForSelectedSource(dbg, "long.js");

  ok(getSelectedSource().url.includes("long.js"), "Selected source is long.js");
});

function countSources(dbg) {
  return dbg.selectors.getSourceCount();
}
