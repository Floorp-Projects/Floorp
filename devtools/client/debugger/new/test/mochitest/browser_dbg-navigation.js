/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function countSources(dbg) {
  const sources = dbg.selectors.getSources(dbg.getState());
  return sources.size;
}

/**
 * Test navigating
 * navigating while paused will reset the pause state and sources
 */
add_task(function* () {
  const dbg = yield initDebugger(
    "doc-script-switching.html",
    "script-switching-01.js"
  );

  invokeInTab("firstCall");
  yield waitForPaused(dbg);

  yield navigate(dbg, "doc-scripts.html", "simple1.js", "long.js");
  yield addBreakpoint(dbg, "simple1.js", 4);
  invokeInTab("main");
  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "simple1.js", 4);
  is(countSources(dbg), 4, "4 sources are loaded.");

  yield navigate(dbg, "about:blank");
  yield waitForDispatch(dbg, "NAVIGATE");
  is(countSources(dbg), 0, "0 sources are loaded.");

  yield navigate(dbg,
    "doc-scripts.html",
    "simple1.js",
    "simple2.js",
    "long.js",
    "scripts.html"
  );

  is(countSources(dbg), 4, "4 sources are loaded.");
});
