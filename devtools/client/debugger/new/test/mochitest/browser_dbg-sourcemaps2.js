/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


function assertBpInGutter(dbg, lineNumber) {
  const el = findElement(dbg, "breakpoint");
  const bpLineNumber = +el.querySelector(".CodeMirror-linenumber").innerText;
  is(bpLineNumber, lineNumber);
}

// Tests loading sourcemapped sources, setting breakpoints, and
// stepping in them.

// This source map does not have source contents, so it's fetched separately
add_task(async function() {
  // NOTE: the CORS call makes the test run times inconsistent
  requestLongerTimeout(2);

  const dbg = await initDebugger("doc-sourcemaps2.html");
  const { selectors: { getBreakpoint, getBreakpoints }, getState } = dbg;

  await waitForSources(dbg, "main.js", "main.min.js");

  ok(true, "Original sources exist");
  const mainSrc = findSource(dbg, "main.js");

  await selectSource(dbg, mainSrc);

  // Test that breakpoint is not off by a line.
  await addBreakpoint(dbg, mainSrc, 4);
  is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
  ok(
    getBreakpoint(getState(), { sourceId: mainSrc.id, line: 4, column: 2 }),
    "Breakpoint has correct line"
  );

  assertBpInGutter(dbg, 4);
  invokeInTab("logMessage");

  await waitForPaused(dbg);
  assertPausedLocation(dbg);
});
